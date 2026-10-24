#pragma once
// Loop slicer (Kontakt Leap-style): drop a loop, cut it at its hits (or an even grid),
// and each slice lands on its own pad / key as a new sampler instrument.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"

class FourfoldProcessor;

namespace ff::gui
{
class SlicerPanel : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit SlicerPanel (FourfoldProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    std::function<void()> onClose;
    std::function<void (const juce::File& instrumentFolder)> onMade;

    void loadFile (const juce::File&);
    void makePads();

private:
    void detect();
    juce::Rectangle<float> waveArea() const;
    float xFor (juce::int64 sample) const;
    juce::int64 sampleFor (float x) const;

    FourfoldProcessor& proc;
    juce::AudioBuffer<float> audio;
    double fileRate = 44100.0;
    juce::File source;
    std::vector<juce::int64> slices;
    std::vector<float> peaks;
    juce::TextButton loadBtn { "Load loop..." }, makeBtn { "Make pads" }, closeBtn { "< Back" };
    juce::ComboBox modeBox;
    juce::Slider sensitivity;
    juce::Label sensL, modeL, info, nameL;
    juce::TextEditor nameEd;
    std::unique_ptr<juce::FileChooser> chooser;
    int dragSlice = -1;
    bool dragOver = false;
    double bpmGuess = 0.0;
};
} // namespace ff::gui
