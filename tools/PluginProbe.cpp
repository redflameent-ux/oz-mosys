// =====================================================================================
//  PluginProbe - the "plugin-oracle" for reading undocumented binary presets.
//
//  The idea: a preset in a closed binary format (DUNE .fxp, Serum .fxp, Spire .spf ...) is
//  opaque on disk, but the plugin itself knows how to decode it. So we host the real plugin
//  headlessly, load each preset INTO it, then read every parameter BY NAME through the plugin
//  API. Out comes a labelled dump - exactly what an OZ importer needs.
//
//  This only works on a machine that has the plugins installed and licensed, so it builds as a
//  separate, opt-in console tool (cmake -DFOURFOLD_BUILD_TOOLS=ON) and runs locally, never in CI.
//
//  Usage:
//      PluginProbe <plugin.vst3|.dll|.component> <preset.fxp | folder-of-presets>
//  Prints a JSON array of { "preset": name, "params": { "<name>": <0..1 value>, ... } }.
//
//  Scope / caveats (this is a starting point to iterate on locally):
//   - Handles the VST2 ".fxp" opaque-chunk form (FPCh): the chunk is the plugin's own state,
//     fed to setStateInformation. Whether a given plugin's VST3 build accepts its old .fxp chunk
//     is plugin-specific - most that share a chunk format across VST2/VST3 (e.g. Serum) do.
//   - The ".fxp" parameter-list form (FxCk) and VST3 ".vstpreset" are TODO.
//   - Some plugins expose fewer parameters to a host than their full internal set.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cstring>

using namespace juce;

// Extract the raw plugin state chunk from a VST2 .fxp "opaque chunk" (FPCh) preset.
// FXP layout (all multi-byte ints are BIG-endian):
//   'CcnK' | byteSize | 'FPCh' | version | fxID | fxVersion | numPrograms | prgName[28] | chunkSize | chunk[]
static MemoryBlock fxpChunk (const File& f)
{
    MemoryBlock mb;
    if (! f.loadFileAsData (mb) || mb.getSize() < 64) return {};
    const auto* d = static_cast<const uint8*> (mb.getData());
    auto be32 = [&] (int off) { return (uint32) ((d[off] << 24) | (d[off + 1] << 16) | (d[off + 2] << 8) | d[off + 3]); };
    if (std::memcmp (d, "CcnK", 4) != 0 || std::memcmp (d + 8, "FPCh", 4) != 0) return {};   // not an opaque-chunk fxp
    const uint32 chunkSize = be32 (56);
    if ((size_t) 60 + chunkSize > mb.getSize()) return {};
    return MemoryBlock (d + 60, chunkSize);
}

static String jstr (const String& s)   // minimal JSON string literal
{
    String out = "\"";
    for (auto c : s)
    {
        if (c == '"' || c == '\\') out << '\\' << (juce_wchar) c;
        else if (c == '\n' || c == '\r' || c == '\t') out << ' ';
        else out << (juce_wchar) c;
    }
    return out + "\"";
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: PluginProbe <plugin.vst3|.dll|.component> <preset.fxp | folder>\n";
        return 1;
    }
    ScopedJuceInitialiser_GUI juceInit;   // plugin scanning/hosting needs the message manager

    AudioPluginFormatManager fm;
    fm.addDefaultFormats();

    const File pluginFile (String (CharPointer_UTF8 (argv[1])));
    OwnedArray<PluginDescription> types;
    for (auto* fmt : fm.getFormats())
        fmt->findAllTypesForFile (types, pluginFile.getFullPathName());
    if (types.isEmpty())
    {
        std::cerr << "no plugin found in " << argv[1] << "\n";
        return 2;
    }

    String err;
    auto inst = fm.createPluginInstance (*types[0], 44100.0, 512, err);
    if (inst == nullptr)
    {
        std::cerr << "could not load plugin: " << err << "\n";
        return 3;
    }
    inst->prepareToPlay (44100.0, 512);

    Array<File> presets;
    const File arg2 (String (CharPointer_UTF8 (argv[2])));
    if (arg2.isDirectory()) presets = arg2.findChildFiles (File::findFiles, true, "*.fxp");
    else                    presets.add (arg2);

    std::cout << "[\n";
    bool firstPreset = true;
    for (auto& pf : presets)
    {
        const auto chunk = fxpChunk (pf);
        if (chunk.getSize() == 0) continue;                       // not an opaque-chunk .fxp - skip for now

        inst->setStateInformation (chunk.getData(), (int) chunk.getSize());

        if (! firstPreset) std::cout << ",\n";
        firstPreset = false;

        std::cout << "  {" << jstr ("preset") << ":" << jstr (pf.getFileNameWithoutExtension()) << ","
                  << jstr ("params") << ":{";
        bool firstParam = true;
        for (auto* p : inst->getParameters())
        {
            if (p == nullptr) continue;
            if (! firstParam) std::cout << ",";
            firstParam = false;
            std::cout << jstr (p->getName (64)) << ":" << String (p->getValue(), 6);
        }
        std::cout << "}}";
    }
    std::cout << "\n]\n";

    inst->releaseResources();
    return 0;
}
