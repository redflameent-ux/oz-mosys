#pragma once
#include "Sections.h"
#include <array>
#include <map>

class FourfoldProcessor;

// =====================================================================================
//  The Mutator (after Absynth's): makes variations of the current sound.
//   - Random: nudges every unlocked section by "Amount", sometimes rewires the mod matrix.
//   - Morph: moves the unlocked sections part of the way toward another preset.
//  Every result goes into a history you can step back and forward through.
// =====================================================================================
namespace ff::design
{
class Mutator
{
public:
    explicit Mutator (FourfoldProcessor& p);

    // Absynth-style: Mutation Amount moves toward a similar-sounding preset,
    // Random Amount adds fresh randomness on top.
    float mutationAmount = 0.5f;      // 0..1
    float randomAmount = 0.3f;        // 0..1
    std::array<bool, numSections> locked {};

    void mutate();                    // a new variation of the current sound
    void retry();                     // throw the last variation away and try again from the same start
    void randomOnly (float amount);   // used by Randomize on a single section

    bool canGoBack() const { return index > 0; }
    bool canGoForward() const { return index + 1 < (int) history.size(); }
    void back();
    void forward();
    void goTo (int i);
    int position() const { return index; }
    int size() const { return (int) history.size(); }
    juce::String labelAt (int i) const { return juce::isPositiveAndBelow (i, (int) history.size()) ? history[(size_t) i].label : juce::String(); }
    void clearHistory() { history.clear(); index = -1; }
    juce::String lastTargetName;

private:
    struct Snapshot { std::map<juce::String, float> values; juce::StringArray msegs; juce::String label; };
    Snapshot capture() const;
    void restore (const Snapshot&);
    Snapshot targetFor (int presetIndex);
    void morph (Snapshot& s, const Snapshot& target, float amount);
    void randomise (Snapshot& s, float amount);
    int pickSimilarPreset();
    void mutateFrom (const Snapshot& base);
    Snapshot base;
    int mutationCount = 0;
    void keepPlayable (Snapshot& s);
    void push (const Snapshot& s);

    FourfoldProcessor& proc;
    std::vector<Snapshot> history;
    int index = -1;
    juce::Random rng;
};
} // namespace ff::design
