#pragma once
// The BROWSER page (Absynth-style):
//   top    - filters | preset list (or the sound map "Explorer") | properties + subpresets
//   bottom - Module Lock | Preset Mutation | Fine Tuning | Mutation History
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../design/FineTune.h"
#include "ExpansionsPage.h"
#include "../design/SoundTypes.h"
#include <set>
#include <map>

class FourfoldProcessor;

namespace ff::gui
{
class BrowsePage;

class SoundMap : public juce::Component
{
public:
    explicit SoundMap (BrowsePage& p) : page (p) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int hover = -1;
private:
    juce::Point<float> posOf (int entry) const;
    int entryAt (juce::Point<float>) const;
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (30.0f, 24.0f); }
    BrowsePage& page;
};

// Module Lock cell: click = lock / unlock, drag sideways = how much this section may change.
class LockCell : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LockCell (const juce::String& n) : name (n)
    {
        setTooltip ("Click to lock (never changes). Drag left/right to set how much it may change. Double-click: 100%");
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }
    bool locked = false;
    float amount = 1.0f;
    std::function<void()> onToggle;
    std::function<void (float)> onAmount;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (locked ? col::sampler.withAlpha (0.28f) : col::panelHi);
        g.fillRoundedRectangle (r, 4.0f);
        if (! locked)
        {
            auto bar = r.reduced (4.0f).removeFromBottom (3.0f);
            g.setColour (col::bg);
            g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (col::mod.withAlpha (0.85f));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * amount), 1.5f);
        }
        g.setColour (locked ? col::sampler : col::text);
        g.setFont (font (11.5f, true));
        auto t = getLocalBounds().withTrimmedBottom (5);
        g.drawFittedText ((locked ? juce::String::charToString (0x25A0) + " " : juce::String()) + name, t.reduced (4, 0), juce::Justification::centred, 1);
        if (! locked && amount < 0.995f)
        {
            g.setColour (col::dim);
            g.setFont (font (9.5f));
            g.drawText (juce::String (juce::roundToInt (amount * 100)) + "%", t.reduced (5, 2), juce::Justification::topRight);
        }
    }
    void mouseDown (const juce::MouseEvent&) override { dragged = false; downAmount = amount; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (locked || std::abs (e.getDistanceFromDragStartX()) < 3) return;
        dragged = true;
        amount = juce::jlimit (0.0f, 1.0f, downAmount + (float) e.getDistanceFromDragStartX() / (float) juce::jmax (40, getWidth()));
        if (onAmount) onAmount (amount);
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragged && e.mouseWasClicked() && e.getNumberOfClicks() < 2 && onToggle) onToggle();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        amount = 1.0f;
        if (onAmount) onAmount (amount);
        repaint();
    }
private:
    juce::String name;
    bool dragged = false;
    float downAmount = 1.0f;
};

class BrowsePage : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit BrowsePage (FourfoldProcessor&);
    ~BrowsePage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    struct Entry
    {
        juce::String name, bank, author, tags, notes;
        juce::StringArray types, modes;
        float x = 0.5f, y = 0.5f, movement = 0.5f;
        int factoryIndex = -1;
        juce::File file;
        bool measured = true;
    };
    std::vector<Entry> entries;
    std::vector<int> visible;            // entries passing the filters, in list order
    std::vector<juce::String> banks;
    std::set<juce::String> favourites;
    juce::Colour bankColour (const juce::String& bank) const;
    bool passes (const Entry&) const;
    int selected = -1;

    void audition (int entry);
    void keep (int entry);

private:
    // preset list
    int getNumRows() override { return (int) visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    void rebuild();
    void refilter();
    void rebuildBankButtons();
    void showInfo (int entry);
    void refreshMutation();
    void loadFavourites();
    void saveFavourites();
    void saveCurrent (bool asNew);
    void showLoadSectionMenu();

    FourfoldProcessor& proc;
    design::FineTune fineTune;

    // filters
    juce::OwnedArray<juce::TextButton> bankButtons;
    juce::Viewport bankView;
    juce::Component bankHolder;
    std::set<juce::String> hiddenBanks;
    juce::Slider brightRange, aggroRange, moveRange;
    juce::Label brightL, aggroL, moveL;
    juce::TextButton allBanks { "All" }, noBanks { "None" };

    // centre
    juce::TextButton expansionsBtn { "Expansions" };
    juce::OwnedArray<juce::TextButton> typeChips, modeChips;
    std::set<juce::String> typeFilter, modeFilter;
    static constexpr int chipsH = 50;
    std::unique_ptr<ExpansionsPage> expansions;
    std::map<juce::String, juce::Image> bankCovers;
    juce::Rectangle<int> coverArea;
    void showExpansions (bool);
    juce::TextButton explorerBtn { "Explorer" }, favOnlyBtn { "Favourites" }, auditionBtn { "Audition" };
    juce::TextEditor search;
    juce::ListBox list { "Presets", this };
    SoundMap map { *this };
    juce::Label countL;

    // properties
    juce::TextEditor nameEd, authorEd, bankEd, tagsEd, notesEd;
    juce::Label nameL, authorL, bankL, tagsL, notesL, statusL;
    juce::TextButton saveBtn { "Save" }, saveAsBtn { "Save As" }, revertBtn { "Revert" };
    juce::ComboBox sectionBox;
    juce::TextButton secSave { "Save" }, secLoad { "Load" }, secCopy { "Copy" }, secPaste { "Paste" }, secInit { "Init" }, secRandom { "Rand" };

    // mutation
    juce::OwnedArray<LockCell> lockButtons;
    juce::Slider mutAmount, randAmount;
    juce::Label mutL, randL, targetL;
    juce::TextButton mutateBtn { "Mutate" }, retryBtn { "Retry" }, histBack { "<" }, histSave { "Save" };
    juce::OwnedArray<juce::Slider> fineKnobs;
    juce::OwnedArray<juce::Label> fineLabels;
    juce::ListBox history;
    struct HistoryModel : juce::ListBoxModel
    {
        BrowsePage* page = nullptr;
        int getNumRows() override;
        void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
        void listBoxItemClicked (int, const juce::MouseEvent&) override;
    } historyModel;
    friend struct HistoryModel;

    juce::MemoryBlock beforeBrowsing;
    juce::String beforeName;
    bool haveBefore = false, explorer = false;
    double previewOffAt = 0;
    juce::String shownPreset;
    int shownHistory = -2;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> leftArea, centreArea, rightArea, lockArea, mutArea, fineArea, histArea;
};
} // namespace ff::gui
