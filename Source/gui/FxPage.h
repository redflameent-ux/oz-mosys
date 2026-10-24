#pragma once
// The FX page (Guitar Rig style): component browser | rack of 8 units | signal flow.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../dsp/Rack.h"

class FourfoldProcessor;

namespace ff::gui
{
class FxPage;

class RackSlotView : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    RackSlotView (FxPage&, FourfoldProcessor&, int index, std::function<void (Knob&)> reg);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    int type() const;
    const int index;

private:
    FxPage& page;
    FourfoldProcessor& proc;
    juce::OwnedArray<Knob> knobs;     // A B C D Mix
    juce::ToggleButton onBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    juce::TextButton upBtn { "^" }, downBtn { "v" }, removeBtn { "x" }, irBtn { "Load IR..." };
    juce::Label nameL, irL;
    bool dropHover = false;
    int shownType = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

class FxPage : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    FxPage (FourfoldProcessor&, std::function<void (Knob&)> registerKnob);
    ~FxPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void setUnit (int slot, int type);       // puts a unit in a slot with its default settings
    void addUnit (int type);                 // first empty slot
    void moveSlot (int from, int to);
    void clearSlot (int slot);
    static juce::Colour categoryColour (const juce::String& cat);
    juce::String hoverHelp;

private:
    int getNumRows() override { return (int) shownUnits.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override;
    void listBoxItemClicked (int, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    void timerCallback() override;
    void showCategory (const juce::String& cat);
    void setParam (const juce::String& id, float real);

    FourfoldProcessor& proc;
    juce::OwnedArray<juce::TextButton> catButtons;
    juce::String category;
    std::vector<int> shownUnits;
    juce::ListBox unitList { "Units", this };
    juce::OwnedArray<RackSlotView> slots;
    juce::Component rackHolder;
    juce::Viewport rackView;
    juce::ToggleButton splitBtn { "Split Low / High" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> splitAtt;
    std::unique_ptr<Knob> xoverKnob;
    juce::TextButton clearAllBtn { "Clear rack" };
    juce::Rectangle<int> browserArea, rackArea, flowArea, flowBox;
    std::array<int, numRackSlots> slotTypesSeen = [] { std::array<int, numRackSlots> a {}; a.fill (-1); return a; }();
};
} // namespace ff::gui
