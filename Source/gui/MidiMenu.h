#pragma once
// Right-click menu items for MIDI learn, and the "all MIDI links" panel.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../design/MidiLearn.h"

namespace ff::gui
{
class MidiMapPanel : public juce::Component, private juce::ListBoxModel
{
public:
    explicit MidiMapPanel (design::MidiLearn& m) : learn (m)
    {
        list.setModel (this);
        list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, col::bg);
        addAndMakeVisible (list);
        clearBtn.onClick = [this] { learn.clearAll(); refresh(); };
        addAndMakeVisible (clearBtn);
        title.setText ("MIDI LINKS - right-click any knob and pick MIDI Learn, then move a knob on your controller",
                       juce::dontSendNotification);
        title.setFont (font (11.0f, true));
        title.setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (title);
        refresh();
        setSize (420, 320);
    }
    void refresh() { rows = learn.assignments(); list.updateContent(); list.repaint(); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (34));
        clearBtn.setBounds (r.removeFromBottom (26).removeFromRight (100));
        r.removeFromBottom (6);
        list.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (col::panel); }

private:
    int getNumRows() override { return juce::jmax (1, (int) rows.size()); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
    {
        g.setFont (font (12.5f));
        if (rows.empty())
        {
            g.setColour (col::dim);
            g.drawText ("Nothing linked yet", 8, 0, w - 16, h, juce::Justification::centredLeft);
            return;
        }
        const auto& a = rows[(size_t) row];
        if (row % 2) { g.setColour (juce::Colour (0xff101218)); g.fillRect (0, 0, w, h); }
        g.setColour (col::mod);
        g.drawText (design::MidiLearn::ccName (a.cc), 8, 0, 120, h, juce::Justification::centredLeft);
        g.setColour (col::text);
        g.drawText (a.name, 130, 0, w - 200, h, juce::Justification::centredLeft, true);
        g.setColour (col::filter);
        g.drawText ("remove", w - 64, 0, 56, h, juce::Justification::centredRight);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (row < (int) rows.size() && e.x > list.getWidth() - 80) { learn.forgetCC (rows[(size_t) row].cc); refresh(); }
    }

    design::MidiLearn& learn;
    std::vector<design::MidiLearn::Assignment> rows;
    juce::ListBox list;
    juce::TextButton clearBtn { "Clear all" };
    juce::Label title;
};

// Adds MIDI learn items (ids 9001..9003) to a knob's menu.
inline void addMidiItems (juce::PopupMenu& menu, design::MidiLearn& learn, const juce::String& paramId)
{
    menu.addSectionHeader ("MIDI");
    const int cc = learn.ccFor (paramId);
    if (learn.isLearning (paramId)) menu.addItem (9002, "Cancel MIDI Learn");
    else menu.addItem (9001, cc >= 0 ? "MIDI Learn (now " + design::MidiLearn::ccName (cc) + ")" : "MIDI Learn - then move a knob on your controller");
    if (cc >= 0) menu.addItem (9003, "Forget " + design::MidiLearn::ccName (cc));
    menu.addItem (9004, "Show all MIDI links...");
}

inline bool handleMidiItem (int result, design::MidiLearn& learn, const juce::String& paramId, juce::Component* anchor)
{
    switch (result)
    {
        case 9001: learn.learn (paramId); return true;
        case 9002: learn.cancelLearn(); return true;
        case 9003: learn.forget (paramId); return true;
        case 9004:
            if (anchor != nullptr)
                juce::CallOutBox::launchAsynchronously (std::make_unique<MidiMapPanel> (learn), anchor->getScreenBounds(), nullptr);
            return true;
        default: return false;
    }
}
} // namespace ff::gui
