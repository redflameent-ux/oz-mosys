#pragma once
// Expansion packs: collections of presets with cover art, installed from a single
// .ozpack file (a zip holding pack.xml, cover.png and Presets/*.ozpreset).
#include <juce_gui_basics/juce_gui_basics.h>

namespace ff::packs
{
struct Pack
{
    juce::String name, tagline, author, style;
    juce::File folder;            // empty for the built-in banks
    juce::Image cover;            // may be null -> drawn procedurally
    int presetCount = 0;
    bool builtIn = false;
};

juce::File expansionsFolder();                    // Documents/OZ-mosys/Expansions
std::vector<Pack> builtInPacks();                 // one per factory bank
std::vector<Pack> installedPacks();
juce::Array<juce::File> presetsIn (const Pack&);
const char* taglineFor (const juce::String& bank);
juce::String styleFor (const juce::String& bank);

bool install (const juce::File& ozpack, juce::String& nameOut, juce::String& error);
bool remove (const Pack&);
bool makePack (const juce::File& out, const juce::String& name, const juce::String& tagline, const juce::String& author,
               const juce::String& style, const juce::File& coverImage, const juce::Array<juce::File>& presets, juce::String& error);

// procedural cover for packs without artwork (deterministic per name)
void drawCover (juce::Graphics&, juce::Rectangle<float>, const juce::String& name, juce::Colour accent);
} // namespace ff::packs
