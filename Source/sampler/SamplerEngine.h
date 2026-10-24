#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Library.h"
#include "SamplerVoice.h"
#include <array>
#include <atomic>
#include <mutex>

// =====================================================================================
//  OZ-mosys sampler: the multisample engine from Sol Intent, as one layer.
//  Plays recordings from the sample library (Documents/OZ-mosys/Samples by default).
//  The SAMPLER page drives it: browser, 4x4 pads and the mini mapper.
// =====================================================================================
namespace ff
{
class SamplerEngine
{
public:
    explicit SamplerEngine (juce::AudioProcessorValueTreeState& state, juce::MidiKeyboardState& keys);
    ~SamplerEngine();

    void prepare (double sampleRate, int block);
    // renders into `out` (added). midi has already been merged with the on-screen keyboard.
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, int numSamples);
    void panic() { panicRequested = true; }

    // ---- library (message thread) - same names the Sol Intent screens use
    void rescanLibrary();
    const std::vector<sol::InstrumentInfo>& getInstrumentList() const { return instrumentList; }
    void selectInstrument (int layer, int indexInList);
    void loadFolderIntoLayer (int layer, const juce::File& folder);
    void reloadLayersUsing (const juce::File& folder);
    int  findInList (const juce::File& folder) const;
    sol::InstrumentInfo getLayerInfo (int) const { return info; }
    sol::InstrumentPtr getDisplayInstrument (int);
    bool isLoading() const { return loadsInFlight.load() > 0; }
    juce::String getStatus();
    void setStatus (const juce::String&);
    void loadNow (const sol::InstrumentInfo&);    // on the calling thread (tests, state restore)

    int  getArticulation (int) const { return articulation.load(); }
    void setArticulation (int, int index) { articulation = juce::jmax (0, index); }

    // state
    void saveTo (juce::ValueTree& state) const;
    void restoreFrom (const juce::ValueTree& state);

    // ---- what the screens read
    juce::MidiKeyboardState& keyboardState;
    std::array<std::atomic<float>, 128> noteGlow {};
    std::atomic<int> lastNote { -1 };
    int padBaseNote = 48;
    int activeLayer = 0;

private:
    void queueLoad (const sol::InstrumentInfo&);
    void hand (sol::InstrumentPtr, const juce::String& error, int generation);

    juce::AudioProcessorValueTreeState& apvts;
    juce::Synthesiser synth;
    sol::VoiceSettings settings;
    juce::SpinLock swapLock;
    sol::InstrumentPtr pending, current, display;
    std::vector<sol::InstrumentPtr> retired;
    std::atomic<bool> hasPending { false }, panicRequested { false };
    std::atomic<int> articulation { 0 }, generation { 0 }, loadsInFlight { 0 };
    sol::InstrumentInfo info;
    juce::AudioBuffer<float> buffer;
    float gainL = 0.0f, gainR = 0.0f;
    unsigned int rr = 0;

    juce::AudioFormatManager formats;
    juce::ThreadPool loader { 1 };
    std::mutex displayMutex;
    juce::String status;
    std::vector<sol::InstrumentInfo> instrumentList;

    struct
    {
        std::atomic<float> *play, *level, *tune, *fine, *pan, *attack, *decay, *sustain, *release, *letRing, *vel;
    } P {};
};
} // namespace ff
