#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class FourfoldProcessor;

// =====================================================================================
//  A patch split into sections (an engine, the filter, an envelope, the effects ...).
//  Used for subpresets (save / load / copy / paste a single section), the Mutator's
//  locks, and Init / Randomize per section.
// =====================================================================================
namespace ff::design
{
enum Section
{
    secWavetable, secAnalog, secGranular, secResonator, secFilter, secAmpEnv, secEnv2, secEnv3,
    secLfos, secMsegs, secMatrix, secMacros, secEffects, secVoice,
    numSections
};

const juce::StringArray& sectionNames();
bool paramInSection (const juce::String& paramId, int section);
std::vector<juce::RangedAudioParameter*> sectionParams (juce::AudioProcessorValueTreeState&, int section);
int sectionOfParam (const juce::String& paramId);

// a section as data (parameter values + MSEG shapes for the MSEG section)
juce::ValueTree captureSection (FourfoldProcessor&, int section);
void applySection (FourfoldProcessor&, const juce::ValueTree&, int onlySection = -1);

juce::File subpresetFolder (int section);
bool saveSection (FourfoldProcessor&, int section, const juce::File&);
bool loadSection (FourfoldProcessor&, int section, const juce::File&);
void initSection (FourfoldProcessor&, int section);

// clipboard for copy / paste of a section
juce::ValueTree& sectionClipboard();

// sets a parameter from a 0..1 value as one undoable host gesture
void setNormalised (juce::RangedAudioParameter*, float value);
} // namespace ff::design
