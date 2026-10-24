#pragma once
// =====================================================================================
//  Morph pad (after FM8's Morph square) + Orb (after Omnisphere's Orb).
//   - Four corner sounds A B C D. The puck position blends between them: every smooth
//     setting glides, switches (wave types, modes) jump to the nearest corner.
//   - The Orb moves the puck on its own: Dice (random jumps), Inertia (throw it and it
//     keeps bouncing), Attractor (circles the middle), Recorded path (your own motion).
//  Runs on the message thread (30x a second) and moves the real parameters, so what you
//  hear is always what the knobs show, and saving keeps the corners.
// =====================================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <map>
#include <vector>

namespace ff::perf
{
class MorphEngine : private juce::Timer
{
public:
    using Values = std::map<juce::String, float>;   // param id -> normalised value

    explicit MorphEngine (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~MorphEngine() override { stopTimer(); }

    static bool morphable (const juce::String& id);  // which params the pad moves
    Values captureNow() const;

    void setCorner (int i, const Values& v);
    const Values& corner (int i) const { return corners[(size_t) juce::jlimit (0, 3, i)]; }
    bool hasCorner (int i) const { return ! corner (i).empty(); }
    void clearCorners();

    // Orb input from the page
    void grab (float x, float y);                  // puck dragged by hand
    void release (float vx, float vy);             // let go (velocity per second, for Inertia)
    bool held = false;
    void startRecording() { path.clear(); recording = true; }
    void stopRecording() { recording = false; }
    bool isRecording() const { return recording; }
    const std::vector<juce::Point<float>>& getPath() const { return path; }
    juce::Point<float> puck() const { return { x, y }; }
    juce::Point<float> diceTarget() const { return target; }

    void saveTo (juce::ValueTree& state) const;
    void restoreFrom (const juce::ValueTree& state);   // a tree without a "Morph" child clears it

    std::function<void()> onMoved;    // GUI hook
    void tick() { timerCallback(); }  // tests

private:
    void timerCallback() override;
    void moveOrb (float dt);
    void apply();
    void setXY (float nx, float ny);
    static juce::String toString (const Values&);
    static Values fromString (const juce::String&);

    juce::AudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    std::array<Values, 4> corners;
    std::atomic<float>* pOn = nullptr, *pX = nullptr, *pY = nullptr, *pMode = nullptr, *pSpeed = nullptr, *pDepth = nullptr;
    float x = 0.0f, y = 1.0f, vx = 0.0f, vy = 0.0f;
    float lastAppliedX = -1.0f, lastAppliedY = -1.0f;
    bool dirty = true, recording = false;
    juce::Point<float> target { 0.5f, 0.5f };
    double diceClock = 0.0, phase = 0.0, pathPos = 0.0;
    std::vector<juce::Point<float>> path;
    juce::Random rng;
    double lastTime = 0.0;
};
} // namespace ff::perf
