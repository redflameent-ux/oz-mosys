#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

namespace ff
{
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // real-world values (not 0..1)
    struct Route { int src, dst; float amt; };
    std::vector<Route> routes;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", {}, {} },

        { "Ancient Halo",   // wavetable formants + granular choir, slow and wide
          { { "wt_table", 2 }, { "wt_pos", 0.2f }, { "wt_unison", 5 }, { "wt_detune", 0.3f }, { "wt_level", 0.55f },
            { "gr_on", 1 }, { "gr_pos", 0.35f }, { "gr_size", 220 }, { "gr_density", 35 }, { "gr_spray", 0.25f },
            { "gr_stereo", 1.0f }, { "gr_scan", 0.15f }, { "gr_level", 0.6f },
            { "flt_type", 1 }, { "flt_cutoff", 0.72f }, { "flt_res", 0.2f },
            { "amp_a", 1.6f }, { "amp_d", 1.0f }, { "amp_s", 0.9f }, { "amp_r", 3.5f },
            { "lfo1_rate", 0.12f }, { "cho_mix", 0.4f }, { "rev_mix", 0.45f }, { "rev_size", 0.9f } },
          { { srcLfo1, dstWtPos, 0.35f }, { srcMacro1, dstCutoff, 0.3f }, { srcModWheel, dstGrSpray, 0.5f } } },

        { "Sub Reese",
          { { "wt_table", 0 }, { "wt_pos", 0.66f }, { "wt_unison", 5 }, { "wt_detune", 0.35f }, { "wt_spread", 0.4f },
            { "an_on", 1 }, { "an_shape", 0.0f }, { "an_oct", -1 }, { "an_sub", 0.8f }, { "an_level", 0.6f },
            { "flt_cutoff", 0.42f }, { "flt_res", 0.25f }, { "flt_drive", 0.35f }, { "flt_env", 0.2f },
            { "e2_d", 0.6f }, { "e2_s", 0.2f }, { "glide", 0.06f }, { "fx_drive", 0.25f }, { "rev_mix", 0.05f } },
          { { srcLfo2, dstWtDetune, 0.15f }, { srcMacro1, dstCutoff, 0.4f } } },

        { "Velvet Keys",
          { { "wt_on", 0 }, { "an_on", 1 }, { "an_shape", 0.45f }, { "an_drift", 0.4f }, { "an_level", 0.8f },
            { "flt_type", 0 }, { "flt_cutoff", 0.45f }, { "flt_res", 0.15f }, { "flt_env", 0.35f }, { "flt_key", 0.5f },
            { "amp_a", 0.003f }, { "amp_d", 1.8f }, { "amp_s", 0.35f }, { "amp_r", 0.5f },
            { "e2_d", 0.9f }, { "e2_s", 0.1f }, { "amp_vel", 0.8f }, { "cho_mix", 0.55f }, { "rev_mix", 0.22f } },
          { { srcVelocity, dstCutoff, 0.25f }, { srcLfo2, dstAnPw, 0.15f } } },

        { "Frost Drone",
          { { "wt_on", 0 }, { "gr_on", 1 }, { "gr_pos", 0.6f }, { "gr_size", 380 }, { "gr_density", 45 },
            { "gr_spray", 0.35f }, { "gr_stereo", 1.0f }, { "gr_pitchrand", 0.12f }, { "gr_scan", 0.08f },
            { "gr_reverse", 0.3f }, { "gr_level", 0.9f }, { "flt_type", 1 }, { "flt_cutoff", 0.7f },
            { "amp_a", 2.5f }, { "amp_s", 1.0f }, { "amp_r", 5.0f }, { "lfo1_rate", 0.07f }, { "lfo1_shape", 5 },
            { "master", 0.9f }, { "dly_mix", 0.2f }, { "dly_time", 600 }, { "dly_fb", 0.5f }, { "rev_mix", 0.6f }, { "rev_size", 0.95f } },
          { { srcLfo1, dstGrPos, 0.2f }, { srcLfo1, dstCutoff, 0.1f }, { srcMacro1, dstGrSize, 0.5f } } },

        { "Neon Pluck",
          { { "wt_table", 3 }, { "wt_pos", 0.35f }, { "wt_unison", 3 }, { "wt_detune", 0.15f },
            { "flt_cutoff", 0.3f }, { "flt_res", 0.35f }, { "flt_env", 0.5f },
            { "e2_a", 0.0f }, { "e2_d", 0.18f }, { "e2_s", 0.0f }, { "e2_r", 0.2f },
            { "amp_a", 0.001f }, { "amp_d", 0.4f }, { "amp_s", 0.0f }, { "amp_r", 0.3f },
            { "master", 0.9f }, { "dly_mix", 0.3f }, { "dly_time", 375 }, { "dly_fb", 0.45f }, { "rev_mix", 0.2f } },
          { { srcEnv2, dstWtPos, 0.3f }, { srcVelocity, dstCutoff, 0.2f } } },

        { "Brass Ember",
          { { "wt_table", 4 }, { "wt_pos", 0.2f }, { "wt_unison", 3 }, { "wt_detune", 0.12f }, { "wt_level", 0.5f },
            { "an_on", 1 }, { "an_shape", 1.0f }, { "an_drift", 0.5f }, { "an_level", 0.6f },
            { "flt_cutoff", 0.35f }, { "flt_res", 0.1f }, { "flt_env", 0.45f },
            { "e2_a", 0.12f }, { "e2_d", 0.8f }, { "e2_s", 0.45f },
            { "amp_a", 0.05f }, { "amp_s", 0.9f }, { "amp_r", 0.3f }, { "lfo2_rate", 5.5f }, { "rev_mix", 0.2f } },
          { { srcLfo2, dstPitchAll, 0.006f }, { srcLfo1, dstWtPos, 0.2f } } },

        { "Glass Motion",
          { { "wt_table", 1 }, { "wt_pos", 0.4f }, { "wt_unison", 4 }, { "wt_detune", 0.2f },
            { "flt_type", 1 }, { "flt_cutoff", 0.8f }, { "lfo1_shape", 4 }, { "lfo1_rate", 4.0f },
            { "amp_a", 0.01f }, { "amp_s", 0.7f }, { "amp_r", 0.8f },
            { "master", 0.95f }, { "dly_mix", 0.35f }, { "dly_time", 250 }, { "dly_fb", 0.5f }, { "rev_mix", 0.35f } },
          { { srcLfo1, dstWtPos, 0.4f }, { srcRandom, dstPan, 0.4f } } },

        { "Macro Morph",
          { { "wt_table", 0 }, { "wt_pos", 0.0f }, { "an_on", 1 }, { "an_shape", 2.0f }, { "an_level", 0.0f },
            { "gr_on", 1 }, { "gr_level", 0.0f }, { "flt_cutoff", 0.35f }, { "rev_mix", 0.25f } },
          { { srcMacro1, dstWtPos, 1.0f }, { srcMacro1, dstCutoff, 0.5f }, { srcMacro2, dstAnLevel, 0.8f },
            { srcMacro3, dstGrLevel, 0.8f }, { srcMacro3, dstGrSpray, 0.6f }, { srcMacro4, dstWtWarp, 1.0f } } },
    };
    return presets;
}

inline void applyFactoryPreset (juce::AudioProcessorValueTreeState& state, int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& preset = list[(size_t) index];

    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, value] : preset.values)
        if (auto* rp = state.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));

    for (size_t i = 0; i < preset.routes.size() && (int) i < numModSlots; ++i)
    {
        const auto& r = preset.routes[i];
        auto set = [&] (const juce::String& id, float v) { if (auto* rp = state.getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
        set (slotSrc ((int) i), (float) r.src);
        set (slotDst ((int) i), (float) r.dst);
        set (slotAmt ((int) i), r.amt);
    }
}

} // namespace ff
