#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "SynthVoice.h"
#include "dsp/Effects.h"
#include "dsp/Mseg.h"
#include <deque>
#include "sampler/SamplerEngine.h"
#include "design/MidiLearn.h"
#include "roll/RollEngine.h"
#include "design/Mutator.h"

class FourfoldProcessor : public juce::AudioProcessor
{
public:
    FourfoldProcessor();
    ~FourfoldProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OZ-mosys"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    void setCurrentProgramIndexOnly (int index) { currentProgram = index; }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor ----
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::unique_ptr<ff::SamplerEngine> sampler;
    ff::roll::RollEngine roll;
    std::unique_ptr<ff::design::Mutator> mutator;
    std::unique_ptr<ff::design::MidiLearn> midiLearn;
    ff::FxRack rack;
    ff::perf::PerformEngine perform;
    void setArpPattern (const ff::perf::ArpPattern&);
    ff::perf::ArpPatternPtr getArpPattern() const { return arpPattern; }
    void syncRack() { rack.sync(); }
    void setRackIr (int slot, const juce::File& f) { rack.setIrFile (slot, f); }

    bool loadUserWavetable (const juce::File& file);
    bool loadGrainSample (const juce::File& file);
    void resetGrainSample();
    juce::String getUserTableName() const;
    juce::String getGrainSourceName() const;

    ff::WavetableBank::Ptr getDisplayTable() const;   // table currently selected
    ff::GrainSource::Ptr getDisplayGrains() const;

    bool savePresetToFile (const juce::File& file);
    bool loadPresetFromFile (const juce::File& file);
    static juce::File getUserPresetFolder();
    juce::String currentPresetName { "Init" };
    std::atomic<float> modView[ff::numModSources] {};   // live mod values for the GUI
    std::atomic<int> activeVoices { 0 };
    float liveModWheel() const { return shared.modWheel; }

    // preset info (Absynth-style): shown and edited on the DESIGN page
    struct PresetInfo { juce::String author, bank, tags, notes; float mapX = 0.5f, mapY = 0.5f, movement = 0.5f; bool measured = false; };
    PresetInfo presetInfo;
    static juce::String getDefaultAuthor();
    static void setDefaultAuthor (const juce::String&);
    void measureCurrentSound();   // fills presetInfo's map position (renders offline, ~1 s)

    // live positions for the displays (first active voice), -1 if none
    float getLiveWtPos() const;
    float getLiveGrPos() const;

    std::atomic<float> outputLevel { 0.0f };

    // ---- breakpoint envelopes (message thread) ----
    void setMseg (int index, const ff::MsegShape& shape);
    ff::MsegPtr getMseg (int index) const;
    juce::ValueTree msegsToTree() const;
    void msegsFromTree (const juce::ValueTree&);
    static ff::MsegShape defaultMseg (int index);
    static constexpr int numMsegs = 4;

    // macro names (shown on the macro bar), saved with the sound
    juce::StringArray macroNames { "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6", "Macro 7", "Macro 8" };

private:
    struct RackTimer : juce::Timer { FourfoldProcessor* owner = nullptr; void timerCallback() override { owner->rack.sync(); } } rackTimer;
    double rackPpq = 0.0;
    ff::perf::ArpPatternPtr arpPattern = std::make_shared<ff::perf::ArpPattern>(), arpAudio = arpPattern;
    std::deque<ff::perf::ArpPatternPtr> arpKeep;
    void refreshSharedPointers();
    void applyStateTree (const juce::ValueTree& tree);

    ff::SharedState shared;
    juce::MPESynthesiser synth, monoSynth;
    int lastMasterBend = -1;
    void updateZoneLayout (int masterBendRange);
    bool wasMono = false;
    std::vector<ff::SynthVoice*> voices;
    ff::EffectsChain fx;

    std::array<ff::WavetableBank::Ptr, ff::numBuiltInTables> builtInTables;
    ff::WavetableBank::Ptr userTable;
    std::array<ff::GrainSource::Ptr, 4> builtInGrains;
    ff::GrainSource::Ptr userGrains;
    ff::GrainSource::Ptr selectedGrains() const;

    // pointers the audio thread is using this block
    ff::WavetableBank::Ptr audioTable;
    ff::GrainSource::Ptr audioGrains;
    juce::SpinLock swapLock;
    juce::AudioBuffer<float> samplerBuf;
    ff::MsegPtr msegShapes[4], msegAudio[4];
    std::deque<ff::MsegPtr> msegKeep;
    juce::MidiBuffer emptyMidi, samplerMidi;
    juce::ReferenceCountedArray<juce::ReferenceCountedObject> graveyard;   // old data freed off the audio thread

    juce::File userTableFile, grainFile;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourfoldProcessor)
};
