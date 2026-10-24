#pragma once
// =====================================================================================
//  Performance tools that sit in front of the synth:
//   - Chords: one finger plays a whole chord (in a key, or a fixed shape), with strum + humanize
//   - Arpeggiator: 32-step pattern with FM8-style rows (on / tie / accent / note / octave /
//     transpose / velocity), synced to the host or its own clock, latch, swing.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include <array>

namespace ff::perf
{
inline const juce::StringArray& chordScaleNames()
{
    static const juce::StringArray n { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Harmonic Minor", "Locrian", "Hijaz" };
    return n;
}
inline const juce::StringArray& chordTypeNames()
{
    static const juce::StringArray n { "Triad", "Seventh", "Ninth", "Sus2", "Sus4", "Power (5th)", "Octaves", "Add9", "Eleventh" };
    return n;
}
inline const juce::StringArray& arpModeNames()
{
    static const juce::StringArray n { "Up", "Down", "Up / Down", "As played", "Random", "Chord", "Converge", "Bounce low" };
    return n;
}
inline const juce::StringArray& arpRateNames()
{
    static const juce::StringArray n { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" };
    return n;
}

struct ArpStep
{
    bool on = true, tie = false, accent = false;
    int noteIdx = 0;      // 0 = follow the mode, 1..8 = that held note, 9 = whole chord
    int octave = 0;       // -2..+2
    int transpose = 0;    // semitones -12..+12
    float velocity = 0.8f;
};

struct ArpPattern
{
    static constexpr int maxSteps = 32;
    std::array<ArpStep, maxSteps> steps {};
    juce::String toString() const;
    static ArpPattern fromString (const juce::String&);
    static ArpPattern preset (int which);              // starting patterns
    static const juce::StringArray& presetNames();
};
using ArpPatternPtr = std::shared_ptr<const ArpPattern>;

// chord tones (semitones from the played note) for the current chord settings
std::vector<int> chordFor (int midiNote, int key, int scale, int type, int voicing, bool followKey);
juce::String chordName (int midiNote, int key, int scale, int type, bool followKey);

class PerformEngine
{
public:
    struct Settings
    {
        bool chordsOn = false, followKey = true;
        int key = 0, scale = 0, type = 0, voicing = 0, octave = 0;
        float strum = 0.0f, humanize = 0.0f;
        bool arpOn = false, latch = false;
        int rate = 3, mode = 0, octaves = 1, numSteps = 16;
        float gate = 0.6f, swing = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();
    // transforms `midi` in place (notes in -> chords/arp notes out). ppq/bpm from the host when playing.
    void process (juce::MidiBuffer& midi, int numSamples, const Settings& s, const ArpPattern& pattern,
                  double bpm, double hostPpq, bool hostPlaying);
    std::atomic<int> currentStep { -1 };   // for the GUI
    std::atomic<int> lastChordRoot { -1 };

private:
    struct Held { int note; float vel; juce::int64 order; };
    struct Pending { juce::int64 time; juce::MidiMessage msg; int sourceNote; };
    void allArpNotesOff (juce::MidiBuffer& out, int sample);
    void noteInToArp (int note, float vel, bool on);
    std::vector<int> arpSequence (const Settings& s) const;

    double sampleRate = 48000.0;
    juce::int64 sampleClock = 0, orderCounter = 0;
    std::vector<Held> held, latched;
    bool latchArmed = false;
    std::vector<Pending> pending;
    std::vector<std::pair<int, std::vector<int>>> chordOut;   // source note -> notes it produced
    // arp state
    double internalPpq = 0.0;
    bool wasRunning = false;
    juce::int64 lastStepIndex = -1;
    int seqPos = 0;
    std::vector<std::pair<int, juce::int64>> sounding;           // note, sample time to release (-1 = until next step)
    juce::Random rng { 1234 };
};
} // namespace ff::perf
