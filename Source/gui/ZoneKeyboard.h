#pragma once
// The on-screen keyboard with coloured zones (Kontakt-style): keys the sampler has
// recordings on get a gold strip, and when Chords is on "In key", the notes of the
// chosen key / scale get a purple dot (the key's root note a bigger one).
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace ff::gui
{
class ZoneKeyboard : public juce::MidiKeyboardComponent
{
public:
    ZoneKeyboard (FourfoldProcessor& p, juce::MidiKeyboardState& s)
        : juce::MidiKeyboardComponent (s, horizontalKeyboard), proc (p)
    {
        refresher.owner = this;
        refresher.startTimerHz (4);
    }
    ~ZoneKeyboard() override { refresher.stopTimer(); }

    static constexpr juce::uint32 chordColour = 0xffb98cff;
    void paint (juce::Graphics& g) override
    {
        if (! refreshedOnce) { refreshedOnce = true; refresh(); }
        juce::MidiKeyboardComponent::paint (g);
    }

private:
    struct Refresher : juce::Timer { ZoneKeyboard* owner = nullptr; void timerCallback() override { owner->refresh(); } } refresher;
    void refresh()
    {
        std::array<bool, 128> smp {};
        std::array<int, 128> scale {};
        const bool samplerOn = proc.apvts.getRawParameterValue ("smp_play")->load() > 0.5f;
        if (samplerOn && proc.sampler != nullptr)
            if (auto inst = proc.sampler->getDisplayInstrument (0))
                if (auto* art = inst->get (proc.sampler->getArticulation (0)))
                    for (auto& z : art->zones)
                        for (int n = juce::jmax (0, z.loKey); n <= juce::jmin (127, z.hiKey); ++n) smp[(size_t) n] = true;
        const bool chords = proc.apvts.getRawParameterValue ("ch_on")->load() > 0.5f
                         && proc.apvts.getRawParameterValue ("ch_follow")->load() < 0.5f;
        if (chords)
        {
            static const int steps[9][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
                                             { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 3, 5, 6, 8, 10 }, { 0, 1, 4, 5, 7, 8, 10 } };
            const int key = (int) proc.apvts.getRawParameterValue ("ch_key")->load();
            const int sc = juce::jlimit (0, 8, (int) proc.apvts.getRawParameterValue ("ch_scale")->load());
            for (int n = 0; n < 128; ++n)
                for (int d = 0; d < 7; ++d)
                    if ((n - key - steps[sc][d] + 1200) % 12 == 0) scale[(size_t) n] = d == 0 ? 2 : 1;
        }
        if (smp != samplerKeys || scale != scaleKeys) { samplerKeys = smp; scaleKeys = scale; if (refreshedOnce) repaint(); }
    }

    void decorate (int note, juce::Graphics& g, juce::Rectangle<float> area, bool black)
    {
        if (samplerKeys[(size_t) note])
        {
            g.setColour (col::sampler.withAlpha (black ? 0.9f : 0.75f));
            g.fillRect (area.removeFromBottom (black ? 4.0f : 6.0f));
        }
        if (const int s = scaleKeys[(size_t) note]; s > 0)
        {
            const float d = s == 2 ? 8.0f : 5.0f;
            g.setColour (juce::Colour (chordColour));
            g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ area.getCentreX(), area.getBottom() - (black ? 10.0f : 16.0f) }));
        }
    }

    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour line, juce::Colour text) override
    {
        juce::MidiKeyboardComponent::drawWhiteNote (note, g, area, down, over, line, text);
        decorate (note, g, area, false);
    }
    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool down, bool over, juce::Colour fill) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (note, g, area, down, over, fill);
        decorate (note, g, area, true);
    }

    FourfoldProcessor& proc;
    bool refreshedOnce = false;
    std::array<bool, 128> samplerKeys {};
    std::array<int, 128> scaleKeys {};
};
} // namespace ff::gui
