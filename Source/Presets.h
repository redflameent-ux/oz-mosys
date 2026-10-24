#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

namespace ff
{
struct FactoryPreset
{
    juce::String name;
    juce::String category;
    std::vector<std::pair<juce::String, float>> values;   // real-world values (not 0..1)
    struct Route { int src, dst; float amt; };
    std::vector<Route> routes;
};

// Defined in Presets.cpp
const std::vector<FactoryPreset>& factoryPresets();

inline void applyFactoryPreset (juce::AudioProcessorValueTreeState& state, int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& preset = list[(size_t) index];

    // synth presets leave the sampler page alone
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! rp->getParameterID().startsWith ("smp_"))
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
