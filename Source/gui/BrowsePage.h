#pragma once
// The BROWSER page (Absynth-style):
//   top    - filters | preset list (or the sound map "Explorer") | properties + subpresets
//   bottom - Module Lock | Preset Mutation | Fine Tuning | Mutation History
#include <juce_audio_utils/juce_audio_utils.h>
#include "Widgets.h"
#include "../design/FineTune.h"
#include "ExpansionsPage.h"
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
    juce::OwnedArray<juce::TextButton> lockButtons;
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
