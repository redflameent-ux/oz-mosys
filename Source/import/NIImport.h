#pragma once
// =====================================================================================
//  Native Instruments importer: FM8 (.fm8 / .nfm8) and Absynth 5 (.nabs / .ksd).
//  Unlike Omnisphere and Arturia, these are closed binary formats with no published layout,
//  so we do NOT read their synthesis parameters. We take the preset's name, its folder/bank,
//  and any readable sound-type words inside the file, then rebuild a close VIBRA-VII cousin
//  from that: FM8 patches on the OZ FM engine, Absynth patches on the engine that best fits
//  the sound type. No NI audio, wavetable or sample data is read or copied.
//  Results go into private expansions on this computer: Documents/VIBRA-VII/Expansions.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "OmniImport.h"   // shares Converted + writePreset

namespace ff::import
{
enum class NIKind { FM8, Absynth };

// Whole folders / loose files. Each top-level folder becomes one expansion
// "FM8 - <name>" / "Absynth - <name>". Returns presets written.
int importNI (NIKind kind, const juce::Array<juce::File>& sources, juce::AudioProcessorValueTreeState& apvts,
              std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);
} // namespace ff::import
