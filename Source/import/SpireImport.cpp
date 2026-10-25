#include "SpireImport.h"
#include "SpireConvert.h"
#include <set>
#include <map>
#include <cmath>

using namespace juce;

namespace ff::import
{
namespace
{
bool convertSpire (const File& spf, const String& name, const String& bank,
                   AudioProcessorValueTreeState& apvts, Converted& out)
{
    std::map<String, float> P;
    if (! spire::parseSpf (spf, P)) return false;
    std::map<String, float> v;
    spire::toOz (P, v);

    out = {};
    out.name = name;
    out.bank = bank;
    out.author = "Imported (Spire)";
    out.tags = "spire, imported";
    out.notes = "Imported from Reveal Sound Spire by reading the preset file directly. A close "
                "cousin: oscillators, filters, envelopes, pan and effect sends come from the "
                "preset's real values; tuning and envelope times are approximate.";
    // clamp each value to the parameter's range and keep only what differs from the default
    for (auto& [id, x] : v)
        if (auto* pr = apvts.getParameter (id))
        {
            const float def = pr->convertFrom0to1 (pr->getDefaultValue());
            const float clamped = pr->convertFrom0to1 (pr->convertTo0to1 (x));
            if (std::abs (clamped - def) > 1.0e-4f) out.values[id] = clamped;
        }
    return true;
}

struct Item { String product, sub, name; File file; };
} // namespace

int importSpire (const Array<File>& sources, AudioProcessorValueTreeState& apvts,
                 std::function<bool (int, int, const String&)> progress, String& summary)
{
    std::vector<Item> items;
    auto addRoot = [&] (const File& root)
    {
        auto files = root.findChildFiles (File::findFiles, true, "*.spf");
        files.sort();
        for (auto& f : files)
        {
            const auto rel = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            Item it;
            it.product = rel.containsChar ('/') ? rel.upToFirstOccurrenceOf ("/", false, false) : root.getFileName();
            const String after = rel.fromFirstOccurrenceOf ("/", false, false);
            if (after.containsChar ('/')) it.sub = after.upToLastOccurrenceOf ("/", false, false);
            it.name = f.getFileNameWithoutExtension();
            it.file = f;
            items.push_back (it);
        }
    };
    for (auto& src : sources)
    {
        if (src.isDirectory()) addRoot (src);
        else if (src.hasFileExtension ("spf"))
        { Item it; it.product = src.getParentDirectory().getFileName(); it.name = src.getFileNameWithoutExtension(); it.file = src; items.push_back (it); }
    }

    const auto expansions = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("OZ-mosys").getChildFile ("Expansions");
    int written = 0, failed = 0;
    std::map<String, int> perPack;
    std::set<String> seen;
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        if (progress && ! progress (i, (int) items.size(), it.name)) break;

        const String packName = "Spire - " + File::createLegalFileName (it.product);
        if (! seen.insert (packName + "/" + it.sub + "/" + it.name).second) continue;

        Converted cv;
        if (! convertSpire (it.file, it.name, packName, apvts, cv)) { ++failed; continue; }
        if (it.sub.isNotEmpty()) cv.tags << ", " << it.sub.replaceCharacter ('/', ' ').toLowerCase();

        auto packDir = expansions.getChildFile (packName);
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", packName);
            px.setAttribute ("tagline", "Your " + it.product + " Spire patches, rebuilt in OZ-mosys");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }
        auto dir = packDir.getChildFile ("Presets");
        if (it.sub.isNotEmpty()) dir = dir.getChildFile (File::createLegalFileName (it.sub.replaceCharacter ('/', ' ')));
        if (writePreset (cv, dir.getChildFile (File::createLegalFileName (cv.name) + ".ozpreset"))) { ++written; ++perPack[packName]; }
        else ++failed;
    }
    summary = String (written) + " presets imported into " + String ((int) perPack.size()) + " collection" + (perPack.size() == 1 ? "" : "s");
    if (failed > 0) summary << " (" << failed << " skipped)";
    return written;
}
} // namespace ff::import
