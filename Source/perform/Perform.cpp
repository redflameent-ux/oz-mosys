#include "Perform.h"

namespace ff::perf
{
// ===================================================================================== pattern
juce::String ArpPattern::toString() const
{
    juce::String s;
    for (auto& st : steps)
        s << (st.on ? 1 : 0) << ',' << (st.tie ? 1 : 0) << ',' << (st.accent ? 1 : 0) << ',' << st.noteIdx << ','
          << st.octave << ',' << st.transpose << ',' << juce::String (st.velocity, 2) << ';';
    return s;
}

ArpPattern ArpPattern::fromString (const juce::String& text)
{
    ArpPattern p;
    int i = 0;
    for (auto& tok : juce::StringArray::fromTokens (text, ";", ""))
    {
        if (i >= maxSteps) break;
        auto f = juce::StringArray::fromTokens (tok, ",", "");
        if (f.size() < 7) continue;
        auto& st = p.steps[(size_t) i++];
        st.on = f[0].getIntValue() != 0; st.tie = f[1].getIntValue() != 0; st.accent = f[2].getIntValue() != 0;
        st.noteIdx = juce::jlimit (0, 9, f[3].getIntValue());
        st.octave = juce::jlimit (-2, 2, f[4].getIntValue());
        st.transpose = juce::jlimit (-12, 12, f[5].getIntValue());
        st.velocity = juce::jlimit (0.05f, 1.0f, f[6].getFloatValue());
    }
    return p;
}

const juce::StringArray& ArpPattern::presetNames()
{
    static const juce::StringArray n { "Straight", "Offbeat pulse", "Trance gate", "Dotted", "Octave jump", "Reggaeton",
                                       "Trap rolls", "Acid slide", "Chord stabs", "Random" };
    return n;
}

ArpPattern ArpPattern::preset (int which)
{
    ArpPattern p;
    juce::Random r ((juce::int64) juce::Time::getMillisecondCounter());
    for (int i = 0; i < maxSteps; ++i)
    {
        auto& s = p.steps[(size_t) i];
        s = {};
        s.accent = i % 4 == 0;
        switch (which)
        {
            case 1: s.on = i % 2 == 1; s.accent = i % 4 == 2; break;
            case 2: s.on = (i % 8) != 3 && (i % 8) != 6; s.accent = i % 4 == 0; break;
            case 3: s.on = i % 3 == 0; break;
            case 4: s.octave = (i % 2) ? 1 : 0; break;
            case 5: s.on = (i % 16 == 0 || i % 16 == 3 || i % 16 == 6 || i % 16 == 10 || i % 16 == 12); s.accent = i % 16 == 0 || i % 16 == 10; break;
            case 6: s.on = true; s.tie = false; s.velocity = (i % 8 < 6) ? 0.5f + 0.06f * (i % 8) : 0.95f; s.accent = i % 8 == 7; break;
            case 7: s.tie = (i % 4 == 2); s.accent = i % 3 == 0; s.transpose = (i % 8 == 5) ? 12 : 0; break;
            case 8: s.on = i % 4 == 0 || i % 8 == 6; s.noteIdx = 9; break;
            case 9: s.on = r.nextFloat() > 0.3f; s.tie = r.nextFloat() > 0.85f; s.accent = r.nextFloat() > 0.7f;
                    s.octave = r.nextFloat() > 0.8f ? (r.nextBool() ? 1 : -1) : 0; s.velocity = 0.5f + 0.5f * r.nextFloat(); break;
            default: break;
        }
    }
    return p;
}

// ===================================================================================== chords
static const int scaleSteps[9][7] = {
    { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
    { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 3, 5, 6, 8, 10 }, { 0, 1, 4, 5, 7, 8, 10 } };

// semitone offset of scale degree d (any integer) from the key root
static int degreeOffset (int scale, int d)
{
    const int oct = (int) std::floor ((double) d / 7.0);
    const int idx = ((d % 7) + 7) % 7;
    return scaleSteps[juce::jlimit (0, 8, scale)][idx] + 12 * oct;
}

std::vector<int> chordFor (int note, int key, int scale, int type, int voicing, bool followKey)
{
    std::vector<int> t;
    if (followKey)
    {
        // snap the played note to the nearest scale degree, then stack thirds inside the scale
        const int rel = ((note - key) % 12 + 12) % 12;
        int best = 0, bestDist = 99;
        for (int d = 0; d < 7; ++d)
        {
            const int off = degreeOffset (scale, d) % 12;
            const int dist = std::abs (off - rel);
            if (dist < bestDist) { bestDist = dist; best = d; }
        }
        const int base = degreeOffset (scale, best);
        auto deg = [&] (int k) { return degreeOffset (scale, best + k) - base; };
        const int snap = (degreeOffset (scale, best) % 12) - rel;   // move the root onto the scale
        switch (type)
        {
            case 1: t = { 0, deg (2), deg (4), deg (6) }; break;
            case 2: t = { 0, deg (2), deg (4), deg (6), deg (8) }; break;
            case 3: t = { 0, deg (1), deg (4) }; break;
            case 4: t = { 0, deg (3), deg (4) }; break;
            case 5: t = { 0, 7 }; break;
            case 6: t = { 0, 12 }; break;
            case 7: t = { 0, deg (2), deg (4), deg (8) }; break;
            case 8: t = { 0, deg (2), deg (4), deg (6), deg (10) }; break;
            default: t = { 0, deg (2), deg (4) }; break;
        }
        for (auto& v : t) v += snap;
    }
    else
    {
        static const std::vector<int> shapes[9] = { { 0, 4, 7 }, { 0, 4, 7, 10 }, { 0, 4, 7, 10, 14 }, { 0, 2, 7 }, { 0, 5, 7 },
                                                    { 0, 7 }, { 0, 12 }, { 0, 4, 7, 14 }, { 0, 4, 7, 10, 17 } };
        t = shapes[juce::jlimit (0, 8, type)];
        if (scale == 1 || scale == 2 || scale == 3 || scale == 6 || scale == 7)   // minor-flavoured scales give minor shapes
            for (auto& v : t) if (v == 4) v = 3;
    }
    // voicing: 0 close, 1 open (spread every other tone up an octave), 2 drop-2, 3 bass + chord
    if (voicing == 1) for (size_t i = 1; i < t.size(); i += 2) t[i] += 12;
    else if (voicing == 2 && t.size() >= 3) t[t.size() - 2] -= 12;
    else if (voicing == 3) { t.push_back (-12); }
    std::sort (t.begin(), t.end());
    t.erase (std::unique (t.begin(), t.end()), t.end());
    return t;
}

juce::String chordName (int note, int key, int scale, int type, bool followKey)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto tones = chordFor (note, key, scale, type, 0, followKey);
    if (tones.empty()) return {};
    const int root = ((note + tones[0]) % 12 + 12) % 12;
    juce::String q;
    auto has = [&] (int iv) { for (auto v : tones) if (((v - tones[0]) % 12 + 12) % 12 == iv) return true; return false; };
    if (type == 5) q = "5";
    else if (type == 6) q = " oct";
    else if (type == 3) q = "sus2";
    else if (type == 4) q = "sus4";
    else
    {
        const bool minor = has (3) && ! has (4), dim = minor && has (6) && ! has (7);
        q = dim ? "dim" : minor ? "m" : "";
        if (type == 1 || type == 2 || type == 8) q += has (11) ? "maj7" : has (10) ? "7" : "";
        if (type == 2) q += "(9)";
        if (type == 7) q += "add9";
        if (type == 8) q += "(11)";
    }
    return juce::String (names[root]) + q;
}

// ===================================================================================== engine
void PerformEngine::prepare (double sr)
{
    sampleRate = sr;
    held.reserve (128); latched.reserve (128); pending.reserve (512); chordOut.reserve (128); sounding.reserve (128);
    reset();
}

void PerformEngine::reset()
{
    held.clear(); latched.clear(); pending.clear(); chordOut.clear(); sounding.clear();
    internalPpq = 0; wasRunning = false; lastStepIndex = -1; seqPos = 0; latchArmed = false;
    currentStep = -1;
}

void PerformEngine::noteInToArp (int note, float vel, bool on)
{
    if (on)
    {
        if (latchArmed) { latched.clear(); latchArmed = false; }
        held.push_back ({ note, vel, ++orderCounter });
        latched.push_back ({ note, vel, orderCounter });
    }
    else
    {
        held.erase (std::remove_if (held.begin(), held.end(), [note] (const Held& h) { return h.note == note; }), held.end());
        if (held.empty()) latchArmed = true;     // the next new note starts a fresh latched chord
    }
}

std::vector<int> PerformEngine::arpSequence (const Settings& s) const
{
    const auto& src = s.latch ? latched : held;
    std::vector<Held> notes (src.begin(), src.end());
    if (notes.empty()) return {};
    std::vector<int> base;
    if (s.mode == 3) { std::sort (notes.begin(), notes.end(), [] (auto& a, auto& b) { return a.order < b.order; }); }
    else std::sort (notes.begin(), notes.end(), [] (auto& a, auto& b) { return a.note < b.note; });
    for (int o = 0; o < juce::jlimit (1, 4, s.octaves); ++o)
        for (auto& h : notes) base.push_back (h.note + 12 * o);
    std::vector<int> seq;
    switch (s.mode)
    {
        case 1: seq.assign (base.rbegin(), base.rend()); break;
        case 2: seq = base; for (int i = (int) base.size() - 2; i > 0; --i) seq.push_back (base[(size_t) i]); break;
        case 6: { int lo = 0, hi = (int) base.size() - 1; while (lo <= hi) { seq.push_back (base[(size_t) lo++]); if (lo <= hi) seq.push_back (base[(size_t) hi--]); } break; }
        case 7: for (size_t i = 1; i < base.size(); ++i) { seq.push_back (base[0]); seq.push_back (base[i]); } if (seq.empty()) seq = base; break;
        default: seq = base; break;
    }
    return seq;
}

void PerformEngine::allArpNotesOff (juce::MidiBuffer& out, int sample)
{
    for (auto& sn : sounding) out.addEvent (juce::MidiMessage::noteOff (1, sn.first), sample);
    sounding.clear();
}

void PerformEngine::process (juce::MidiBuffer& midi, int n, const Settings& s, const ArpPattern& pattern,
                             double bpm, double hostPpq, bool hostPlaying)
{
    if (! s.chordsOn && ! s.arpOn)
    {
        if (! sounding.empty() || ! pending.empty()) { juce::MidiBuffer out; allArpNotesOff (out, 0); pending.clear(); for (auto m : midi) out.addEvent (m.getMessage(), m.samplePosition); midi.swapWith (out); }
        held.clear(); latched.clear(); chordOut.clear();
        currentStep = -1;
        return;
    }
    juce::MidiBuffer out, notesIn;
    const juce::int64 blockStart = sampleClock;

    // ---- 1. chords: expand note ons/offs (with strum + humanize scheduled into the future) ----
    for (const auto meta : midi)
    {
        auto m = meta.getMessage();
        const int pos = meta.samplePosition;
        if (! m.isNoteOnOrOff()) { out.addEvent (m, pos); continue; }
        const int note = m.getNoteNumber();
        if (! s.chordsOn) { notesIn.addEvent (m, pos); continue; }
        if (m.isNoteOn())
        {
            auto tones = chordFor (note, s.key, s.scale, s.type, s.voicing, s.followKey);
            lastChordRoot = note;
            std::vector<int> made;
            int k = 0;
            for (auto t : tones)
            {
                const int nn = juce::jlimit (0, 127, note + t + 12 * s.octave);
                const double strumMs = s.strum * 120.0 * k + s.humanize * rng.nextDouble() * 18.0;
                const float vel = juce::jlimit (0.05f, 1.0f, m.getFloatVelocity() * (1.0f - s.humanize * 0.25f * rng.nextFloat()));
                pending.push_back ({ blockStart + pos + (juce::int64) (strumMs * 0.001 * sampleRate), juce::MidiMessage::noteOn (1, nn, vel), note });
                made.push_back (nn);
                ++k;
            }
            chordOut.push_back ({ note, made });
        }
        else
        {
            for (auto it = chordOut.begin(); it != chordOut.end(); ++it)
                if (it->first == note)
                {
                    // cancel strummed notes that haven't started yet, release the rest
                    pending.erase (std::remove_if (pending.begin(), pending.end(), [note] (const Pending& p) { return p.sourceNote == note && p.msg.isNoteOn(); }), pending.end());
                    for (int nn : it->second) pending.push_back ({ blockStart + pos, juce::MidiMessage::noteOff (1, nn), note });
                    chordOut.erase (it);
                    break;
                }
        }
    }
    // due scheduled chord notes go to the arp input (or straight out)
    std::sort (pending.begin(), pending.end(), [] (auto& a, auto& b) { return a.time < b.time; });
    while (! pending.empty() && pending.front().time < blockStart + n)
    {
        notesIn.addEvent (pending.front().msg, (int) juce::jmax ((juce::int64) 0, pending.front().time - blockStart));
        pending.erase (pending.begin());
    }

    if (! s.arpOn)
    {
        for (const auto meta : notesIn) out.addEvent (meta.getMessage(), meta.samplePosition);
        if (! sounding.empty()) allArpNotesOff (out, 0);
        held.clear(); latched.clear();
        currentStep = -1;
        midi.swapWith (out);
        sampleClock += n;
        return;
    }

    // ---- 2. arpeggiator ----
    static const double rateBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    const double stepBeats = rateBeats[juce::jlimit (0, 5, s.rate)];
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const int steps = juce::jlimit (1, ArpPattern::maxSteps, s.numSteps);

    // walk the block sample by sample at note-input boundaries
    auto inIt = notesIn.begin();
    for (int i = 0; i < n; ++i)
    {
        while (inIt != notesIn.end() && (*inIt).samplePosition <= i)
        {
            const auto m = (*inIt).getMessage();
            const bool wasEmpty = (s.latch ? latched : held).empty();
            noteInToArp (m.getNoteNumber(), m.getFloatVelocity(), m.isNoteOn());
            if (m.isNoteOn() && wasEmpty && ! hostPlaying) { internalPpq = 0; lastStepIndex = -1; seqPos = 0; }   // key sync
            ++inIt;
        }
        const bool hasNotes = ! (s.latch ? latched : held).empty();
        const double ppq = hostPlaying ? hostPpq + i * beatsPerSample : internalPpq;
        if (! hostPlaying) internalPpq += beatsPerSample;

        // release notes whose gate ended
        for (auto it = sounding.begin(); it != sounding.end();)
            if (it->second >= 0 && blockStart + i >= it->second) { out.addEvent (juce::MidiMessage::noteOff (1, it->first), i); it = sounding.erase (it); }
            else ++it;

        if (! hasNotes) { if (! sounding.empty()) allArpNotesOff (out, i); continue; }

        // swing pushes every second step later
        const juce::int64 rawStep = (juce::int64) std::floor (ppq / stepBeats);
        const double into = ppq / stepBeats - (double) rawStep;
        const bool odd = (rawStep & 1) != 0;
        if (odd && into < s.swing * 0.5) continue;            // the swung step hasn't started yet
        if (rawStep == lastStepIndex) continue;
        lastStepIndex = rawStep;

        const int si = (int) (((rawStep % steps) + steps) % steps);
        currentStep = si;
        const auto& st = pattern.steps[(size_t) si];
        const bool tieOver = st.tie && ! sounding.empty();
        if (! tieOver)
        {
            // previous untied notes end at the step boundary
            for (auto it = sounding.begin(); it != sounding.end();)
                if (it->second < 0) { out.addEvent (juce::MidiMessage::noteOff (1, it->first), i); it = sounding.erase (it); } else ++it;
        }
        if (! st.on || tieOver) continue;

        auto seq = arpSequence (s);
        if (seq.empty()) continue;
        std::vector<int> toPlay;
        const auto& src = s.latch ? latched : held;
        if (st.noteIdx == 9 || s.mode == 5)
            for (auto& h : src) toPlay.push_back (h.note);
        else if (st.noteIdx >= 1)
        {
            std::vector<int> sorted; for (auto& h : src) sorted.push_back (h.note);
            std::sort (sorted.begin(), sorted.end());
            toPlay.push_back (sorted[(size_t) ((st.noteIdx - 1) % (int) sorted.size())]);
        }
        else
        {
            if (s.mode == 4) toPlay.push_back (seq[(size_t) rng.nextInt ((int) seq.size())]);
            else toPlay.push_back (seq[(size_t) (seqPos % (int) seq.size())]);
            ++seqPos;
        }
        float baseVel = 0.8f;
        for (auto& h : src) baseVel = h.vel;
        const float vel = juce::jlimit (0.05f, 1.0f, baseVel * st.velocity * 1.15f * (st.accent ? 1.25f : 0.85f));
        const double gateBeats = stepBeats * juce::jlimit (0.05f, 1.0f, s.gate);
        const juce::int64 offAt = (st.tie || pattern.steps[(size_t) ((si + 1) % steps)].tie) ? -1
                                    : blockStart + i + (juce::int64) (gateBeats / beatsPerSample);
        for (int nn : toPlay)
        {
            const int note = juce::jlimit (0, 127, nn + st.octave * 12 + st.transpose);
            out.addEvent (juce::MidiMessage::noteOn (1, note, vel), i);
            sounding.push_back ({ note, offAt });
        }
    }
    midi.swapWith (out);
    sampleClock += n;
}
} // namespace ff::perf
