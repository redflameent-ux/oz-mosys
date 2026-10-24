#pragma once
// Sound types and modes for the browser chips (FM8 / Massive "attributes", Kontakt "sound type").
// Types come from words in the name / bank / your Types field; modes come from the sound's settings.
#include <juce_core/juce_core.h>
#include <functional>

namespace ff::design
{
inline const juce::StringArray& typeNames()
{
    static const juce::StringArray t { "Bass", "Pad", "Keys", "Lead", "Pluck", "Bell / Mallet", "Strings", "Brass / Wind",
                                       "Vocal / Choir", "Drums", "Percussion", "Drone", "Arp / Seq", "FX", "Synth" };
    return t;
}

inline const juce::StringArray& modeNames()
{
    static const juce::StringArray m { "Percussive", "Slow Attack", "Long Release", "Evolving", "Mono",
                                       "Wavetable", "Analog", "Granular", "Physical", "FM", "Sampled" };
    return m;
}

inline juce::StringArray classifyTypes (const juce::String& name, const juce::String& bank, const juce::String& tags)
{
    static const std::vector<std::pair<const char*, std::vector<const char*>>> words = {
        { "Bass",          { "bass", "sub ", "sub-", "808", "reese", "wobble", "low end", "subby" } },
        { "Pad",           { "pad", "swell", "atmos", "wash", "cloud", "haze", "halo", "frost", "dream", "mist", "aurora" } },
        { "Keys",          { "keys", "piano", "rhodes", "e-piano", "epiano", "organ", "clav", "wurl", "farfisa", "harpsichord" } },
        { "Lead",          { "lead", "solo", "sync", "whistle", "scream" } },
        { "Pluck",         { "pluck", "pizz", "harp", "koto", "kalimba", "kora", "mbira" } },
        { "Bell / Mallet", { "bell", "mallet", "marimba", "vibe", "chime", "music box", "glock", "bowl", "tingsha", "gamelan", "handpan", "tongue" } },
        { "Strings",       { "string", "sitar", "cello", "violin", "bowed", "guitar", "oud", "erhu", "fiddle" } },
        { "Brass / Wind",  { "brass", "horn", "flute", "bansuri", "shakuhachi", "sax", "reed", "duduk", "trumpet", "ney" } },
        { "Vocal / Choir", { "choir", "vox", "vocal", "chant", "voice", "monk", "breath", "throat", "sing" } },
        { "Drums",         { "kick", "snare", "hat", "clap", "drum kit", "rim", "tom ", "808 boom", "cowbell", "drums" } },
        { "Percussion",    { "perc", "tabla", "shaker", "conga", "hand drum", "clay", "djembe", "darbuka", "woodblock", "anvil" } },
        { "Drone",         { "drone", "resonance", "om ", "hum", "tanpura", "shruti" } },
        { "Arp / Seq",     { "arp", "seq", "motorik", "pulse train", "sequence" } },
        { "FX",            { "fx", "riser", "noise", "impact", "glitch", "rain", "wind", "scrap", "machine", "alarm", "static" } },
    };
    const auto hay = (" " + name + " " + tags + " ").toLowerCase();
    juce::StringArray out;
    for (auto& [type, list] : words)
        for (auto* w : list)
            if (hay.contains (w)) { out.addIfNotAlreadyThere (type); break; }

    // types written into your Types field count too
    for (auto& t : typeNames())
        if (tags.containsIgnoreCase (t.upToFirstOccurrenceOf (" /", false, false))) out.addIfNotAlreadyThere (t);

    if (out.isEmpty())
    {
        const auto b = bank.toLowerCase();
        if (b.contains ("drum")) out.add ("Drums");
        else if (b.contains ("perc")) out.add ("Percussion");
        else if (b.contains ("drone") || b.contains ("resonance")) out.add ("Drone");
        else if (b.contains ("chant") || b.contains ("choir")) out.add ("Vocal / Choir");
        else out.add ("Synth");
    }
    return out;
}

// value (id, default) returns the real (not 0..1) value of a parameter in that sound
inline juce::StringArray classifyModes (const std::function<float (const char*, float)>& value, float movement)
{
    juce::StringArray m;
    const float a = value ("amp_a", 0.005f), d = value ("amp_d", 0.3f), s = value ("amp_s", 0.85f), r = value ("amp_r", 0.35f);
    if (s < 0.08f && d < 2.0f) m.add ("Percussive");
    if (a > 0.25f) m.add ("Slow Attack");
    if (r > 2.0f) m.add ("Long Release");
    if (movement > 0.66f) m.add ("Evolving");
    if (value ("voice_mode", 0.0f) > 0.5f) m.add ("Mono");
    if (value ("wt_on", 1.0f) > 0.5f) m.add ("Wavetable");
    if (value ("an_on", 0.0f) > 0.5f) m.add ("Analog");
    if (value ("gr_on", 0.0f) > 0.5f) m.add ("Granular");
    if (value ("res_on", 0.0f) > 0.5f) m.add ("Physical");
    if (value ("fm_on", 0.0f) > 0.5f) m.add ("FM");
    if (value ("smp_play", 0.0f) > 0.5f) m.add ("Sampled");
    return m;
}
} // namespace ff::design
