#pragma once
// =====================================================================================
//  FX rack (Guitar Rig / Omnisphere style): 8 slots, each holding one unit picked from
//  a library. Slots 1-4 and 5-8 become the Low and High chains when Split is on.
//  Units are created on the message thread and handed to the audio thread lock-free.
// =====================================================================================
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <array>
#include <deque>

namespace ff
{
constexpr int numRackSlots = 8;

struct RackTime { double bpm = 120.0, ppq = 0.0; bool playing = false; };
struct UnitParams { float a = 0.5f, b = 0.5f, c = 0.5f, d = 0.5f; };

struct RackUnitInfo { const char* name; const char* category; const char* labels[4]; float defaults[4]; const char* help; };
const std::vector<RackUnitInfo>& rackUnits();          // index 0 = empty
float rackDefaultMix (int type);                        // wet-only units (delays, reverbs) start part-way
juce::StringArray rackUnitNames();
juce::StringArray rackCategories();

class RackUnit
{
public:
    virtual ~RackUnit() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() {}
    virtual void process (float* L, float* R, int n, const UnitParams& p, const RackTime& t) = 0;
    virtual void loadFile (const juce::File&) {}     // IR loader
    int type = 0;
};

std::unique_ptr<RackUnit> createRackUnit (int type);

// Owns the units. Message thread: sync() builds/replaces units when slot types change.
// Audio thread: process().
class FxRack
{
public:
    struct SlotRefs { std::atomic<float> *type = nullptr, *on = nullptr, *a = nullptr, *b = nullptr, *c = nullptr, *d = nullptr, *mix = nullptr; };
    std::array<SlotRefs, numRackSlots> refs;
    std::atomic<float>* split = nullptr;
    std::atomic<float>* xover = nullptr;
    std::array<juce::File, numRackSlots> irFiles;    // per-slot impulse responses (IR loader)

    void bind (juce::AudioProcessorValueTreeState& s);
    void prepare (double sr, int maxBlock);
    void sync();                                      // message thread
    void process (juce::AudioBuffer<float>& buf, const RackTime& t);   // audio thread
    void setIrFile (int slot, const juce::File& f);   // message thread

private:
    double sampleRate = 48000.0;
    int maxBlock = 512;
    std::array<std::shared_ptr<RackUnit>, numRackSlots> pending, active;
    std::array<int, numRackSlots> builtType {};
    std::deque<std::pair<std::shared_ptr<RackUnit>, juce::uint32>> keepAlive;
    juce::SpinLock lock;
    juce::AudioBuffer<float> low, high, dry;
    juce::dsp::LinkwitzRileyFilter<float> lpX, hpX;
    float lastX = -1.0f;
};
} // namespace ff
