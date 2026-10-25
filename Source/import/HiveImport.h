#pragma once
// =====================================================================================
//  u-he Hive importer (.h2p).
//  u-he presets are plain readable text: a /*@meta*/ header plus "#cm=<module>" sections
//  of key=value parameters, every one named. So unlike the closed formats we read the real
//  parameters directly - oscillators, filters, envelopes, LFOs, the full named mod matrix
//  and the effects - and rebuild a close OZ-mosys cousin. Each top-level folder becomes one
//  expansion "Hive - <name>" in Documents/OZ-mosys/Expansions. Returns presets written.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "OmniImport.h"   // shares Converted + writePreset

namespace ff::import
{
int importHive (const juce::Array<juce::File>& sources, juce::AudioProcessorValueTreeState& apvts,
                std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);
} // namespace ff::import
