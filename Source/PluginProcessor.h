#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "SynthVoice.h"
#include "dsp/Effects.h"

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

    const juce::String getName() const override { return "Fourfold"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor ----
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

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

    // live positions for the displays (first active voice), -1 if none
    float getLiveWtPos() const;
    float getLiveGrPos() const;

    std::atomic<float> outputLevel { 0.0f };

private:
    void refreshSharedPointers();
    void applyStateTree (const juce::ValueTree& tree);

    ff::SharedState shared;
    juce::Synthesiser synth;
    std::vector<ff::SynthVoice*> voices;
    ff::EffectsChain fx;

    std::array<ff::WavetableBank::Ptr, ff::numBuiltInTables> builtInTables;
    ff::WavetableBank::Ptr userTable;
    ff::GrainSource::Ptr builtInGrains, userGrains;

    // pointers the audio thread is using this block
    ff::WavetableBank::Ptr audioTable;
    ff::GrainSource::Ptr audioGrains;
    juce::SpinLock swapLock;
    juce::ReferenceCountedArray<juce::ReferenceCountedObject> graveyard;   // old data freed off the audio thread

    juce::File userTableFile, grainFile;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourfoldProcessor)
};
