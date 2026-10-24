#pragma once
// The PLAY page: Chords tool (Kontakt-style) + 32-step arpeggiator with FM8-style rows.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../perform/Perform.h"

class FourfoldProcessor;

namespace ff::gui
{
class ArpGrid : public juce::Component
{
public:
    explicit ArpGrid (FourfoldProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int currentStep = -1;
    static constexpr int numRows = 7;
    static const char* rowName (int r);
    void resetRow (int r);
    void randomRow (int r);
    juce::Rectangle<float> rowArea (int r) const;
private:
    juce::Rectangle<float> cell (int row, int step) const;
    void edit (const juce::MouseEvent&, bool first);
    void commit();
    FourfoldProcessor& proc;
    perf::ArpPattern working;
    bool haveWorking = false, paintValue = true;
    int dragRow = -1, lastStep = -1;
    float dragStartY = 0; int dragStartVal = 0;
    juce::Random rng;
};

class PlayPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    PlayPage (FourfoldProcessor&, std::function<void (Knob&)> registerKnob);
    ~PlayPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    Knob* knob (const juce::String& id, const juce::String& label, juce::Colour c);
    juce::ComboBox* combo (const juce::String& id, const juce::StringArray& items);
    juce::ToggleButton* toggle (const juce::String& id, const juce::String& text, juce::Colour c);
    void importGroove (const juce::File&);
    int degreeRoot (int degree) const;

    FourfoldProcessor& proc;
    std::function<void (Knob&)> registerKnob;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::Label> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    ArpGrid grid;
    juce::ComboBox patternBox;
    juce::OwnedArray<juce::TextButton> rowReset, rowRandom;
    juce::Rectangle<int> chordArea, arpArea, circlesArea, gridArea;
    std::array<juce::Rectangle<float>, 7> circles;
    std::vector<std::pair<juce::Component*, juce::Rectangle<int>*>> layout;
    int auditionNote = -1; double auditionOff = 0;
    bool dragOver = false;
    juce::String status;
};
} // namespace ff::gui
