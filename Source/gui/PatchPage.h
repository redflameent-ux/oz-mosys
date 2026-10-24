#pragma once
// The PATCH page (Absynth-style signal flow): five engine channels, each with two module
// slots, flowing into the mod oscillator / noise / feedback and the dual filter.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"

class FourfoldProcessor;

namespace ff::gui
{
class PatchPage : public juce::Component, private juce::Timer
{
public:
    PatchPage (FourfoldProcessor&, std::function<void (Knob&)> registerKnob);
    ~PatchPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    Knob* knob (const juce::String& id, const juce::String& label, juce::Colour c, int dest = 0);
    juce::ComboBox* combo (const juce::String& id, const juce::StringArray& items);
    juce::ToggleButton* toggle (const juce::String& id, const juce::String& text, juce::Colour c);

    struct Channel
    {
        juce::Colour colour;
        juce::String name;
        juce::ToggleButton* on = nullptr;
        Knob* level = nullptr;
        Knob* pan = nullptr;
        juce::ComboBox* slotType[2] {};
        Knob* slotKnob[2][3] {};
        juce::Rectangle<int> area, slotArea[2];
    };

    FourfoldProcessor& proc;
    std::function<void (Knob&)> registerKnob;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::ToggleButton> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::array<Channel, 5> channels;

    // bottom row
    std::vector<Knob*> moKnobs, noiseKnobs, f1Knobs, f2Knobs, voiceKnobs;
    juce::ComboBox *moTarget = nullptr, *noiseColour = nullptr, *f1Type = nullptr, *f2Type = nullptr, *route = nullptr,
                   *glideCurve = nullptr, *velCurve = nullptr;
    juce::ToggleButton *f1On = nullptr, *f2On = nullptr;
    juce::Rectangle<int> moArea, noiseArea, filterArea, voiceArea, mixLine;
};
} // namespace ff::gui
