#include "ExpansionsPage.h"
#include "../import/OmniImport.h"
#include "../import/ArturiaImport.h"
#include "../import/NIImport.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace ff::gui
{
static Colour accentFor (const String& name)
{
    const auto h = (uint32) name.hashCode();
    return Colour::fromHSV ((float) (h % 1000) / 1000.0f, 0.6f, 0.95f, 1.0f);
}

// ============================================================================ tile
struct ExpansionsPage::Tile : public Component
{
    Tile (ExpansionsPage& o, const packs::Pack& p) : owner (o), pack (p) { setMouseCursor (MouseCursor::PointingHandCursor); }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto art = r.removeFromTop (r.getWidth());
        if (pack.cover.isValid())
        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addRoundedRectangle (art, 6.0f);
            g.reduceClipRegion (clip);
            g.drawImage (pack.cover, art, RectanglePlacement::fillDestination);
        }
        else
        {
            packs::drawCover (g, art, pack.name, accentFor (pack.name));
            g.setColour (Colours::white.withAlpha (0.9f));
            g.setFont (font (art.getWidth() * 0.1f, true));
            g.drawFittedText (pack.name.toUpperCase(), art.reduced (10).toNearestInt().removeFromBottom ((int) (art.getHeight() * 0.4f)),
                              Justification::bottomLeft, 2);
        }
        if (hover)
        {
            g.setColour (Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (art, 6.0f);
            g.setColour (Colours::white.withAlpha (0.7f));
            g.drawRoundedRectangle (art.reduced (0.5f), 6.0f, 1.2f);
        }
        r.removeFromTop (6);
        g.setColour (col::text);
        g.setFont (font (13.5f, true));
        g.drawText (pack.name, r.removeFromTop (18).toNearestInt(), Justification::centredLeft, true);
        g.setColour (col::dim);
        g.setFont (font (11.5f));
        g.drawText (pack.tagline, r.removeFromTop (16).toNearestInt(), Justification::centredLeft, true);
        r.removeFromTop (4);
        auto badge = r.removeFromTop (18);
        const String b = pack.builtIn ? "BUILT IN" : "INSTALLED";
        const auto bc = pack.builtIn ? col::mod : col::sampler;
        const float bw = GlyphArrangement::getStringWidth (font (10.0f, true), b) + 14.0f;
        g.setColour (bc.withAlpha (0.18f));
        g.fillRoundedRectangle (badge.withWidth (bw), 3.0f);
        g.setColour (bc);
        g.setFont (font (10.0f, true));
        g.drawText (b, badge.withWidth (bw).toNearestInt(), Justification::centred);
        g.setColour (col::dim);
        g.setFont (font (11.0f));
        g.drawText (String (pack.presetCount) + " presets", badge.withTrimmedLeft (bw + 8).toNearestInt(), Justification::centredLeft);
    }
    void mouseEnter (const MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            PopupMenu m;
            m.addItem (1, "Browse these presets");
            if (! pack.builtIn)
            {
                m.addItem (2, "Show in folder");
                m.addSeparator();
                m.addItem (3, "Remove pack");
            }
            m.showMenuAsync (PopupMenu::Options(), [this] (int r)
            {
                if (r == 1 && owner.onOpenBank) owner.onOpenBank (pack.name);
                if (r == 2) pack.folder.revealToUser();
                if (r == 3 && packs::remove (pack))
                {
                    auto* o = &owner;
                    MessageManager::callAsync ([o] { o->refresh(); if (o->onPacksChanged) o->onPacksChanged(); });
                }
            });
            return;
        }
        if (e.mouseWasClicked() && owner.onOpenBank) owner.onOpenBank (pack.name);
    }

    ExpansionsPage& owner;
    packs::Pack pack;
    bool hover = false;
};

struct ExpansionsPage::Grid : public Component
{
    OwnedArray<Tile> tiles;
    String heading1, heading2;
    int split = -1, headingY2 = 0;
    void paint (Graphics& g) override
    {
        g.setColour (col::dim);
        g.setFont (font (11.0f, true));
        g.drawText (heading1, 0, 0, getWidth(), 18, Justification::centredLeft);
        if (heading2.isNotEmpty()) g.drawText (heading2, 0, headingY2, getWidth(), 18, Justification::centredLeft);
    }
};

// ============================================================================ make-a-pack panel
struct ExpansionsPage::MakePackPanel : public Component
{
    explicit MakePackPanel (ExpansionsPage& o) : owner (o)
    {
        for (auto* l : { &title, &nameL, &tagL, &authL, &styleL, &bankL, &coverL, &info })
        {
            l->setFont (font (11.0f, l == &title));
            l->setColour (Label::textColourId, l == &title ? col::text : col::dim);
            addAndMakeVisible (l);
        }
        title.setText ("MAKE A PACK", dontSendNotification);
        title.setFont (font (15.0f, true));
        nameL.setText ("Pack name", dontSendNotification);
        tagL.setText ("One-line description", dontSendNotification);
        authL.setText ("Author / label", dontSendNotification);
        styleL.setText ("Style", dontSendNotification);
        bankL.setText ("Presets from bank", dontSendNotification);
        coverL.setText ("Cover art (square works best)", dontSendNotification);
        for (auto* e : { &nameEd, &tagEd, &authEd }) { e->setFont (font (13.0f)); addAndMakeVisible (e); }
        authEd.setText (FourfoldProcessor::getDefaultAuthor(), false);
        styleBox.addItemList ({ "Core", "Electronic", "Urban", "Cinematic", "World", "Vintage", "Experimental" }, 1);
        styleBox.setSelectedId (2, dontSendNotification);
        addAndMakeVisible (styleBox);
        addAndMakeVisible (bankBox);
        bankBox.onChange = [this] { updateInfo(); if (nameEd.isEmpty()) nameEd.setText (bankBox.getText(), false); };
        for (auto* b : { &coverBtn, &buildBtn, &cancelBtn }) addAndMakeVisible (b);
        buildBtn.setColour (TextButton::buttonColourId, col::sampler.withAlpha (0.3f));
        coverBtn.onClick = [this]
        {
            owner.chooser = std::make_unique<FileChooser> ("Choose cover art", File::getSpecialLocation (File::userPicturesDirectory), "*.png;*.jpg;*.jpeg");
            owner.chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
            {
                if (fc.getResult().existsAsFile()) { cover = fc.getResult(); coverImg = ImageFileFormat::loadFrom (cover); repaint(); }
            });
        };
        cancelBtn.onClick = [this] { setVisible (false); };
        buildBtn.onClick = [this] { build(); };
        refreshBanks();
    }

    void refreshBanks()
    {
        byBank.clear();
        for (auto& d : RangedDirectoryIterator (FourfoldProcessor::getUserPresetFolder(), true, "*.ozpreset"))
        {
            String bank = "My Presets";
            if (auto xml = XmlDocument::parse (d.getFile())) bank = xml->getStringAttribute ("bank", bank);
            if (bank.isEmpty()) bank = "My Presets";
            byBank[bank].add (d.getFile());
        }
        bankBox.clear (dontSendNotification);
        int id = 1;
        for (auto& [b, files] : byBank) bankBox.addItem (b + "  (" + String (files.size()) + ")", id++);
        if (bankBox.getNumItems() > 0) bankBox.setSelectedId (1, dontSendNotification);
        updateInfo();
    }

    String selectedBank() const
    {
        int i = 0;
        for (auto& [b, f] : byBank) if (++i == bankBox.getSelectedId()) return b;
        return {};
    }

    void updateInfo()
    {
        if (byBank.empty())
            info.setText ("You have no saved presets yet. Save some sounds first, and set their Bank (in the browser's Properties) "
                          "to your collection's name - then make the pack from that bank.", dontSendNotification);
        else
            info.setText ("Every preset in the chosen bank goes into the pack. Send the .ozpack file to anyone with OZ-mosys: "
                          "they drop it on the Expansions page to install it.", dontSendNotification);
        buildBtn.setEnabled (! byBank.empty());
    }

    void build()
    {
        const auto bank = selectedBank();
        if (bank.isEmpty()) return;
        auto name = nameEd.getText().trim();
        if (name.isEmpty()) name = bank;
        auto files = byBank[bank];
        owner.chooser = std::make_unique<FileChooser> ("Save pack", File::getSpecialLocation (File::userDesktopDirectory).getChildFile (File::createLegalFileName (name) + ".ozpack"), "*.ozpack");
        owner.chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                                    [this, name, files] (const FileChooser& fc)
        {
            auto out = fc.getResult();
            if (out == File()) return;
            if (! out.hasFileExtension ("ozpack")) out = out.withFileExtension ("ozpack");
            String err;
            if (packs::makePack (out, name, tagEd.getText().trim(), authEd.getText().trim(), styleBox.getText(), cover, files, err))
            {
                owner.status.setText ("Made " + out.getFileName() + " (" + String (files.size()) + " presets). Drop it here to install it yourself too.", dontSendNotification);
                out.revealToUser();
                setVisible (false);
            }
            else owner.status.setText (err, dontSendNotification);
        });
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::sampler.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);
        auto c = coverArea.toFloat();
        if (coverImg.isValid())
        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addRoundedRectangle (c, 6.0f);
            g.reduceClipRegion (clip);
            g.drawImage (coverImg, c, RectanglePlacement::fillDestination);
        }
        else packs::drawCover (g, c, nameEd.getText().isEmpty() ? selectedBank() : nameEd.getText(), accentFor (nameEd.getText()));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18);
        title.setBounds (r.removeFromTop (24));
        r.removeFromTop (8);
        auto right = r.removeFromRight (200);
        r.removeFromRight (18);
        coverL.setBounds (right.removeFromTop (16));
        coverArea = right.removeFromTop (200);
        right.removeFromTop (6);
        coverBtn.setBounds (right.removeFromTop (26));
        auto field = [&r] (Label& l, Component& c)
        {
            l.setBounds (r.removeFromTop (16));
            c.setBounds (r.removeFromTop (26));
            r.removeFromTop (6);
        };
        field (bankL, bankBox);
        field (nameL, nameEd);
        field (tagL, tagEd);
        field (authL, authEd);
        field (styleL, styleBox);
        auto btns = r.removeFromBottom (30);
        cancelBtn.setBounds (btns.removeFromRight (90));
        btns.removeFromRight (8);
        buildBtn.setBounds (btns.removeFromRight (140));
        r.removeFromBottom (6);
        info.setBounds (r);
        info.setJustificationType (Justification::topLeft);
    }

    ExpansionsPage& owner;
    Label title, nameL, tagL, authL, styleL, bankL, coverL, info;
    TextEditor nameEd, tagEd, authEd;
    ComboBox styleBox, bankBox;
    TextButton coverBtn { "Choose Cover..." }, buildBtn { "Build Pack..." }, cancelBtn { "Cancel" };
    File cover;
    Image coverImg;
    Rectangle<int> coverArea;
    std::map<String, Array<File>> byBank;
};

// ============================================================================ page
struct ExpansionsPage::ImportThread : public Thread
{
    ImportThread (ExpansionsPage& o, int k, Array<File> f) : Thread ("OZ preset import"), owner (o), kind (k), sources (std::move (f)) {}
    void run() override
    {
        SafePointer<ExpansionsPage> sp (&owner);
        String summary;
        auto progress = [this, sp] (int done, int total, const String& name)
        {
            if (done % 25 == 0)
                MessageManager::callAsync ([sp, done, total, name]
                {
                    if (sp != nullptr) sp->status.setText ("Importing " + String (done) + " / " + String (total) + "  -  " + name, dontSendNotification);
                });
            return ! threadShouldExit();
        };
        switch (kind)
        {
            case 1:  ff::import::importArturia (sources, owner.proc.apvts, progress, summary); break;
            case 2:  ff::import::importNI (ff::import::NIKind::FM8, sources, owner.proc.apvts, progress, summary); break;
            case 3:  ff::import::importNI (ff::import::NIKind::Absynth, sources, owner.proc.apvts, progress, summary); break;
            default: ff::import::importOmnisphere (sources, owner.proc.apvts, progress, summary); break;
        }
        MessageManager::callAsync ([sp, summary]
        {
            if (sp == nullptr) return;
            sp->status.setText (summary + ". Find them under BROWSE, or as packs here.", dontSendNotification);
            sp->importBtn.setButtonText ("Import presets...");
            sp->refresh();
            if (sp->onPacksChanged) sp->onPacksChanged();
            sp->importer->stopThread (2000);
            sp->importer.reset();
        });
    }
    ExpansionsPage& owner;
    int kind = 0;
    Array<File> sources;
};

ExpansionsPage::ExpansionsPage (FourfoldProcessor& p) : proc (p)
{
    setOpaque (true);
    for (auto s : { "All", "Installed", "Core", "Electronic", "Urban", "Cinematic", "World", "Vintage", "Experimental" })
    {
        auto* b = styleChips.add (new TextButton (s));
        b->setClickingTogglesState (false);
        b->setColour (TextButton::buttonOnColourId, col::sampler.withAlpha (0.28f));
        b->onClick = [this, s] { styleFilter = s; for (auto* c : styleChips) c->setToggleState (c->getButtonText() == styleFilter, dontSendNotification); layoutTiles(); };
        b->setToggleState (styleFilter == s, dontSendNotification);
        addAndMakeVisible (b);
    }
    search.setTextToShowWhenEmpty ("Search packs", col::dim);
    search.setFont (font (13.0f));
    search.onTextChange = [this] { layoutTiles(); };
    addAndMakeVisible (search);
    for (auto* b : { &backBtn, &installBtn, &makeBtn, &importBtn }) addAndMakeVisible (b);
    importBtn.setColour (TextButton::buttonColourId, Colour (0xff7ee0c3).withAlpha (0.25f));
    importBtn.setTooltip ("Rebuild patches from Omnisphere, Arturia (Pigments / Analog Lab / V Collection), FM8 or Absynth as OZ-mosys presets. Each sound is swapped for the closest OZ engine. You can also drag preset files onto this page. They stay private on this computer.");
    importBtn.onClick = [this]
    {
        if (importer != nullptr) { importer->signalThreadShouldExit(); return; }

        // a good place to start browsing per synth (falls back to Documents)
        auto startDir = [] (int kind) -> File
        {
            const auto common = File::getSpecialLocation (File::commonApplicationDataDirectory);
            const auto docs = File::getSpecialLocation (File::userDocumentsDirectory);
            if (kind == 1) { auto a = common.getChildFile ("Arturia").getChildFile ("Presets"); if (a.isDirectory()) return a; }
            return docs;
        };
        auto pickFolder = [this, startDir] (int kind, const String& title)
        {
            chooser = std::make_unique<FileChooser> (title, startDir (kind));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                                  [this, kind] (const FileChooser& fc) { if (fc.getResult().isDirectory()) startImport (kind, { fc.getResult() }); });
        };
        auto pickFiles = [this, startDir] (int kind, const String& title, const String& filter)
        {
            chooser = std::make_unique<FileChooser> (title, startDir (kind), filter);
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                                  [this, kind] (const FileChooser& fc) { if (! fc.getResults().isEmpty()) startImport (kind, fc.getResults()); });
        };

        PopupMenu m;
        m.addSectionHeader ("Omnisphere");
        m.addItem (1, "A whole folder (patches, multis + .omnisphere banks)...");
        m.addItem (2, "Bank files (.omnisphere / .prt_omn / .mlt_omn)...");
        m.addSectionHeader ("Arturia - Pigments, Analog Lab, V Collection");
        m.addItem (10, "A whole folder of presets...");
        m.addItem (11, "Bank files (.pgtx / .labx)...");
        m.addSectionHeader ("Native Instruments");
        m.addItem (20, "FM8 folder (.fm8 / .nfm8)...");
        m.addItem (21, "Absynth folder (.nabs / .ksd)...");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&importBtn), [=] (int r)
        {
            switch (r)
            {
                case 1:  pickFolder (0, "Choose a folder of Omnisphere patches / banks"); break;
                case 2:  pickFiles  (0, "Choose Omnisphere banks", "*.omnisphere;*.prt_omn;*.mlt_omn"); break;
                case 10: pickFolder (1, "Choose a folder of Arturia presets"); break;
                case 11: pickFiles  (1, "Choose Arturia banks", "*.pgtx;*.labx"); break;
                case 20: pickFolder (2, "Choose a folder of FM8 presets"); break;
                case 21: pickFolder (3, "Choose a folder of Absynth presets"); break;
                default: break;
            }
        });
    };
    installBtn.setColour (TextButton::buttonColourId, col::sampler.withAlpha (0.25f));
    backBtn.onClick = [this] { if (onClose) onClose(); };
    installBtn.setTooltip ("Install an OZ-mosys pack (.ozpack). You can also drag the file onto this page.");
    installBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Install a pack", File::getSpecialLocation (File::userDesktopDirectory), "*.ozpack");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) installFile (fc.getResult()); });
    };
    makeBtn.setTooltip ("Bundle a bank of your own presets (plus cover art) into one file you can share");
    makeBtn.onClick = [this] { openMakePack(); };
    status.setFont (font (11.5f));
    status.setColour (Label::textColourId, col::sampler);
    addAndMakeVisible (status);
    grid = std::make_unique<Grid>();
    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    refresh();
}

ExpansionsPage::~ExpansionsPage() { if (importer != nullptr) importer->stopThread (5000); }

void ExpansionsPage::refresh()
{
    all = packs::installedPacks();
    for (auto& b : packs::builtInPacks()) all.push_back (b);
    layoutTiles();
}

void ExpansionsPage::openMakePack()
{
    if (makePanel == nullptr)
    {
        makePanel = std::make_unique<MakePackPanel> (*this);
        addChildComponent (*makePanel);
    }
    makePanel->refreshBanks();
    makePanel->setBounds (getLocalBounds().withSizeKeepingCentre (640, 430));
    makePanel->setVisible (true);
    makePanel->toFront (true);
}

void ExpansionsPage::installFile (const File& f)
{
    String name, err;
    if (packs::install (f, name, err))
    {
        status.setText ("Installed \"" + name + "\"", dontSendNotification);
        refresh();
        if (onPacksChanged) onPacksChanged();
    }
    else status.setText (err, dontSendNotification);
}

bool ExpansionsPage::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
    {
        if (File (f).isDirectory()) return true;
        if (f.endsWithIgnoreCase (".ozpack") || f.endsWithIgnoreCase (".omnisphere") || f.endsWithIgnoreCase (".prt_omn")
            || f.endsWithIgnoreCase (".mlt_omn")
            || f.endsWithIgnoreCase (".pgtx") || f.endsWithIgnoreCase (".labx")
            || f.endsWithIgnoreCase (".fm8") || f.endsWithIgnoreCase (".nfm8")
            || f.endsWithIgnoreCase (".nabs") || f.endsWithIgnoreCase (".ksd")) return true;
    }
    return false;
}

void ExpansionsPage::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;
    Array<File> omni, arturia, fm8, absynth;
    for (auto& f : files)
    {
        const File file (f);
        if (f.endsWithIgnoreCase (".ozpack")) installFile (file);
        else if (f.endsWithIgnoreCase (".omnisphere") || f.endsWithIgnoreCase (".prt_omn") || f.endsWithIgnoreCase (".mlt_omn")) omni.add (file);
        else if (f.endsWithIgnoreCase (".pgtx") || f.endsWithIgnoreCase (".labx")) arturia.add (file);
        else if (f.endsWithIgnoreCase (".fm8") || f.endsWithIgnoreCase (".nfm8")) fm8.add (file);
        else if (f.endsWithIgnoreCase (".nabs") || f.endsWithIgnoreCase (".ksd")) absynth.add (file);
        else if (file.isDirectory())
        {
            // guess the synth from what the folder holds
            if (! file.findChildFiles (File::findFiles, true, "*.fm8;*.nfm8").isEmpty())       fm8.add (file);
            else if (! file.findChildFiles (File::findFiles, true, "*.nabs;*.ksd").isEmpty())  absynth.add (file);
            else if (! file.findChildFiles (File::findFiles, true, "*.omnisphere;*.prt_omn;*.mlt_omn").isEmpty()) omni.add (file);
            else                                                                               arturia.add (file);  // Arturia presets are extensionless
        }
    }
    // one importer runs at a time; start the first non-empty group (drop the rest separately)
    if      (! omni.isEmpty())    startImport (0, omni);
    else if (! arturia.isEmpty()) startImport (1, arturia);
    else if (! fm8.isEmpty())     startImport (2, fm8);
    else if (! absynth.isEmpty()) startImport (3, absynth);
    repaint();
}

void ExpansionsPage::layoutTiles()
{
    grid->tiles.clear();
    const auto q = search.getText().trim();
    std::vector<const packs::Pack*> installed, builtIn;
    for (auto& p : all)
    {
        if (styleFilter == "Installed" && p.builtIn) continue;
        if (styleFilter != "All" && styleFilter != "Installed" && p.style != styleFilter) continue;
        if (q.isNotEmpty() && ! p.name.containsIgnoreCase (q) && ! p.tagline.containsIgnoreCase (q) && ! p.author.containsIgnoreCase (q)) continue;
        (p.builtIn ? builtIn : installed).push_back (&p);
    }
    const int w = juce::jmax (200, viewport.getWidth() - 12);
    const int cols = juce::jmax (2, w / 180);
    const int gap = 16, tw = (w - (cols - 1) * gap) / cols, th = tw + 66;
    int y = 0;
    auto place = [&] (const std::vector<const packs::Pack*>& list)
    {
        for (size_t i = 0; i < list.size(); ++i)
        {
            auto* t = grid->tiles.add (new Tile (*this, *list[i]));
            t->setBounds ((int) (i % (size_t) cols) * (tw + gap), y + (int) (i / (size_t) cols) * (th + gap), tw, th);
            grid->addAndMakeVisible (t);
        }
        y += (int) ((list.size() + (size_t) cols - 1) / (size_t) cols) * (th + gap);
    };
    grid->heading2.clear();
    if (! installed.empty())
    {
        grid->heading1 = "YOUR EXPANSIONS";
        y = 24;
        place (installed);
        if (! builtIn.empty()) { grid->heading2 = "BUILT-IN COLLECTIONS"; grid->headingY2 = y + 4; y += 28; }
    }
    else
    {
        grid->heading1 = builtIn.empty() ? "Nothing matches" : "BUILT-IN COLLECTIONS";
        y = 24;
    }
    place (builtIn);
    grid->setSize (w, y + 10);
    grid->repaint();
}

// ---------------------------------------------------------------------------- preset import
void ExpansionsPage::startImport (int kind, const Array<File>& sources)
{
    if (importer != nullptr) return;
    status.setText ("Reading " + (sources.size() == 1 ? sources[0].getFileName() : String (sources.size()) + " items") + "...", dontSendNotification);
    importBtn.setButtonText ("Stop import");
    importer = std::make_unique<ImportThread> (*this, kind, sources);
    importer->startThread();
}

void ExpansionsPage::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto top = r.removeFromTop (30);
    backBtn.setBounds (top.removeFromLeft (96));
    top.removeFromLeft (16);
    makeBtn.setBounds (top.removeFromRight (120));
    top.removeFromRight (8);
    importBtn.setBounds (top.removeFromRight (140));
    top.removeFromRight (8);
    installBtn.setBounds (top.removeFromRight (120));
    top.removeFromRight (16);
    search.setBounds (top.removeFromRight (220));
    r.removeFromTop (34);   // title
    auto chips = r.removeFromTop (24);
    for (auto* c : styleChips)
    {
        const int cw = (int) GlyphArrangement::getStringWidth (font (12.0f), c->getButtonText()) + 26;
        c->setBounds (chips.removeFromLeft (cw));
        chips.removeFromLeft (6);
    }
    status.setBounds (chips);
    status.setJustificationType (Justification::centredRight);
    r.removeFromTop (12);
    viewport.setBounds (r);
    layoutTiles();
    if (makePanel != nullptr) makePanel->setBounds (getLocalBounds().withSizeKeepingCentre (640, 430));
}

void ExpansionsPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (16, 12);
    r.removeFromTop (34);
    g.setColour (col::text);
    g.setFont (font (22.0f, true));
    g.drawText ("Expansions", r.removeFromTop (30), Justification::centredLeft);
}

void ExpansionsPage::paintOverChildren (Graphics& g)
{
    if (! dragOver) return;
    g.setColour (col::sampler.withAlpha (0.12f));
    g.fillRect (getLocalBounds());
    g.setColour (col::sampler);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (6.0f), 10.0f, 2.0f);
    g.setFont (font (18.0f, true));
    g.drawText ("Drop to install the pack", getLocalBounds(), Justification::centred);
}
} // namespace ff::gui
