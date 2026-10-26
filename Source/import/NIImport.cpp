#include "NIImport.h"
#include "../Params.h"
#include <set>
#include <algorithm>
#include <cstring>

using namespace juce;

namespace ff::import
{
namespace
{
bool any (const String& s, std::initializer_list<const char*> words)
{
    for (auto* w : words) if (s.containsIgnoreCase (w)) return true;
    return false;
}

// Collect readable ASCII runs (length >= 4) from a byte range. We keep only word-like runs
// (something with a vowel or a space) so binary noise doesn't trip the classifier.
void appendAsciiRuns (const unsigned char* d, size_t n, String& out)
{
    int run = 0; size_t start = 0;
    auto flush = [&]
    {
        if (run >= 4)
        {
            String s (CharPointer_UTF8 ((const char*) (d + start)), (size_t) run);
            if (s.containsAnyOf ("aeiouAEIOU "))   // skip tag/hex-looking junk like "DSINv", "taPsC"
                out << s << " ";
        }
        run = 0;
    };
    for (size_t i = 0; i < n; ++i)
    {
        const unsigned char c = d[i];
        if (c >= 32 && c < 127) { if (run == 0) start = i; ++run; }
        else flush();
    }
    flush();
}

// What we can learn about a preset from the file itself.
struct NIMeta
{
    String name;        // real name, when the file carries one (.ksd); else empty -> use filename
    String author;      // real author (+ vendor), when present
    String hint;        // text fed to the classifier (name, labels, NI category tags, sample refs)
    bool sampleBased = false;   // references a sample (Absynth sample patches)
};

// Parse the XML metadata document NI's SoundShell (.ksd) stores, zlib-compressed, inside the file.
// It carries the real name, author/vendor/bank and a rich tag taxonomy (Instrument/Type/Timbre/
// Articulation/Genre + user attributes), all of which make for far better classification than a
// filename guess.
void parseKsdXml (const String& xml, NIMeta& m)
{
    auto doc = XmlDocument::parse (xml);
    if (doc == nullptr) return;

    auto sub = [] (XmlElement* e, const char* tag) -> String
    { if (e != nullptr) if (auto* c = e->getChildByName (tag)) return c->getAllSubText().trim(); return {}; };

    m.name = sub (doc.get(), "doc_name");

    String authors;
    if (auto* info = doc->getChildByName ("info"))
    {
        if (auto* ca = info->getChildByName ("commonAttr"))
        {
            const String a = sub (ca, "Author"), v = sub (ca, "Vendor"), bank = sub (ca, "Bankname");
            if (a.isNotEmpty()) authors = a;
            if (v.isNotEmpty()) authors = authors.isNotEmpty() ? authors + " / " + v : v;
            m.hint << " " << bank << " " << sub (ca, "Comment");
        }
        // NI's musical tag taxonomy -> gather every attribute value.
        if (auto* mus = info->getChildByName ("MusicalAttr"))
            for (auto* a = mus->getFirstChildElement(); a != nullptr; a = a->getNextElement())
                for (int ai = 0; ai < a->getNumAttributes(); ++ai)
                    m.hint << " " << a->getAttributeValue (ai);
    }
    // Free-text category tags live in <info><attribute><value> blocks (often several values).
    for (auto* child = doc->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->hasTagName ("info"))
            for (auto* attr = child->getFirstChildElement(); attr != nullptr; attr = attr->getNextElement())
                if (attr->hasTagName ("attribute"))
                    for (auto* val = attr->getFirstChildElement(); val != nullptr; val = val->getNextElement())
                        if (val->hasTagName ("value"))
                            m.hint << " " << val->getAllSubText();

    m.author = authors;
}

// Read what the file can tell us. .nfm8 / .nabs carry readable labels and sample references as
// plain text; .ksd (NI SoundShell) carries a zlib-compressed XML metadata document.
NIMeta readMeta (const File& f, bool fm)
{
    NIMeta m;
    MemoryBlock mb;
    { FileInputStream in (f); if (! in.openedOk()) return m; in.readIntoMemoryBlock (mb, 1024 * 1024); }
    const auto* d = static_cast<const unsigned char*> (mb.getData());
    const size_t n = mb.getSize();

    appendAsciiRuns (d, n, m.hint);
    m.sampleBased = ! fm && (m.hint.containsIgnoreCase (".wav") || m.hint.containsIgnoreCase (".aif"));

    if (f.hasFileExtension ("ksd"))
    {
        // Inflate zlib blocks (0x78 0x01/0x9C/0xDA) until we find the one holding the XML document.
        for (size_t i = 0, tries = 0; i + 1 < n && tries < 24; ++i)
        {
            if (d[i] != 0x78) continue;
            const unsigned char f2 = d[i + 1];
            if (f2 != 0x01 && f2 != 0x9C && f2 != 0xDA) continue;
            ++tries;
            MemoryBlock comp (d + i, n - i);
            MemoryInputStream mis (comp, false);
            GZIPDecompressorInputStream z (&mis, false, GZIPDecompressorInputStream::zlibFormat);
            MemoryBlock dec;
            if (z.readIntoMemoryBlock (dec, 2 * 1024 * 1024) == 0 || dec.getSize() < 64) continue;

            const auto* dd = static_cast<const char*> (dec.getData());
            const int dn = (int) dec.getSize();
            int xs = -1;
            for (int k = 0; k + 5 < dn; ++k)
                if (dd[k] == '<' && dd[k+1] == '?' && dd[k+2] == 'x' && dd[k+3] == 'm' && dd[k+4] == 'l') { xs = k; break; }
            if (xs < 0) continue;                          // not the XML block; keep scanning
            int xe = dn;
            for (int k = xs; k + 16 <= dn; ++k)
                if (std::memcmp (dd + k, "</NI_DOC_HEADER>", 16) == 0) { xe = k + 16; break; }
            parseKsdXml (String (CharPointer_UTF8 (dd + xs), CharPointer_UTF8 (dd + xe)), m);
            break;
        }
    }
    return m;
}

enum class Cat { Bass, Lead, Pad, Keys, Organ, Brass, Strings, Pluck, Bell, Mallet, Seq, Perc, Vocal, Wind, SFX };

Cat classify (const String& s)
{
    if (any (s, { "bass", "808", "sub " }))                       return Cat::Bass;
    if (any (s, { "lead", "solo" }))                              return Cat::Lead;
    if (any (s, { "pad", "atmos", "drone", "texture", "ambient", "evolv", "soundscape", "string" })) return Cat::Pad;
    if (any (s, { "bell", "chime", "glass", "music box", "glock", "celesta" })) return Cat::Bell;
    if (any (s, { "mallet", "marimba", "vibra", "xylo", "kalimba" })) return Cat::Mallet;
    if (any (s, { "rhodes", "wurli", "e-piano", "epiano", "piano", "keyboard", "key", "clav", "dx" })) return Cat::Keys;
    if (any (s, { "organ", "b3", "hammond" }))                    return Cat::Organ;
    if (any (s, { "brass", "horn", "trumpet", "trombone", "sax" })) return Cat::Brass;
    if (any (s, { "pluck", "harp", "guitar", "pizz", "stab" }))   return Cat::Pluck;
    if (any (s, { "seq", "arp" }))                                return Cat::Seq;
    if (any (s, { "perc", "drum", "kick", "snare", "hat", "tom", "clap" })) return Cat::Perc;
    if (any (s, { "choir", "vox", "vocal", "voice", "chant" }))   return Cat::Vocal;
    if (any (s, { "wind", "breath", "flute", "woodwind", "air" }))  return Cat::Wind;
    if (any (s, { "fx", "noise", "riser", "impact", "hit", "sfx", "scape" })) return Cat::SFX;
    return Cat::Keys;
}

String catTag (Cat c)
{
    switch (c)
    {
        case Cat::Bass: return "bass"; case Cat::Lead: return "lead"; case Cat::Pad: return "pad";
        case Cat::Keys: return "keys"; case Cat::Organ: return "organ"; case Cat::Brass: return "brass";
        case Cat::Strings: return "strings"; case Cat::Pluck: return "pluck"; case Cat::Bell: return "bell";
        case Cat::Mallet: return "mallet"; case Cat::Seq: return "sequence"; case Cat::Perc: return "percussion";
        case Cat::Vocal: return "vocal"; case Cat::Wind: return "breath"; case Cat::SFX: return "fx";
    }
    return {};
}

struct Builder { std::map<String, float> v; void set (const String& id, float x) { v[id] = x; } };

// FM8 -> the OZ FM engine, with a starting six-operator feel chosen by category.
void buildFM (Builder& b, Cat c)
{
    for (auto* e : { "wt", "an", "gr", "res", "fm" }) b.set (String (e) + "_on", 0.0f);
    b.set ("fm_on", 1.0f);
    b.set ("flt_on", 1.0f);
    float a = 0.003f, d = 1.2f, s = 0.5f, r = 0.5f, index = 1.0f, ratioB = 1.0f, bToA = 0.35f;
    switch (c)
    {
        case Cat::Bell:   d = 3.0f; s = 0.0f; r = 1.5f; index = 1.3f; ratioB = 3.5f; bToA = 0.6f; break;
        case Cat::Keys:   d = 1.4f; s = 0.0f; r = 0.5f; index = 0.8f; ratioB = 14.0f; bToA = 0.5f; break;  // DX e-piano
        case Cat::Bass:   d = 0.6f; s = 0.7f; r = 0.25f; index = 0.7f; ratioB = 1.0f; bToA = 0.4f; break;
        case Cat::Brass:  a = 0.05f; s = 0.85f; index = 0.9f; ratioB = 1.0f; bToA = 0.3f; break;
        case Cat::Organ:  s = 1.0f; r = 0.15f; index = 0.3f; ratioB = 2.0f; bToA = 0.15f; break;
        case Cat::Pluck:  d = 0.8f; s = 0.0f; r = 0.4f; index = 1.0f; ratioB = 2.0f; bToA = 0.5f; break;
        case Cat::Mallet: d = 0.9f; s = 0.0f; r = 0.4f; index = 1.1f; ratioB = 7.0f; bToA = 0.55f; break;
        case Cat::Lead:   s = 0.9f; index = 1.0f; ratioB = 1.0f; bToA = 0.4f; break;
        case Cat::Pad:    a = 0.6f; r = 1.6f; s = 0.8f; index = 0.6f; ratioB = 1.0f; bToA = 0.3f; b.set ("rev_mix", 0.2f); break;
        default: break;
    }
    b.set ("fm_index", index);
    b.set ("fm2_ratio", ratioB);
    b.set ("fm_m21", jlimit (0.0f, 1.0f, bToA));
    b.set ("amp_a", a); b.set ("amp_d", d); b.set ("amp_s", s); b.set ("amp_r", r);
}

// Absynth -> whichever OZ engine best fits the sound type. A patch that references a sample
// (sampleBased) is rebuilt on OZ's granular engine, since those Absynth sounds are sample-driven.
void buildAbsynth (Builder& b, Cat c, bool sampleBased)
{
    for (auto* e : { "wt", "an", "gr", "res", "fm" }) b.set (String (e) + "_on", 0.0f);
    b.set ("flt_on", 1.0f);
    float a = 0.01f, d = 0.5f, s = 0.8f, r = 0.6f;
    auto wt  = [&] (int t, float pos, int uni) { b.set ("wt_on", 1.0f); b.set ("wt_table", (float) t); b.set ("wt_pos", pos); b.set ("wt_unison", (float) uni); };
    auto res = [&] (int t) { b.set ("res_on", 1.0f); b.set ("res_type", (float) t); };
    auto gr  = [&] (int src) { b.set ("gr_on", 1.0f); b.set ("gr_source", (float) src); };
    switch (c)
    {
        case Cat::Bass:    b.set ("an_on", 1.0f); b.set ("an_shape", 0.9f); b.set ("an_sub", 0.4f); s = 0.9f; r = 0.25f; break;
        case Cat::Lead:    b.set ("an_on", 1.0f); b.set ("an_shape", 1.0f); s = 0.9f; break;
        case Cat::Pad:     wt (1, 0.5f, 5); a = 1.0f; r = 2.0f; b.set ("rev_mix", 0.28f); break;
        case Cat::Keys:    res (18); d = 1.2f; s = 0.0f; break;
        case Cat::Organ:   wt (5, 0.5f, 1); s = 1.0f; r = 0.15f; break;
        case Cat::Brass:   b.set ("an_on", 1.0f); b.set ("an_shape", 1.0f); a = 0.06f; s = 0.85f; break;
        case Cat::Strings: wt (1, 0.5f, 5); a = 0.25f; r = 0.9f; break;
        case Cat::Pluck:   res (0); d = 0.9f; s = 0.0f; break;
        case Cat::Bell:    res (7); d = 3.0f; s = 0.0f; r = 1.2f; break;
        case Cat::Mallet:  res (17); d = 1.2f; s = 0.0f; break;
        case Cat::Seq:     b.set ("an_on", 1.0f); b.set ("an_shape", 1.0f); d = 0.3f; s = 0.4f; r = 0.2f; b.set ("arp_on", 1.0f); break;
        case Cat::Perc:    res (10); d = 0.4f; s = 0.0f; r = 0.2f; break;
        case Cat::Vocal:   gr (0); a = 0.3f; r = 1.0f; break;
        case Cat::Wind:    gr (2); a = 0.15f; r = 0.6f; break;
        case Cat::SFX:     gr (3); a = 0.4f; r = 1.8f; b.set ("rev_mix", 0.25f); break;
    }
    if (sampleBased)
    {
        for (auto* e : { "wt", "an", "res", "fm" }) b.set (String (e) + "_on", 0.0f);
        int src = 0;                                 // granular source by character
        switch (c) { case Cat::Wind: src = 2; break; case Cat::SFX: src = 3; break; default: src = 0; break; }
        gr (src);
    }
    b.set ("amp_a", a); b.set ("amp_d", d); b.set ("amp_s", s); b.set ("amp_r", r);
}

struct Item { String folder, name; File file; };
} // namespace

int importNI (NIKind kind, const Array<File>& sources, AudioProcessorValueTreeState& apvts,
              std::function<bool (int, int, const String&)> progress, String& summary)
{
    const bool fm = kind == NIKind::FM8;
    const String wildcard = fm ? "*.fm8;*.nfm8" : "*.nabs;*.ksd";
    const String label = fm ? "FM8" : "Absynth";

    std::vector<Item> items;
    auto addRoot = [&] (const File& root)
    {
        auto files = root.findChildFiles (File::findFiles, true, wildcard);
        files.sort();
        for (auto& f : files)
        {
            const auto rel = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            Item it;
            it.folder = rel.containsChar ('/') ? rel.upToFirstOccurrenceOf ("/", false, false) : root.getFileName();
            it.name = f.getFileNameWithoutExtension();
            it.file = f;
            items.push_back (it);
        }
    };
    for (auto& src : sources)
    {
        if (src.isDirectory()) addRoot (src);
        else if (src.hasFileExtension (fm ? "fm8;nfm8" : "nabs;ksd"))
        { Item it; it.folder = src.getParentDirectory().getFileName(); it.name = src.getFileNameWithoutExtension(); it.file = src; items.push_back (it); }
    }

    const auto expansions = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VIBRA-VII").getChildFile ("Expansions");
    int written = 0, failed = 0;
    std::map<String, int> perPack;
    std::set<String> seen;
    for (int i = 0; i < (int) items.size(); ++i)
    {
        const auto& it = items[(size_t) i];
        if (progress && ! progress (i, (int) items.size(), it.name)) break;

        const NIMeta meta = readMeta (it.file, fm);
        const String displayName = meta.name.isNotEmpty() ? meta.name : it.name;
        const String hint = displayName + " " + it.folder + " " + meta.hint;
        const Cat c = classify (hint);
        const bool sampleBased = meta.sampleBased;   // Absynth sample patches -> OZ granular engine

        Builder b;
        if (fm) buildFM (b, c); else buildAbsynth (b, c, sampleBased);

        Converted cv;
        cv.name = displayName;
        const String packName = label + " - " + File::createLegalFileName (it.folder);
        cv.bank = packName;
        cv.author = meta.author.isNotEmpty() ? String ("Imported (") + label + ") - " + meta.author
                                             : String ("Imported (") + label + ")";
        cv.tags = String (fm ? "fm8" : "absynth") + ", " + catTag (c) + (sampleBased ? ", sampled" : "");
        if (fm)
            cv.notes = "Imported from Native Instruments FM8. Rebuilt from the preset's name and sound type "
                       "(the FM8 format stores no readable parameters, so they are not read).";
        else
            cv.notes = String ("Imported from Native Instruments Absynth. Rebuilt from the preset's name, "
                       "sound type and embedded text (labels and sample references)")
                     + (sampleBased ? ", on OZ's granular engine because this patch is sample-based" : "")
                     + ". The Absynth format is closed, so its synth parameters themselves are not read.";

        for (auto& [id, x] : b.v)
            if (auto* p = apvts.getParameter (id))
            {
                const float def = p->convertFrom0to1 (p->getDefaultValue());
                const float clamped = p->convertFrom0to1 (p->convertTo0to1 (x));
                if (std::abs (clamped - def) > 1.0e-4f) cv.values[id] = clamped;
            }

        if (! seen.insert (packName + "/" + cv.name).second) continue;
        auto packDir = expansions.getChildFile (packName);
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", packName);
            px.setAttribute ("tagline", "Your " + it.folder + " " + label + " patches, rebuilt in VIBRA-VII");
            px.setAttribute ("author", "Imported");
            px.setAttribute ("style", "imported");
            px.writeTo (packDir.getChildFile ("pack.xml"));
        }
        if (writePreset (cv, packDir.getChildFile ("Presets").getChildFile (File::createLegalFileName (cv.name) + ".ozpreset")))
        { ++written; ++perPack[packName]; }
        else ++failed;
    }
    summary = String (written) + " presets imported into " + String ((int) perPack.size()) + " collection" + (perPack.size() == 1 ? "" : "s");
    if (failed > 0) summary << " (" << failed << " couldn't be read)";
    return written;
}
} // namespace ff::import
