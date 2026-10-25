// =====================================================================================
//  ImportLib - run any OZ-mosys preset importer headlessly over a folder.
//
//  The importers (Omnisphere / Arturia / NI / Hive ...) live in the plugin and need the
//  processor's AudioProcessorValueTreeState for parameter ranges + defaults. This tool spins up
//  a FourfoldProcessor offline and calls the chosen importer, so a whole library can be converted
//  from the command line instead of clicking the Expansions page. Opt-in + local only
//  (cmake -DFOURFOLD_BUILD_TOOLS=ON); never built in CI.
//
//  Usage:
//      ImportLib <arturia|absynth|fm8|hive|omni> <folder-or-file> [<more>...]
//  Each importer writes .ozpreset packs into Documents/OZ-mosys/Expansions, naming packs itself.
// =====================================================================================
#include "../Source/PluginProcessor.h"
#include "../Source/import/OmniImport.h"
#include "../Source/import/ArturiaImport.h"
#include "../Source/import/NIImport.h"
#include "../Source/import/HiveImport.h"
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3)
    {
        std::cerr << "usage: ImportLib <arturia|absynth|fm8|hive|omni> <folder-or-file> [<more>...]\n";
        return 1;
    }
    const juce::String which = juce::CharPointer_UTF8 (argv[1]);   // copy-init: avoids the most-vexing-parse
    juce::Array<juce::File> srcs;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String path = juce::CharPointer_UTF8 (argv[i]);
        srcs.add (juce::File (path));
    }

    FourfoldProcessor proc;
    juce::String summary;
    if      (which == "arturia") ff::import::importArturia (srcs, proc.apvts, nullptr, summary);
    else if (which == "absynth") ff::import::importNI (ff::import::NIKind::Absynth, srcs, proc.apvts, nullptr, summary);
    else if (which == "fm8")     ff::import::importNI (ff::import::NIKind::FM8, srcs, proc.apvts, nullptr, summary);
    else if (which == "hive")    ff::import::importHive (srcs, proc.apvts, nullptr, summary);
    else if (which == "omni")    ff::import::importOmnisphere (srcs, proc.apvts, nullptr, summary);
    else { std::cerr << "unknown importer: " << which << "\n"; return 2; }

    std::printf ("%s\n", summary.toRawUTF8());
    return 0;
}
