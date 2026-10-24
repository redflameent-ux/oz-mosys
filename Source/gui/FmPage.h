#pragma once
// The FM page (FM8-style): the 6x6 FM matrix on the left, the six operators on the right.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../dsp/FmEngine.h"

class FourfoldProcessor;

namespace ff::gui
{
const juce::Colour fmCol { 0xff4fd6b0 };

// one cell of the matrix: drag up/down to set, double-click to clear
class FmCell : public juce::Component, public juce::SettableTooltipClient
{
public:
    FmCell (juce::RangedAudioParameter& p, bool isFeedback, bool isOutput);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    float value = 0.0f;
private:
    juce::ParameterAttachment attach;
    bool feedback, output;
    float downValue = 0.0f;
};

class FmEnvView : public juce::Component
{
public:
    FmEnvView (juce::AudioProcessorValueTreeState& s, int op) : state (s), index (op) {}
    void paint (juce::Graphics&) override;
private:
    juce::AudioProcessorValueTreeState& state;
    int index;
};

class FmPage : public juce::Component, private juce::Timer
{
public:
    FmPage (FourfoldProcessor&, std::function<void (Knob&)> registerKnob);
    ~FmPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyAlgorithm (int index);
    Knob* knob (const juce::String& id, const juce::String& label, int dest = 0);

    FourfoldProcessor& proc;
    std::function<void (Knob&)> registerKnob;
    juce::OwnedArray<FmCell> cells;          // 36 matrix + 6 outputs
    juce::OwnedArray<Knob> knobs;
    std::vector<Knob*> globalKnobs;
    std::array<std::vector<Knob*>, numFmOps> opKnobs;
    juce::OwnedArray<juce::ComboBox> waveBoxes;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    juce::OwnedArray<FmEnvView> envViews;
    juce::ToggleButton onBtn { "FM ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    juce::ComboBox algoBox;
    juce::TextButton clearBtn { "Clear routing" };
    juce::Label help;
    juce::Rectangle<int> matrixArea, gridArea, globalArea, opsArea;
};
} // namespace ff::gui
