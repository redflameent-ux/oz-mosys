#pragma once
// =====================================================================================
//  Arturia importer: reads Arturia preset files and rebuilds them as VIBRA-VII presets.
//  Arturia stores every preset (Pigments, Analog Lab and the whole V Collection) as a
//  Boost "serialization::archive" text file: plain text, space-separated, with each
//  parameter written as <length> <name> <value>. We read the metadata (name, bank, type,
//  tags, comment) from the header and, for Pigments, the real parameters too (oscillators,
//  filter, envelopes, LFOs, effects). Analog Lab / V Collection patches use a different
//  parameter name per engine, so those are rebuilt from the sound type plus the handful of
//  parameters Arturia shares across engines. Each Arturia soundsource is swapped for the
//  closest VIBRA-VII engine, so the result is a close cousin, not a copy. No Arturia audio,
//  wavetable or sample data is read or copied.
//  Results go into private expansions on this computer: Documents/VIBRA-VII/Expansions.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "OmniImport.h"   // shares Converted + writePreset

namespace ff::import
{
// Convert one Arturia archive (already sliced out of a file) into a Converted preset.
// `name` / `bank` usually come from the file and its folder. `apvts` supplies ranges + defaults.
bool convertArturiaPreset (const juce::MemoryBlock& archive, const juce::String& name, const juce::String& bank,
                           juce::AudioProcessorValueTreeState& apvts, Converted& out);

// Whole folders / loose files (any mix). Each top-level product folder (e.g. "Pigments",
// "Analog Lab V", "Mini V3") becomes one expansion "Arturia - <name>". Bank files that pack
// several presets are split into their individual patches. progress(done,total,name) returns
// false to cancel. Returns presets written.
int importArturia (const juce::Array<juce::File>& sources, juce::AudioProcessorValueTreeState& apvts,
                   std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);
} // namespace ff::import
