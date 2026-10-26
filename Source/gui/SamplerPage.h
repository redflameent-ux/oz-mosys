#pragma once
// The SAMPLER page: library browser (drop folders on the left), 4x4 pads and the mini mapper.
// The pads and mapper are carried over from Sol Intent.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../sampler/SamplerEngine.h"
#include "Widgets.h"
#include "SlicerPanel.h"

namespace ui
{
    // Sol Intent's colour names, pointed at the VIBRA-VII palette
    const juce::Colour bg       = ff::gui::col::bg;
    const juce::Colour panel    = ff::gui::col::panel;
    const juce::Colour panelHi  = ff::gui::col::panelHi;
    const juce::Colour line     = ff::gui::col::border;
    const juce::Colour gold     = ff::gui::col::sampler;
    const juce::Colour goldDim  = ff::gui::col::sampler.withMultipliedBrightness (0.65f);
    const juce::Colour violet   = ff::gui::col::granular;
    const juce::Colour teal     = ff::gui::col::wavetable;
    const juce::Colour text     = ff::gui::col::text;
    const juce::Colour textDim  = ff::gui::col::dim;
}

using SamplerHost = ff::SamplerEngine;

// ---------------------------------------------------------------------------- 4x4 pads
class PadGrid : public juce::Component
{
public:
    explicit PadGrid (SamplerHost& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setArticulation (const sol::Articulation* a, sol::InstrumentPtr keepAlive) { art = a; inst = std::move (keepAlive); repaint(); }
private:
    int padAt (juce::Point<float>) const;
    juce::Rectangle<float> padBounds (int pad) const;
    void release();
    SamplerHost& proc;
    sol::InstrumentPtr inst;
    const sol::Articulation* art = nullptr;
    int heldNote = -1;
};

// ---------------------------------------------------------------------------- MAP screen
class MapView : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit MapView (SamplerHost&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void setTarget (const sol::InstrumentInfo&, int articulation);
    const sol::InstrumentInfo& getTarget() const { return info; }
    void addFiles (const juce::StringArray& paths);
    void refreshLastKey();

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    sol::ArticulationSpec* currentArt();
    sol::ZoneSpec* currentZone();
    void selectZone (int);
    void changed();                 // saves + reloads shortly after the last edit
    void refreshControls();
    void refreshArticulationBox();
    void askForName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done);
    juce::Rectangle<int> mapArea() const;
    int keyAtX (float x) const;
    float xForKey (int key) const;

    SamplerHost& proc;
    sol::InstrumentInfo info;
    sol::InstrumentSpec spec;
    int art = 0, selected = -1;
    bool editable = false, updating = false;
    int viewLo = 36, viewHi = 84;

    juce::ComboBox artBox;
    juce::TextButton addArt { "+ Articulation" }, renameArt { "Rename" }, deleteArt { "Delete" }, importButton { "Import files..." };
    juce::ListBox zoneList { "Zones", this };
    juce::Slider root, lo, hi, velLo, velHi, gain;
    juce::Label rootL, loL, hiL, velLoL, velHiL, gainL;
    juce::TextButton setRoot { "Root = last key" }, playZone { "Play" }, autoRanges { "Auto ranges" }, removeZone { "Remove" };
    std::unique_ptr<juce::FileChooser> chooser;
};


// ---------------------------------------------------------------------------- the page
class FourfoldProcessor;

class SamplerPage : public juce::Component, public juce::FileDragAndDropTarget,
                    private juce::ListBoxModel, private juce::Timer
{
public:
    SamplerPage (FourfoldProcessor&, juce::AudioProcessorValueTreeState&);
    ~SamplerPage() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;
    void refreshList();
    void refreshInstrumentViews();
    void chooseLibraryFolder();
    void newInstrument (std::function<void (juce::File)> then = nullptr);
    void rebuildArticulationButtons();
    void importFolderAsInstrument (const juce::StringArray& files);
    void makeSureItPlays();

    struct Row { bool header; juce::String label; int index; };
    std::vector<Row> rows;

    FourfoldProcessor& owner;
    SamplerHost& proc;
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton libraryButton { "Library..." }, rescanButton { "Rescan" }, newButton { "+ New" },
                     octDown { "-" }, octUp { "+" };
    juce::ListBox browser { "Instruments", this };
    PadGrid pads { proc };
    MapView mapView { proc };
    juce::Label title, subtitle, statusLabel, padRangeLabel;
    juce::OwnedArray<juce::TextButton> artButtons;

    juce::ComboBox playMode;
    juce::ToggleButton throughFx { "THROUGH FX" }, letRing { "LET RING" }, choke { "CHOKE" };
    juce::TextButton sliceButton { "Slice a loop..." };
    std::unique_ptr<ff::gui::SlicerPanel> slicer;
    std::vector<std::unique_ptr<ff::gui::Knob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;

    sol::InstrumentPtr shown;
    int shownArt = -1;
    bool dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> leftArea, centreArea, rightArea, knobArea;
};
