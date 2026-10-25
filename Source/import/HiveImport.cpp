#include "HiveImport.h"
#include "../Params.h"
#include <set>
#include <map>
#include <cmath>
#include <algorithm>

using namespace juce;

namespace ff::import
{
namespace
{
// ---------------------------------------------------------------------------------------
//  u-he .h2p is plain text: a /*@meta*/ header, then "#cm=<module>" sections of Key=Value
//  lines, then a trailing binary blob we stop before. We read it into module -> (key -> value).
// ---------------------------------------------------------------------------------------
struct Patch
{
    String author, usage;
    StringArray modSrc;                                   // index -> Hive mod-source name
    std::map<String, std::map<String, String>> cm;        // module -> (key -> value)

    bool has (const String& m, const String& k) const
    {
        auto it = cm.find (m);
        return it != cm.end() && it->second.find (k) != it->second.end();
    }
    String s (const String& m, const String& k) const
    {
        auto it = cm.find (m); if (it == cm.end()) return {};
        auto jt = it->second.find (k); return jt == it->second.end() ? String() : jt->second;
    }
    float f (const String& m, const String& k, float def = 0.0f) const { return has (m, k) ? s (m, k).getFloatValue() : def; }
    int   i (const String& m, const String& k, int def = 0)       const { return has (m, k) ? s (m, k).getIntValue()   : def; }
};

Patch parse (const String& text)
{
    Patch p;
    auto unq = [] (String v) { v = v.trim(); if (v.startsWithChar ('\'') && v.endsWithChar ('\'') && v.length() >= 2) v = v.substring (1, v.length() - 1); return v; };
    const auto lines = StringArray::fromLines (text);
    String cur;
    bool inMeta = false;
    for (int li = 0; li < lines.size(); ++li)
    {
        const String t = lines[li].trim();
        if (t.startsWith ("$$$$") || t.startsWith ("// Section for")) break;   // binary blob starts here
        if (t.startsWith ("/*@meta")) { inMeta = true; continue; }
        if (inMeta)
        {
            if (t == "*/") { inMeta = false; continue; }
            if (t == "Author:" && li + 1 < lines.size()) p.author = unq (lines[li + 1]);
            else if (t == "Usage:" && li + 1 < lines.size()) p.usage = unq (lines[li + 1]);
            continue;
        }
        if (t.startsWith ("#cm=")) { cur = t.substring (4); continue; }
        if (t.startsWith ("#ms=")) { p.modSrc.add (t.substring (4)); continue; }
        if (t.startsWithChar ('#')) continue;              // other directives (#AM, #Vers, #nm, #mv...)
        const int eq = t.indexOfChar ('=');
        if (eq > 0 && cur.isNotEmpty()) p.cm[cur][t.substring (0, eq)] = t.substring (eq + 1);
    }
    return p;
}

// Hive's 0..100 time knobs -> seconds (soft exponential curve)
float hiveTime (float v)
{
    v = jlimit (0.0f, 100.0f, v) / 100.0f;
    return 0.001f + 20.0f * v * v * v;
}

struct Builder { std::map<String, float> v; void set (const String& id, float x) { v[id] = x; } };

bool convertHive (const String& text, const String& name, const String& bank,
                  AudioProcessorValueTreeState& apvts, Converted& out)
{
    const Patch p = parse (text);
    if (p.cm.empty()) return false;

    Builder b;
    for (auto* e : { "wt", "an", "gr", "res", "fm" }) b.set (String (e) + "_on", 0.0f);

    // ---- oscillators: Osc1 -> wavetable engine, Osc2 -> analog engine ----
    // (exact wavetable/waveform differs between synths, so we carry pitch / level / pan /
    //  unison / detune faithfully and leave the engine's own default waveform)
    if (p.f ("Osc1", "Volume") > 0.5f)
    {
        b.set ("wt_on", 1.0f);
        b.set ("wt_oct", (float) jlimit (-3, 3, p.i ("Osc1", "Octave")));
        b.set ("wt_semi", (float) jlimit (-12, 12, p.i ("Osc1", "Semi")));
        b.set ("wt_unison", (float) jlimit (1, 7, p.i ("Osc1", "Unison", 1)));
        b.set ("wt_detune", jlimit (0.0f, 1.0f, p.f ("Osc1", "Detune") / 100.0f));
        b.set ("wt_level", jlimit (0.0f, 1.0f, p.f ("Osc1", "Volume", 50.0f) / 100.0f));
        b.set ("wt_pan", jlimit (-1.0f, 1.0f, p.f ("Osc1", "Pan") / 100.0f));
    }
    if (p.f ("Osc2", "Volume") > 0.5f)
    {
        b.set ("an_on", 1.0f);
        b.set ("an_oct", (float) jlimit (-3, 3, p.i ("Osc2", "Octave")));
        b.set ("an_semi", (float) jlimit (-12, 12, p.i ("Osc2", "Semi")));
        b.set ("an_level", jlimit (0.0f, 1.0f, p.f ("Osc2", "Volume", 50.0f) / 100.0f));
        b.set ("an_pan", jlimit (-1.0f, 1.0f, p.f ("Osc2", "Pan") / 100.0f));
        b.set ("an_shape", 1.0f);                                  // default saw
        b.set ("an_sub", jlimit (0.0f, 1.0f, p.f ("Osc2", "SubVol") / 100.0f));
    }
    if (b.v["wt_on"] < 0.5f && b.v["an_on"] < 0.5f) b.set ("wt_on", 1.0f);

    // ---- filters (Cutoff is semitone-based ~0..150; Res/Env/Key are 0..100) ----
    auto filt = [&] (const String& mod, const String& pre)
    {
        if (! p.cm.count (mod)) { b.set (pre + "_on", 0.0f); return; }
        b.set (pre + "_on", 1.0f);
        b.set (pre + "_type", 1.0f);                               // LP 12 (Hive's filter model isn't mapped 1:1)
        b.set (pre + "_cutoff", jlimit (0.0f, 1.0f, p.f (mod, "Cutoff", 100.0f) / 150.0f));
        b.set (pre + "_res", jlimit (0.0f, 0.95f, p.f (mod, "Res") / 100.0f));
        b.set (pre + "_env", jlimit (-1.0f, 1.0f, p.f (mod, "Env") / 100.0f));
        if (pre == "flt") b.set ("flt_key", jlimit (0.0f, 1.0f, p.f (mod, "Key") / 100.0f));
    };
    filt ("Filter1", "flt");
    const bool f2used = p.cm.count ("Filter2")
                      && (p.i ("Filter2", "inO1") || p.i ("Filter2", "inO2") || p.i ("Filter2", "inS1") || p.i ("Filter2", "inS2"));
    if (f2used) { filt ("Filter2", "flt2"); b.set ("flt_route", 1.0f); }
    else b.set ("flt2_on", 0.0f);

    // ---- envelopes ----
    auto env = [&] (const String& mod, const String& pre, float sDef)
    {
        if (! p.cm.count (mod)) return;
        b.set (pre + "_a", hiveTime (p.f (mod, "Atk")));
        b.set (pre + "_d", jmax (0.001f, hiveTime (p.f (mod, "Dec", 30.0f))));
        b.set (pre + "_s", jlimit (0.0f, 1.0f, p.f (mod, "Sus", sDef * 100.0f) / 100.0f));
        b.set (pre + "_r", jmax (0.005f, hiveTime (p.f (mod, "Rel", 20.0f))));
    };
    env ("AmpEnv1", "amp", 0.85f);
    env ("ModEnv1", "e2", 0.0f);
    env ("ModEnv2", "e3", 0.5f);

    // ---- LFOs ----
    auto lfo = [&] (const String& mod, const String& pre)
    {
        if (! p.cm.count (mod)) return;
        const float r = jlimit (0.0f, 1.0f, p.f (mod, "Rate") / 100.0f);
        b.set (pre + "_rate", jlimit (0.01f, 30.0f, 0.05f * std::pow (600.0f, r)));
        b.set (pre + "_shape", (float) (p.i (mod, "Wave") % 6));
    };
    lfo ("LFO1", "lfo1");
    lfo ("LFO2", "lfo2");

    // ---- mod matrix: Hive names every routing, so this comes across faithfully ----
    auto hiveSrc = [&] (int idx) -> int
    {
        if (idx < 0 || idx >= p.modSrc.size()) return -1;
        const String s = p.modSrc[idx];
        if (s == "ModWhl") return srcModWheel;
        if (s == "LFO 1") return srcLfo1;
        if (s == "LFO 2" || s == "Vibrato LFO") return srcLfo2;
        if (s == "Velocity") return srcVelocity;
        if (s == "Pressure" || s == "Xpress" || s == "Breath") return srcAftertouch;
        if (s == "KeyFollow" || s == "KeyFollow2") return srcKeyTrack;
        if (s == "Random" || s == "Alternate") return srcRandom;
        if (s.startsWith ("Mod Envelope 1") || s.startsWith ("Amp Envelope 1")) return srcEnv2;
        if (s.startsWith ("Mod Envelope 2") || s.startsWith ("Amp Envelope 2")) return srcEnv3;
        return -1;
    };
    auto hiveDst = [&] (const String& d) -> int
    {
        if (d.startsWith ("Filter1:Cut")) return dstCutoff;
        if (d.startsWith ("Filter2:Cut")) return dstCutoff2;
        if (d.startsWith ("Filter1:Res")) return dstResonance;
        if (d.startsWith ("Filter2:Res")) return dstRes2;
        if (d.startsWith ("Filter1:Env")) return dstCutoff;
        if (d.startsWith ("Osc1:Tune") || d.startsWith ("Osc1:Pitch")) return dstWtPitch;
        if (d.startsWith ("Osc2:Tune") || d.startsWith ("Osc2:Pitch")) return dstAnPitch;
        if (d.startsWith ("Osc1:Detune")) return dstWtDetune;
        if (d.startsWith ("Osc1:Pan") || d.startsWith ("Osc2:Pan")) return dstPan;
        if (d.startsWith ("Osc1:Vol")) return dstWtLevel;
        if (d.startsWith ("Osc2:Vol")) return dstAnLevel;
        if (d.startsWith ("LFO1:Rate")) return dstLfo1Rate;
        if (d.startsWith ("LFO2:Rate")) return dstLfo2Rate;
        if (d.containsIgnoreCase ("Pitch")) return dstPitchAll;
        return -1;
    };
    int slot = 0;
    auto addMM = [&] (int src, const String& dest, float depth)
    {
        if (src < 0 || slot >= numModSlots) return;
        const int dst = hiveDst (dest);
        const float amt = jlimit (-1.0f, 1.0f, depth / 100.0f);
        if (dst < 0 || std::abs (amt) < 0.01f) return;
        b.set (slotSrc (slot), (float) src);
        b.set (slotDst (slot), (float) dst);
        b.set (slotAmt (slot), amt);
        ++slot;
    };
    for (int n = 1; n <= 12 && slot < numModSlots; ++n)
    {
        const String mm = "MM" + String (n);
        if (! p.cm.count (mm) || p.i (mm, "Active") != 1) continue;
        const int src = hiveSrc (p.i (mm, "Source"));
        addMM (src, p.s (mm, "Dest1"), p.f (mm, "Depth1"));
        addMM (src, p.s (mm, "Dest2"), p.f (mm, "Depth2"));
    }

    // ---- effects -> OZ sends ----
    if (p.f ("Reverb", "Mix") > 1.0f) b.set ("rev_mix", jlimit (0.0f, 0.6f, p.f ("Reverb", "Mix") / 100.0f));
    if (p.f ("Delay", "Mix") > 1.0f)  b.set ("dly_mix", jlimit (0.0f, 0.5f, p.f ("Delay", "Mix") / 100.0f));
    if (p.f ("Chorus", "Wet") > 1.0f) b.set ("cho_mix", jlimit (0.0f, 0.5f, p.f ("Chorus", "Wet") / 100.0f));
    else if (p.f ("Phaser", "Wet") > 1.0f) b.set ("cho_mix", jlimit (0.0f, 0.4f, p.f ("Phaser", "Wet") / 100.0f * 0.6f));
    if (p.f ("Distort", "Mix") > 1.0f && p.f ("Distort", "Amount") > 1.0f) b.set ("fx_drive", jlimit (0.0f, 0.8f, p.f ("Distort", "Amount") / 100.0f));

    // ---- arpeggiator ----
    if (p.i ("ARP", "OnOff") == 1) { b.set ("arp_on", 1.0f); b.set ("arp_oct", (float) jlimit (1, 4, 1 + p.i ("ARP", "Oct"))); }

    // ---- write up ----
    out = {};
    out.name = name;
    out.bank = bank;
    out.author = p.author.isNotEmpty() ? p.author : String ("Imported (Hive)");
    out.tags = "hive";
    out.notes = "Imported from u-he Hive, read from the preset's own named parameters (oscillators, "
                "filters, envelopes, LFOs, the full mod matrix and effects)."
              + (p.usage.isNotEmpty() ? " Original note: " + p.usage.replaceCharacters ("\r\n", "  ") : String());

    for (auto& [id, x] : b.v)
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

int importHive (const Array<File>& sources, AudioProcessorValueTreeState& apvts,
                std::function<bool (int, int, const String&)> progress, String& summary)
{
    std::vector<Item> items;
    auto addRoot = [&] (const File& root)
    {
        auto files = root.findChildFiles (File::findFiles, true, "*.h2p");
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
        else if (src.hasFileExtension ("h2p"))
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

        const String text = it.file.loadFileAsString();
        if (text.isEmpty()) { ++failed; continue; }

        const String packName = "Hive - " + File::createLegalFileName (it.product);
        if (! seen.insert (packName + "/" + it.sub + "/" + it.name).second) continue;

        Converted cv;
        if (! convertHive (text, it.name, packName, apvts, cv)) { ++failed; continue; }
        if (it.sub.isNotEmpty()) cv.tags << ", " << it.sub.replaceCharacter ('/', ' ').toLowerCase();

        auto packDir = expansions.getChildFile (packName);
        if (! packDir.getChildFile ("pack.xml").existsAsFile())
        {
            packDir.createDirectory();
            XmlElement px ("Pack");
            px.setAttribute ("name", packName);
            px.setAttribute ("tagline", "Your " + it.product + " Hive patches, rebuilt in OZ-mosys");
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
    if (failed > 0) summary << " (" << failed << " couldn't be read)";
    return written;
}
} // namespace ff::import
