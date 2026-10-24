#pragma once
#include "PluginProcessor.h"
#include "gui/Widgets.h"
#include "gui/Displays.h"
#include "gui/SamplerPage.h"
#include "gui/RollPage.h"
#include "gui/EnvelopePage.h"
#include "gui/BrowsePage.h"
#include "gui/PerformBar.h"
#include "gui/MidiMenu.h"
#include "gui/FmPage.h"
#include "gui/PatchPage.h"

class FourfoldEditor : public juce::AudioProcessorEditor,
                       public juce::DragAndDropContainer,
                       private juce::Timer
{
public:
    explicit FourfoldEditor (FourfoldProcessor&);
    ~FourfoldEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Knob = ff::gui::Knob;
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;

    Knob* makeKnob (std::vector<std::unique_ptr<Knob>>& group, const juce::String& id, const juce::String& label,
                    juce::Colour accent, int modDest = 0);
    juce::ComboBox& makeCombo (juce::ComboBox& box, const juce::String& id, const juce::StringArray& items);
    juce::ToggleButton& makeToggle (juce::ToggleButton& b, const juce::String& id, const juce::String& text, juce::Colour accent);
    static void layoutGrid (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> area, int cols, int startIndex = 0, int count = -1);

    void assignModulation (int source, int dest);
    void showModMenuFor (Knob& k);
    void setParam (const juce::String& id, float realValue);

    void refreshPresetList();
    void presetChosen();
    void stepPreset (int delta);
    void savePreset();
    void loadWavetable();
    void loadGrainSample();
    void loadGrainFile (const juce::File& f);

    FourfoldProcessor& proc;
    ff::gui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    struct Panel { juce::Rectangle<int> r; juce::String title; juce::Colour accent; };
    std::vector<Panel> panels;

    // top bar
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" };
    std::vector<std::unique_ptr<Knob>> topKnobs;
    juce::Rectangle<int> meterArea, logoArea;
    std::vector<juce::File> userPresetFiles;
    bool ignorePresetChange = false;

    // wavetable
    juce::ToggleButton wtOn;
    juce::ComboBox wtTable, wtWarp;
    juce::TextButton wtLoad { "Import" };
    ff::gui::WavetableDisplay wtDisplay;
    std::vector<std::unique_ptr<Knob>> wtKnobs;

    // analog
    juce::ToggleButton anOn;
    ff::gui::AnalogDisplay anDisplay;
    std::vector<std::unique_ptr<Knob>> anKnobs;

    // granular
    juce::ToggleButton grOn;
    juce::TextButton grLoad { "Load Sample" };
    juce::ComboBox grSource;
    ff::gui::GrainDisplay grDisplay;
    std::vector<std::unique_ptr<Knob>> grKnobs;

    // resonator
    juce::ToggleButton resOn;
    juce::ComboBox resType;
    ff::gui::ResonatorDisplay resDisplay;
    std::vector<std::unique_ptr<Knob>> resKnobs;

    // tuning / voice
    juce::ToggleButton monoToggle;
    juce::ComboBox scaleBox, rootBox;
    std::vector<std::unique_ptr<Knob>> tuneKnobs;

    // filter + envelopes + lfos
    juce::ToggleButton fltOn;
    juce::ComboBox fltType;
    std::vector<std::unique_ptr<Knob>> fltKnobs, ampKnobs, e2Knobs, e3Knobs, lfoKnobs, lfoFadeKnobs;
    juce::ComboBox lfoShape[2];
    juce::ToggleButton lfoRetrig[2];

    // macros + matrix
    std::unique_ptr<ff::gui::ModStrip> modStrip;
    struct MatrixRow
    {
        juce::ComboBox src, dst;
        juce::Slider amt;
        juce::TextButton clear { "x" };
        std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA;
        std::unique_ptr<APVTS::SliderAttachment> amtA;
    };
    std::array<MatrixRow, ff::numModSlots> rows;

    // fx
    std::vector<std::unique_ptr<Knob>> fxKnobs;
    juce::ComboBox driveType;

    juce::MidiKeyboardComponent keyboard;

    // pages
    juce::TextButton synthTab { "SYNTH" }, patchTab { "PATCH" }, fmTab { "FM" }, envTab { "ENV" }, browseTab { "BROWSE" }, samplerTab { "SAMPLER" }, rollTab { "ROLL" };
    std::unique_ptr<ff::gui::EnvelopePage> envPage;
    std::unique_ptr<ff::gui::FmPage> fmPage;
    std::unique_ptr<ff::gui::PatchPage> patchPage;
    void registerPageKnob (Knob& k);
    std::unique_ptr<ff::gui::BrowsePage> browsePage;
    std::unique_ptr<ff::gui::MacroBar> macroBar;
    std::unique_ptr<SamplerPage> samplerPage;
    std::unique_ptr<ff::gui::RollPage> rollPage;
    void showPage (int page);   // 0 synth, 1 patch, 2 fm, 3 env, 4 browse, 5 sampler, 6 roll

    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAttachments;
    std::vector<Knob*> allKnobs;
    std::unique_ptr<juce::FileChooser> chooser;
    int midiTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourfoldEditor)
};
