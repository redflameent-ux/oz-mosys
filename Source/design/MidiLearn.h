#pragma once
// MIDI learn: link a hardware knob/fader (MIDI CC) to any control.
// The mapping belongs to your setup, not to a sound, so it's kept in the VIBRA-VII settings
// file and stays the same across presets and projects.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

namespace ff::design
{
class MidiLearn : private juce::Timer
{
public:
    explicit MidiLearn (juce::AudioProcessor& p) : proc (p)
    {
        for (auto* prm : proc.getParameters())
            params.push_back (dynamic_cast<juce::RangedAudioParameter*> (prm));
        for (auto& m : map) m.store (-1);
        for (auto& d : dirty) d.store (false);
        load();
        startTimerHz (60);
    }
    ~MidiLearn() override { stopTimer(); }

    // ---- audio thread ----
    void processMidi (const juce::MidiBuffer& midi) noexcept
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (! m.isController()) continue;
            const int cc = m.getControllerNumber();
            if (cc == 6 || cc == 38 || (cc >= 98 && cc <= 101) || cc >= 120) continue;   // RPN/NRPN and channel-mode messages
            if (m.getChannel() > 1 && cc == 74) continue;                              // MPE slide lives on member channels
            const int want = learnIndex.load (std::memory_order_relaxed);
            if (want >= 0)
            {
                learnedCC.store (cc);
                learnIndex.store (-1);
            }
            if (map[(size_t) cc].load (std::memory_order_relaxed) >= 0)
            {
                pending[(size_t) cc].store ((float) m.getControllerValue() / 127.0f, std::memory_order_relaxed);
                dirty[(size_t) cc].store (true, std::memory_order_release);
            }
        }
    }

    // ---- message thread ----
    void learn (const juce::String& paramId)
    {
        const int idx = indexOf (paramId);
        learnTarget = idx;
        learnIndex.store (idx);
        if (onChange) onChange();
    }
    void cancelLearn() { learnIndex.store (-1); learnTarget = -1; if (onChange) onChange(); }
    bool isLearning (const juce::String& paramId) const { return learnTarget >= 0 && learnTarget == indexOf (paramId); }
    bool isLearningAnything() const { return learnTarget >= 0; }
    juce::String learningParamId() const
    {
        return learnTarget >= 0 && learnTarget < (int) params.size() && params[(size_t) learnTarget] != nullptr
                 ? params[(size_t) learnTarget]->getParameterID() : juce::String();
    }

    int ccFor (const juce::String& paramId) const
    {
        const int idx = indexOf (paramId);
        if (idx < 0) return -1;
        for (int cc = 0; cc < 128; ++cc) if (map[(size_t) cc].load() == idx) return cc;
        return -1;
    }
    void forget (const juce::String& paramId)
    {
        const int idx = indexOf (paramId);
        for (auto& m : map) if (m.load() == idx) m.store (-1);
        save();
        if (onChange) onChange();
    }
    void forgetCC (int cc) { if (juce::isPositiveAndBelow (cc, 128)) { map[(size_t) cc].store (-1); save(); if (onChange) onChange(); } }
    void clearAll() { for (auto& m : map) m.store (-1); save(); if (onChange) onChange(); }

    struct Assignment { int cc; juce::String paramId, name; };
    std::vector<Assignment> assignments() const
    {
        std::vector<Assignment> a;
        for (int cc = 0; cc < 128; ++cc)
        {
            const int idx = map[(size_t) cc].load();
            if (idx >= 0 && idx < (int) params.size() && params[(size_t) idx] != nullptr)
                a.push_back ({ cc, params[(size_t) idx]->getParameterID(), params[(size_t) idx]->getName (64) });
        }
        return a;
    }

    std::function<void()> onChange;

    static juce::String ccName (int cc)
    {
        switch (cc)
        {
            case 1: return "CC1 Mod Wheel";
            case 2: return "CC2 Breath";
            case 7: return "CC7 Volume";
            case 10: return "CC10 Pan";
            case 11: return "CC11 Expression";
            case 64: return "CC64 Sustain";
            case 74: return "CC74 Brightness";
            default: return "CC" + juce::String (cc);
        }
    }

private:
    int indexOf (const juce::String& id) const
    {
        for (size_t i = 0; i < params.size(); ++i)
            if (params[i] != nullptr && params[i]->getParameterID() == id) return (int) i;
        return -1;
    }

    void timerCallback() override
    {
        // a learn finished on the audio thread
        const int cc = learnedCC.exchange (-1);
        if (cc >= 0 && learnTarget >= 0)
        {
            for (auto& m : map) if (m.load() == learnTarget) m.store (-1);   // one CC per control
            map[(size_t) cc].store (learnTarget);
            learnTarget = -1;
            save();
            if (onChange) onChange();
        }
        // move knobs that hardware turned
        for (int i = 0; i < 128; ++i)
            if (dirty[(size_t) i].exchange (false, std::memory_order_acquire))
            {
                const int idx = map[(size_t) i].load();
                if (idx < 0 || idx >= (int) params.size() || params[(size_t) idx] == nullptr) continue;
                auto* p = params[(size_t) idx];
                const float v = pending[(size_t) i].load();
                if (std::abs (p->getValue() - v) > 1.0e-4f)
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (v);
                    p->endChangeGesture();
                }
            }
    }

    static std::unique_ptr<juce::PropertiesFile> settings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "VIBRA-VII"; o.folderName = "VIBRA-VII"; o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (o);
    }
    void save()
    {
        juce::StringArray parts;
        for (auto& a : assignments()) parts.add (juce::String (a.cc) + "=" + a.paramId);
        auto f = settings();
        f->setValue ("midiMap", parts.joinIntoString (";"));
        f->saveIfNeeded();
    }
    void load()
    {
        auto f = settings();
        for (auto& part : juce::StringArray::fromTokens (f->getValue ("midiMap"), ";", ""))
        {
            const int cc = part.upToFirstOccurrenceOf ("=", false, false).getIntValue();
            const int idx = indexOf (part.fromFirstOccurrenceOf ("=", false, false));
            if (juce::isPositiveAndBelow (cc, 128) && idx >= 0) map[(size_t) cc].store (idx);
        }
    }

    juce::AudioProcessor& proc;
    std::vector<juce::RangedAudioParameter*> params;
    std::array<std::atomic<int>, 128> map;
    std::array<std::atomic<float>, 128> pending {};
    std::array<std::atomic<bool>, 128> dirty;
    std::atomic<int> learnIndex { -1 }, learnedCC { -1 };
    int learnTarget = -1;
};
} // namespace ff::design
