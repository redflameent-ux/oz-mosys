#pragma once
// Wave editor (Absynth 5 wave page / FM8 spectrum style): draw a start and an end waveform,
// or set their harmonics with bars; OZ-mosys morphs between them and plays the result
// as a wavetable (sweep it with the Position knob).
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"

class FourfoldProcessor;

namespace ff::gui
{
class WaveCanvas : public juce::Component
{
public:
    static constexpr int size = 2048, numHarmonics = 32;
    std::vector<float> wave = std::vector<float> ((size_t) size, 0.0f);
    std::array<float, numHarmonics> harm {};
    bool harmonicMode = false;
    std::function<void()> onEdit;

    void setSine();
    void fromHarmonics();
    void toHarmonics();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void drawTo (juce::Point<float>);
    juce::Point<float> last;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (12.0f, 10.0f); }
};

class WaveDrawPage : public juce::Component
{
public:
    explicit WaveDrawPage (FourfoldProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;

private:
    void selectFrame (int which);
    void apply();
    void grabCurrent();
    WaveCanvas& canvas() { return frames[(size_t) editing]; }
    std::vector<std::vector<float>> buildTable (int numFrames) const;

    FourfoldProcessor& proc;
    std::array<WaveCanvas, 2> frames;
    int editing = 0;
    juce::TextButton startBtn { "Start wave" }, endBtn { "End wave" }, drawBtn { "Draw" }, harmBtn { "Harmonics" },
                     sineBtn { "Sine" }, sawBtn { "Saw" }, squareBtn { "Square" }, triBtn { "Triangle" }, noiseBtn { "Noise" },
                     smoothBtn { "Smooth" }, normBtn { "Normalize" }, copyBtn { "Copy to other" }, grabBtn { "Grab current" },
                     applyBtn { "Play it" }, closeBtn { "< Back" };
    juce::ComboBox morphBox, framesBox;
    juce::TextEditor nameEd;
    juce::Label status, morphL, framesL, nameL;
    juce::Rectangle<int> previewArea;
};
} // namespace ff::gui
