#pragma once
// MORPH + ORB: an FM8-style morph square between four corner sounds, and an
// Omnisphere-style Orb that moves the puck on its own.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include <deque>

class FourfoldProcessor;

namespace ff::gui
{
class MorphPad : public juce::Component
{
public:
    explicit MorphPad (FourfoldProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Rectangle<float> square() const;
    static juce::Colour cornerColour (int i);
private:
    juce::Point<float> toXY (juce::Point<float>) const;
    FourfoldProcessor& proc;
    std::deque<juce::Point<float>> trail;
    juce::Point<float> lastPos; double lastT = 0; juce::Point<float> vel;
    friend class MorphPage;
};

class MorphPage : public juce::Component, private juce::Timer
{
public:
    MorphPage (FourfoldProcessor&, std::function<void (Knob&)> registerKnob);
    ~MorphPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;

private:
    void timerCallback() override;
    void fillVariations();
    FourfoldProcessor& proc;
    MorphPad pad;
    juce::TextButton closeBtn { "< Back to PLAY" }, fillBtn { "Make B C D from this sound" }, clearBtn { "Clear corners" }, recBtn { "Record motion" };
    juce::OwnedArray<juce::TextButton> setBtns, diceBtns;
    juce::ToggleButton onToggle { "MORPH" };
    juce::ComboBox orbMode;
    juce::Slider varAmount;
    juce::Label varL, orbL, help;
    juce::OwnedArray<Knob> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
    juce::Rectangle<int> side;
};
} // namespace ff::gui
