#include "Packs.h"
#include "../Presets.h"

using namespace juce;

namespace ff::packs
{
File expansionsFolder()
{
    auto d = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VIBRA-VII").getChildFile ("Expansions");
    d.createDirectory();
    return d;
}

const char* taglineFor (const String& b)
{
    static const std::pair<const char*, const char*> t[] = {
        { "Signature", "The VIBRA-VII sound, start here" },
        { "Dark & Cinematic", "Shadows, tension and wide screens" },
        { "Reggae & Dub", "Bass weight, skanks and echo chambers" },
        { "Trance & Electronic", "Supersaws, plucks and euphoric lifts" },
        { "Hip-Hop & Trap", "808s, bells and late-night keys" },
        { "Sacred World", "Ancient strings, flutes and bowls" },
        { "Resonance & Drones", "Tuned air that never stops ringing" },
        { "Memphis Drums", "Cowbells, dusty kicks and tape grit" },
        { "Chants & Dark Keys", "Voices from the stone hall" },
        { "Trap & Fusion Percussion", "Hits from every corner of the map" },
        { "Industrial", "Scrap metal, gates and cold machines" },
        { "Japanese Acid Jazz", "Rhodes, flutes and Tokyo club nights" },
        { "60s Psych", "Farfisa, fuzz and swirling tape" },
        { "Dungeon Synth", "Candlelit pads from the keep" },
        { "8-Bit Lo-Fi", "Pulse waves and broken cartridges" },
        { "Krautrock", "Motorik sequences and cosmic organs" },
        { "Zamrock", "Fuzz guitars from 70s Lusaka" },
        { "Jazz Noir", "Smoky keys for rainy streets" } };
    for (auto& [k, v] : t) if (b == k) return v;
    return "";
}

String styleFor (const String& b)
{
    if (b.containsIgnoreCase ("Trance") || b.containsIgnoreCase ("Kraut") || b.containsIgnoreCase ("8-Bit")) return "Electronic";
    if (b.containsIgnoreCase ("Hip") || b.containsIgnoreCase ("Memphis") || b.containsIgnoreCase ("Trap")) return "Urban";
    if (b.containsIgnoreCase ("Cinematic") || b.containsIgnoreCase ("Drone") || b.containsIgnoreCase ("Dungeon") || b.containsIgnoreCase ("Chant")) return "Cinematic";
    if (b.containsIgnoreCase ("World") || b.containsIgnoreCase ("Reggae") || b.containsIgnoreCase ("Zam")) return "World";
    if (b.containsIgnoreCase ("Jazz") || b.containsIgnoreCase ("Psych")) return "Vintage";
    if (b.containsIgnoreCase ("Industrial")) return "Experimental";
    return "Core";
}

std::vector<Pack> builtInPacks()
{
    std::vector<Pack> v;
    for (auto& f : factoryPresets())
    {
        if (f.name == "Init") continue;
        auto it = std::find_if (v.begin(), v.end(), [&] (auto& p) { return p.name == f.category; });
        if (it == v.end())
        {
            Pack p;
            p.name = f.category; p.tagline = taglineFor (f.category); p.author = "VIBRA-VII";
            p.style = styleFor (f.category); p.builtIn = true; p.presetCount = 1;
            v.push_back (p);
        }
        else ++it->presetCount;
    }
    return v;
}

std::vector<Pack> installedPacks()
{
    std::vector<Pack> v;
    for (auto& d : RangedDirectoryIterator (expansionsFolder(), false, "*", File::findDirectories))
    {
        Pack p;
        p.folder = d.getFile();
        p.name = p.folder.getFileName();
        if (auto xml = XmlDocument::parse (p.folder.getChildFile ("pack.xml")))
        {
            p.name = xml->getStringAttribute ("name", p.name);
            p.tagline = xml->getStringAttribute ("tagline");
            p.author = xml->getStringAttribute ("author");
            p.style = xml->getStringAttribute ("style");
        }
        for (auto* n : { "cover.png", "cover.jpg", "cover.jpeg" })
            if (auto f = p.folder.getChildFile (n); f.existsAsFile()) { p.cover = ImageFileFormat::loadFrom (f); break; }
        p.presetCount = presetsIn (p).size();
        v.push_back (p);
    }
    std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.name.compareNatural (b.name) < 0; });
    return v;
}

Array<File> presetsIn (const Pack& p)
{
    if (p.builtIn || ! p.folder.isDirectory()) return {};
    auto files = p.folder.findChildFiles (File::findFiles, true, "*.ozpreset");
    files.sort();
    return files;
}

bool install (const File& ozpack, String& nameOut, String& error)
{
    ZipFile zip (ozpack);
    if (zip.getNumEntries() == 0) { error = "That file isn't an VIBRA-VII pack."; return false; }
    auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("ozpack_" + String::toHexString (Random::getSystemRandom().nextInt()));
    tmp.createDirectory();
    // refuse anything that tries to escape the folder
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto n = zip.getEntry (i)->filename;
        if (n.contains ("..") || n.startsWithChar ('/') || n.containsChar (':')) { error = "This pack has unsafe file names."; tmp.deleteRecursively(); return false; }
    }
    if (zip.uncompressTo (tmp).failed()) { error = "Couldn't unpack the file."; tmp.deleteRecursively(); return false; }
    // the pack may sit inside a single top folder
    File root = tmp;
    if (! root.getChildFile ("pack.xml").existsAsFile())
    {
        auto subs = tmp.findChildFiles (File::findDirectories, false);
        if (subs.size() == 1 && subs[0].getChildFile ("pack.xml").existsAsFile()) root = subs[0];
    }
    String name = ozpack.getFileNameWithoutExtension();
    if (auto xml = XmlDocument::parse (root.getChildFile ("pack.xml"))) name = xml->getStringAttribute ("name", name);
    if (root.findChildFiles (File::findFiles, true, "*.ozpreset").isEmpty()) { error = "No presets found in this pack."; tmp.deleteRecursively(); return false; }
    auto dest = expansionsFolder().getChildFile (File::createLegalFileName (name));
    if (dest.exists()) dest.deleteRecursively();
    const bool ok = root.copyDirectoryTo (dest);
    tmp.deleteRecursively();
    if (! ok) { error = "Couldn't copy the pack into your Expansions folder."; return false; }
    nameOut = name;
    return true;
}

bool remove (const Pack& p)
{
    if (p.builtIn || ! p.folder.isAChildOf (expansionsFolder())) return false;
    return p.folder.moveToTrash() || p.folder.deleteRecursively();
}

bool makePack (const File& out, const String& name, const String& tagline, const String& author, const String& style,
               const File& coverImage, const Array<File>& presets, String& error)
{
    if (presets.isEmpty()) { error = "Pick at least one preset."; return false; }
    XmlElement xml ("OZPack");
    xml.setAttribute ("name", name);
    xml.setAttribute ("tagline", tagline);
    xml.setAttribute ("author", author);
    xml.setAttribute ("style", style);
    xml.setAttribute ("version", 1);
    ZipFile::Builder zb;
    zb.addEntry (new MemoryInputStream (MemoryBlock (xml.toString().toRawUTF8(), xml.toString().getNumBytesAsUTF8()), true), 9, "pack.xml", Time::getCurrentTime());
    if (coverImage.existsAsFile())
    {
        // normalise artwork to a 600px square PNG
        auto img = ImageFileFormat::loadFrom (coverImage);
        if (img.isValid())
        {
            const int s = juce::jmin (img.getWidth(), img.getHeight());
            img = img.getClippedImage ({ (img.getWidth() - s) / 2, (img.getHeight() - s) / 2, s, s }).rescaled (600, 600, Graphics::highResamplingQuality);
            MemoryOutputStream mo;
            PNGImageFormat().writeImageToStream (img, mo);
            zb.addEntry (new MemoryInputStream (mo.getMemoryBlock(), true), 0, "cover.png", Time::getCurrentTime());
        }
    }
    for (auto& f : presets)
    {
        // stamp the pack name in as the bank
        String text = f.loadFileAsString();
        if (auto px = XmlDocument::parse (text))
        {
            px->setAttribute ("bank", name);
            if (px->getStringAttribute ("author").isEmpty()) px->setAttribute ("author", author);
            text = px->toString();
        }
        zb.addEntry (new MemoryInputStream (MemoryBlock (text.toRawUTF8(), text.getNumBytesAsUTF8()), true), 9,
                     "Presets/" + f.getFileName(), Time::getCurrentTime());
    }
    out.deleteFile();
    FileOutputStream os (out);
    if (! os.openedOk()) { error = "Couldn't write " + out.getFileName(); return false; }
    if (! zb.writeToStream (os, nullptr)) { error = "Couldn't build the pack."; return false; }
    return true;
}

void drawCover (Graphics& g, Rectangle<float> r, const String& name, Colour accent)
{
    Random rng ((int64) (uint32) name.hashCode());
    Graphics::ScopedSaveState ss (g);
    Path clip; clip.addRoundedRectangle (r, 6.0f);
    g.reduceClipRegion (clip);
    const auto c2 = accent.withRotatedHue (0.12f + rng.nextFloat() * 0.25f);
    ColourGradient bg (accent.darker (2.6f), r.getX(), r.getY(), c2.darker (1.4f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (r);
    const int style = rng.nextInt (4);
    const auto cx = r.getX() + r.getWidth() * (0.3f + rng.nextFloat() * 0.4f), cy = r.getY() + r.getHeight() * (0.3f + rng.nextFloat() * 0.4f);
    if (style == 0)        // rings
        for (int i = 14; i > 0; --i)
        {
            const float rad = r.getWidth() * 0.06f * i;
            g.setColour ((i % 2 ? accent : c2).withAlpha (0.08f + 0.02f * (14 - i)));
            g.drawEllipse (cx - rad, cy - rad, rad * 2, rad * 2, 1.5f + (14 - i) * 0.2f);
        }
    else if (style == 1)   // waveform lines
        for (int l = 0; l < 22; ++l)
        {
            Path p;
            const float y0 = r.getY() + r.getHeight() * (l + 0.5f) / 22.0f;
            const float f = 1.0f + rng.nextFloat() * 3.0f, a = r.getHeight() * 0.02f * (1.0f + l % 5);
            for (int x = 0; x <= 60; ++x)
            {
                const float xx = r.getX() + r.getWidth() * x / 60.0f;
                const float env = std::exp (-std::pow ((xx - cx) / (r.getWidth() * 0.3f), 2.0f));
                const float yy = y0 + std::sin (x * 0.2f * f + l) * a * env;
                x == 0 ? p.startNewSubPath (xx, yy) : p.lineTo (xx, yy);
            }
            g.setColour (accent.withAlpha (0.15f + 0.5f * (float) std::exp (-std::pow ((y0 - cy) / (r.getHeight() * 0.25f), 2.0f))));
            g.strokePath (p, PathStrokeType (1.2f));
        }
    else if (style == 2)   // grain cloud
        for (int i = 0; i < 260; ++i)
        {
            const float a = rng.nextFloat() * MathConstants<float>::twoPi, d = std::pow (rng.nextFloat(), 0.6f) * r.getWidth() * 0.45f;
            const float s = 1.0f + rng.nextFloat() * 4.0f;
            g.setColour ((rng.nextBool() ? accent : c2.brighter()).withAlpha (0.2f + rng.nextFloat() * 0.6f));
            g.fillEllipse (cx + std::cos (a) * d - s / 2, cy + std::sin (a) * d * 0.7f - s / 2, s, s);
        }
    else                   // shards
        for (int i = 0; i < 18; ++i)
        {
            Path p;
            p.addTriangle (cx, cy, r.getX() + rng.nextFloat() * r.getWidth(), r.getY() + rng.nextFloat() * r.getHeight(),
                           r.getX() + rng.nextFloat() * r.getWidth(), r.getY() + rng.nextFloat() * r.getHeight());
            g.setColour ((i % 2 ? accent : c2).withAlpha (0.07f + rng.nextFloat() * 0.12f));
            g.fillPath (p);
        }
    ColourGradient shade (Colours::transparentBlack, r.getX(), r.getCentreY(), Colours::black.withAlpha (0.65f), r.getX(), r.getBottom(), false);
    g.setGradientFill (shade);
    g.fillRect (r);
}
} // namespace ff::packs
