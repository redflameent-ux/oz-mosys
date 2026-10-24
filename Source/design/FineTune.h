#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

// =====================================================================================
//  Fine Tuning (as in Absynth's browser): eight broad strokes over the whole sound.
//  Each knob is -1..+1 and works relative to the sound as it was loaded, so turning a
//  knob back to the centre gives you the original again.
// =====================================================================================
namespace ff::design
{
class FineTune
{
public:
    static constexpr int numKnobs = 8;
    static const juce::StringArray& names()
    {
        static const juce::StringArray n { "Volume", "Brightness", "Bass", "Resonance", "Distortion", "FX", "Mod Amount", "Mod Time" };
        return n;
    }

    explicit FineTune (juce::AudioProcessorValueTreeState& s) : state (s) {}

    float knob[numKnobs] {};

    void captureBase()
    {
        base.clear();
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                base[rp->getParameterID()] = rp->convertFrom0to1 (rp->getValue());
        for (auto& k : knob) k = 0.0f;
    }

    void apply()
    {
        if (base.empty()) captureBase();
        auto b = [this] (const juce::String& id) { auto it = base.find (id); return it == base.end() ? 0.0f : it->second; };
        auto set = [this] (const juce::String& id, float real)
        {
            if (auto* p = state.getParameter (id))
            {
                const float v = p->convertTo0to1 (juce::jlimit (p->getNormalisableRange().start, p->getNormalisableRange().end, real));
                if (std::abs (p->getValue() - v) > 1.0e-5f) p->setValueNotifyingHost (v);
            }
        };
        const float vol = knob[0], bright = knob[1], bass = knob[2], res = knob[3], dist = knob[4], fx = knob[5], modA = knob[6], modT = knob[7];

        set ("master", b ("master") * (1.0f + 0.45f * vol));
        set ("flt_cutoff", b ("flt_cutoff") + 0.25f * bright);
        set ("eq_treble", b ("eq_treble") + 9.0f * bright);
        set ("res_bright", b ("res_bright") + 0.3f * bright);
        set ("eq_bass", b ("eq_bass") + 10.0f * bass);
        set ("flt_res", b ("flt_res") + 0.4f * res);
        set ("fx_drive", dist >= 0 ? b ("fx_drive") + (1.0f - b ("fx_drive")) * 0.7f * dist : b ("fx_drive") * (1.0f + dist));
        for (const char* id : { "rev_mix", "dly_mix", "cho_mix" })
        {
            const float v = b (id);
            set (id, fx >= 0 ? v + (juce::jmax (v, 0.25f) - v + (1.0f - v) * 0.4f) * fx : v * (1.0f + fx));
        }
        for (int i = 1; i <= 10; ++i) set ("mod" + juce::String (i) + "_amt", b ("mod" + juce::String (i) + "_amt") * (1.0f + modA));
        const float timeScale = std::exp2 (2.0f * modT);
        for (const char* id : { "amp_a", "amp_d", "amp_r", "e2_a", "e2_d", "e2_r", "e3_a", "e3_d", "e3_r" }) set (id, b (id) * timeScale);
        for (const char* id : { "lfo1_rate", "lfo2_rate" }) set (id, b (id) / timeScale);
    }

private:
    juce::AudioProcessorValueTreeState& state;
    std::map<juce::String, float> base;
};
} // namespace ff::design
