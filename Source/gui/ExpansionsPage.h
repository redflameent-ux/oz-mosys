#pragma once
// EXPANSIONS: every preset collection as a cover-art tile. Install packs (.ozpack),
// build your own pack from a bank of your presets, click a tile to browse it.
#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../design/Packs.h"

class FourfoldProcessor;

namespace ff::gui
{
class ExpansionsPage : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit ExpansionsPage (FourfoldProcessor&);
    ~ExpansionsPage() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void refresh();

    std::function<void (const juce::String& bank)> onOpenBank;
    std::function<void()> onClose, onPacksChanged;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    struct Tile;
    struct Grid;
    struct MakePackPanel;

private:
    void installFile (const juce::File&);
    void layoutTiles();
    void openMakePack();

    FourfoldProcessor& proc;
    std::vector<packs::Pack> all;
    juce::OwnedArray<juce::TextButton> styleChips;
    juce::String styleFilter { "All" };
    juce::TextEditor search;
    juce::TextButton backBtn { "< Presets" }, installBtn { "Install Pack..." }, makeBtn { "Make a Pack..." }, omniBtn { "Import Omnisphere..." };
    struct ImportThread;
    std::unique_ptr<ImportThread> importer;
    void startOmniImport (const juce::Array<juce::File>& sources);
    juce::Viewport viewport;
    std::unique_ptr<Grid> grid;
    std::unique_ptr<MakePackPanel> makePanel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Label status;
    bool dragOver = false;
};
} // namespace ff::gui
