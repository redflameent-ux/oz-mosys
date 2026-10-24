#pragma once
// The ENVELOPE page: four breakpoint envelopes (MSEG 1-4), listed on the left and
// stacked on the right. Points, curves, sustain, loop, tempo sync, an LFO overlay and
// sample & hold - and a target picker that wires the envelope into the mod matrix.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../dsp/Mseg.h"

class FourfoldProcessor;

namespace ff::gui
{
class MsegEditor : public juce::Component
{
public:
    explicit MsegEditor (FourfoldProcessor&, int index);
    void reload();
    int getIndex() const { return index; }
    MsegShape shape;
    bool selected = false, compact = false;
    double gridStep = 0.0;               // snap step (seconds or beats), 0 = free
    int brush = 5;                       // stepper: shape painted by clicking a step
    std::function<void()> onChange, onSelect;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void commit();
    juce::Rectangle<float> plot() const;
    float xForTime (double t) const;
    double timeForX (float x) const;
    float yForLevel (float l) const;
    float levelForY (float y) const;
    int pointAt (juce::Point<float>) const;
    int segmentAt (juce::Point<float>) const;
    double viewLength() const;
    void paintSteps (juce::Graphics&);
    void stepMouse (const juce::MouseEvent&, bool isDown);
    int stepAt (float x) const;
    int lastStep = -1;

    FourfoldProcessor& proc;
    const int index;
    int dragPoint = -1, dragSegment = -1, hoverPoint = -1;
    float downCurve = 0.0f;
    juce::Point<float> downPos;
    double frozenLength = 0.0;
};

class EnvelopePage : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit EnvelopePage (FourfoldProcessor&);
    ~EnvelopePage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    int getNumRows() override { return 4; }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    void select (int i);
    void reloadAll();
    void refreshControls();
    void applyToSelected (std::function<void (MsegShape&)> change);
    void loadShape (int which);
    void assignTarget();
    juce::String targetsOf (int mseg) const;

    FourfoldProcessor& proc;
    int current = 0;
    juce::ListBox list { "Envelopes", this };
    juce::OwnedArray<MsegEditor> editors;
    juce::TextButton stepBtn { "Performer" };
    juce::ComboBox stepsBox, stepLenBox;
    juce::Label stepsL, stepLenL, brushL;
    juce::OwnedArray<juce::Component> brushes;
    int brush = 5;
    juce::TextButton stackBtn { "Stacked" }, syncBtn { "Sync" }, lfoBtn { "LFO" }, shBtn { "S&H" }, shapesBtn { "Shapes" },
                     assignBtn { "Assign" };
    juce::ComboBox modeBox, gridBox, targetBox;
    juce::Slider rateSlider, lfoRate, lfoDepth, shRate, amount;
    juce::Label modeL, rateL, gridL, lfoRateL, lfoDepthL, shRateL, targetL, amountL, help, targetsInfo;
    juce::String shownPreset;
    bool stacked = true, updating = false;
    juce::Rectangle<int> listArea, mainArea, headerArea;
};
} // namespace ff::gui
