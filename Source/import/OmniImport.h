#pragma once
// =====================================================================================
//  Omnisphere importer: reads Omnisphere patch files (.prt_omn - plain XML) and rebuilds
//  them as OZ-mosys presets. Everything that shapes the sound (filters, envelopes, LFOs,
//  arp, effects, mod wheel routings, layer pitch / level / pan) is carried across; each
//  Omnisphere soundsource is swapped for the closest OZ-mosys engine, chosen from the
//  soundsource's name and folder (no Omnisphere audio is read or copied).
//  Results go into private expansions on this computer: Documents/OZ-mosys/Expansions.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace ff::import
{
struct Converted
{
    juce::String name, bank, tags, notes;
    std::map<juce::String, float> values;   // param id -> real value (only what differs from Init)
    juce::String arpPattern;                // ArpPattern::toString(), empty = default
    juce::String author;                    // preset author line; empty = "Imported (Omnisphere)"
};

// one patch. `apvts` supplies parameter ranges and defaults.
bool convertOmniPatch (const juce::XmlElement& root, const juce::String& name, const juce::String& bank,
                       juce::AudioProcessorValueTreeState& apvts, Converted& out);

// writes a sparse .ozpreset (loadPresetFromFile fills the rest with defaults)
bool writePreset (const Converted&, const juce::File& file);

// whole folder (recursively). Each top-level sub-folder becomes one expansion "Omni - <name>".
// progress(done, total, currentName) returns false to cancel. Returns presets written.
// any mix of folders, .prt_omn patches and .omnisphere bundles (patches inside bundles are read
// straight from the bundle; their sample data is never touched)
int importOmnisphere (const juce::Array<juce::File>& sources, juce::AudioProcessorValueTreeState& apvts,
                      std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);

int importOmnisphereFolder (const juce::File& root, juce::AudioProcessorValueTreeState& apvts,
                            std::function<bool (int, int, const juce::String&)> progress, juce::String& summary);
} // namespace ff::import
