#pragma once
// =====================================================================================
//  Reveal Sound Spire importer (.spf).
//  Spire presets are fixed 3133-byte files with a float parameter block; the param name->id map
//  comes from a table inside Spire.vst3 (see docs/PLUGIN_ORACLE.md). We read each preset's real
//  values directly - no plugin needed - and rebuild a close OZ-mosys cousin: oscillators, filters,
//  envelopes, pan and effect sends come across, with tuning and times approximate. Each top-level
//  folder becomes one expansion "Spire - <name>" in Documents/OZ-mosys/Expansions.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "OmniImport.h"   // shares Converted + writePreset

namespace ff::import
{
int importSpire (const juce::Array<juce::File>& sources, juce::AudioProcessorValueTreeState& apvts,
                 std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);
} // namespace ff::import
