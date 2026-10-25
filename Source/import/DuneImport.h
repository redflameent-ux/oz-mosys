#pragma once
// =====================================================================================
//  Synapse DUNE 3 importer (.fxp).
//  A DUNE preset is a VST2 FPCh wrapper (fxID "DNE3") around a zlib stream; inflated, it holds a
//  `para` block of [id][value] records. The parameter id->name map was recovered from a table
//  inside DUNE3.vst3 (see docs/PLUGIN_ORACLE.md). So we read each preset's REAL values straight
//  off disk - no plugin needed - and rebuild a close OZ-mosys cousin: filters, oscillator levels,
//  pan, envelopes and effect sends come across faithfully, tuning is approximate. Each top-level
//  folder becomes one expansion "DUNE 3 - <name>" in Documents/OZ-mosys/Expansions. DUNE 2 files
//  (fxID "DUE2") are a different format and are skipped. Returns presets written.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "OmniImport.h"   // shares Converted + writePreset

namespace ff::import
{
int importDune (const juce::Array<juce::File>& sources, juce::AudioProcessorValueTreeState& apvts,
                std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);
} // namespace ff::import
