#include "BrowsePage.h"
#include "../PluginProcessor.h"
#include "../Presets.h"
#include "../design/Sections.h"
#include "../design/Mutator.h"

using namespace juce;

namespace ff::gui
{
// ============================================================================ sound map ("Explorer")
Point<float> SoundMap::posOf (int i) const
{
    auto a = area();
    const auto& e = page.entries[(size_t) i];
    const auto h = (uint32) e.name.hashCode();
    const float jx = ((float) (h & 0xff) / 255.0f - 0.5f) * 0.03f;
    const float jy = ((float) ((h >> 8) & 0xff) / 255.0f - 0.5f) * 0.03f;
    return { a.getX() + juce::jlimit (0.0f, 1.0f, e.x + jx) * a.getWidth(),
             a.getBottom() - juce::jlimit (0.0f, 1.0f, e.y + jy) * a.getHeight() };
}

int SoundMap::entryAt (Point<float> p) const
{
    int best = -1;
    float bestD = 10.0f;
    for (int i : page.visible)
    {
        const float d = posOf (i).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void SoundMap::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (b, 6.0f);
    auto a = area();
    ColourGradient grad (Colour (0xff10131c), a.getX(), a.getBottom(), Colour (0xff1c1420), a.getRight(), a.getY(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (a.expanded (10.0f), 8.0f);
    g.setColour (col::border);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * i / 4.0f), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * i / 4.0f), a.getX(), a.getRight());
    }
    g.setColour (col::dim);
    g.setFont (font (10.5f, true));
    g.drawText ("DARK", (int) a.getX(), (int) a.getBottom() + 4, 80, 14, Justification::left);
    g.drawText ("BRIGHT", (int) a.getRight() - 80, (int) a.getBottom() + 4, 80, 14, Justification::right);
    g.drawText ("AGGRESSIVE", (int) a.getX(), (int) a.getY() - 20, 120, 14, Justification::left);
    g.drawText ("SOFT", (int) a.getX() - 28, (int) a.getBottom() - 8, 28, 14, Justification::centred);

    std::vector<bool> shown (page.entries.size(), false);
    for (int i : page.visible) shown[(size_t) i] = true;
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < (int) page.entries.size(); ++i)
        {
            if ((pass == 1) != shown[(size_t) i]) continue;
            const auto& e = page.entries[(size_t) i];
            const auto c = page.bankColour (e.bank);
            const auto p = posOf (i);
            const float r = 3.0f + e.movement * 3.5f;
            if (! shown[(size_t) i]) { g.setColour (c.withAlpha (0.1f)); g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (p)); continue; }
            g.setColour (c.withAlpha (0.22f));
            g.fillEllipse (Rectangle<float> (r * 3.2f, r * 3.2f).withCentre (p));
            g.setColour (c.withAlpha (e.measured ? 0.95f : 0.5f));
            g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (p));
            if (e.factoryIndex < 0)
            {
                g.setColour (Colours::white.withAlpha (0.8f));
                g.drawEllipse (Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (p), 1.0f);
            }
        }
    auto label = [&] (int i, bool strong)
    {
        if (i < 0 || i >= (int) page.entries.size()) return;
        const auto p = posOf (i);
        const auto& e = page.entries[(size_t) i];
        g.setColour (strong ? Colours::white : col::text);
        g.drawEllipse (Rectangle<float> (18.0f, 18.0f).withCentre (p), 1.6f);
        g.setFont (font (12.0f, true));
        const int w = (int) GlyphArrangement::getStringWidth (font (12.0f, true), e.name) + 14;
        auto box = Rectangle<int> ((int) p.x + 12, (int) p.y - 22, w, 20);
        if (box.getRight() > getWidth() - 4) box.setX ((int) p.x - 12 - w);
        g.setColour (col::panelHi.withAlpha (0.95f));
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (Colours::white);
        g.drawText (e.name, box, Justification::centred);
    };
    label (page.selected, true);
    if (hover != page.selected) label (hover, false);
}

void SoundMap::mouseMove (const MouseEvent& e)
{
    const int h = entryAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void SoundMap::mouseDown (const MouseEvent& e) { if (const int h = entryAt (e.position); h >= 0) page.audition (h); }
void SoundMap::mouseDoubleClick (const MouseEvent& e) { if (const int h = entryAt (e.position); h >= 0) page.keep (h); }

// ============================================================================ history list
int BrowsePage::HistoryModel::getNumRows() { return page->proc.mutator->size(); }

void BrowsePage::HistoryModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    auto& m = *page->proc.mutator;
    const int idx = m.size() - 1 - row;   // newest at the top
    const bool cur = idx == m.position();
    if (cur) { g.setColour (col::panelHi); g.fillRoundedRectangle (1.0f, 1.0f, (float) w - 2, (float) h - 2, 3.0f); }
    g.setColour (cur ? col::mod : col::text);
    g.setFont (font (12.0f));
    g.drawText (m.labelAt (idx), 6, 0, w - 12, h, Justification::centredLeft, true);
}

void BrowsePage::HistoryModel::listBoxItemClicked (int row, const MouseEvent&)
{
    auto& m = *page->proc.mutator;
    m.goTo (m.size() - 1 - row);
    page->fineTune.captureBase();
    page->refreshMutation();
}

// ============================================================================ page
static File settingsFile()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VIBRA-VII").getChildFile ("favourites.txt");
    f.getParentDirectory().createDirectory();
    return f;
}

BrowsePage::BrowsePage (FourfoldProcessor& p) : proc (p), fineTune (p.apvts)
{
    setOpaque (true);
    historyModel.page = this;
    loadFavourites();

    // ---- filters ----
    auto setupRange = [this] (Slider& s, Label& l, const String& text)
    {
        s.setSliderStyle (Slider::TwoValueHorizontal);
        s.setRange (0.0, 1.0, 0.01);
        s.setMinAndMaxValues (0.0, 1.0, dontSendNotification);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::trackColourId, col::sampler);
        s.onValueChange = [this] { refilter(); };
        addAndMakeVisible (s);
        l.setText (text, dontSendNotification);
        l.setFont (font (10.5f));
        l.setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    };
    setupRange (brightRange, brightL, "Dark  -  Bright");
    setupRange (aggroRange, aggroL, "Soft  -  Aggressive");
    setupRange (moveRange, moveL, "Still  -  Evolving");
    for (auto* b : { &allBanks, &noBanks }) addAndMakeVisible (b);
    bankView.setViewedComponent (&bankHolder, false);
    bankView.setScrollBarsShown (true, false);
    bankView.setScrollBarThickness (6);
    addAndMakeVisible (bankView);
    allBanks.onClick = [this] { hiddenBanks.clear(); rebuildBankButtons(); refilter(); };
    noBanks.onClick = [this] { for (auto& b : banks) hiddenBanks.insert (b); rebuildBankButtons(); refilter(); };

    // ---- expansions ----
    expansionsBtn.setColour (TextButton::buttonColourId, col::sampler.withAlpha (0.22f));
    expansionsBtn.setTooltip ("Preset collections with cover art - install packs or make your own");
    expansionsBtn.onClick = [this] { showExpansions (true); };
    addAndMakeVisible (expansionsBtn);
    expansions = std::make_unique<ExpansionsPage> (proc);
    expansions->onClose = [this] { showExpansions (false); };
    expansions->onPacksChanged = [this] { rebuild(); };
    expansions->onOpenBank = [this] (const String& bank)
    {
        hiddenBanks.clear();
        for (auto& b : banks) if (b != bank) hiddenBanks.insert (b);
        rebuildBankButtons();
        refilter();
        showExpansions (false);
        if (! visible.empty()) audition (visible.front());
    };
    addChildComponent (*expansions);

    // ---- centre ----
    for (auto* b : { &explorerBtn, &favOnlyBtn, &auditionBtn })
    {
        b->setColour (TextButton::buttonOnColourId, col::sampler.withAlpha (0.28f));
        addAndMakeVisible (b);
    }
    explorerBtn.setTooltip ("Switch between the preset list and the sound map");
    explorerBtn.onClick = [this] { explorer = ! explorer; explorerBtn.setToggleState (explorer, dontSendNotification); list.setVisible (! explorer); map.setVisible (explorer); };
    favOnlyBtn.setClickingTogglesState (true);
    favOnlyBtn.onClick = [this] { refilter(); };
    auditionBtn.setClickingTogglesState (true);
    auditionBtn.setToggleState (true, dontSendNotification);
    auditionBtn.setTooltip ("Play a note when you click a preset");
    search.setTextToShowWhenEmpty ("Search", col::dim);
    search.setFont (font (13.0f));
    search.onTextChange = [this] { refilter(); };
    addAndMakeVisible (search);
    auto makeChips = [this] (juce::OwnedArray<TextButton>& arr, const StringArray& names, std::set<String>& filter, Colour c)
    {
        for (auto& n : names)
        {
            auto* b = arr.add (new TextButton (n));
            b->setColour (TextButton::buttonOnColourId, c.withAlpha (0.3f));
            b->setColour (TextButton::textColourOnId, c.brighter (0.4f));
            b->setTooltip ("Show only these (pick several to widen).  Click again to clear.");
            b->onClick = [this, b, n, &filter]
            {
                if (filter.count (n)) filter.erase (n); else filter.insert (n);
                b->setToggleState (filter.count (n) > 0, dontSendNotification);
                refilter();
            };
            addAndMakeVisible (b);
        }
    };
    makeChips (typeChips, design::typeNames(), typeFilter, col::sampler);
    makeChips (modeChips, design::modeNames(), modeFilter, col::mod);
    list.setRowHeight (34);
    list.setColour (ListBox::backgroundColourId, col::bg);
    addAndMakeVisible (list);
    addChildComponent (map);
    countL.setFont (font (10.5f));
    countL.setColour (Label::textColourId, col::dim);
    countL.setJustificationType (Justification::centredRight);
    addAndMakeVisible (countL);

    // ---- properties ----
    const std::pair<Label*, const char*> labels[] = { { &nameL, "PRESET NAME" }, { &authorL, "AUTHOR" }, { &bankL, "BANK" },
                                                      { &tagsL, "TYPES / CHARACTER" }, { &notesL, "COMMENTS" } };
    for (auto& [l, t] : labels)
    {
        l->setText (t, dontSendNotification);
        l->setFont (font (9.5f, true));
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    for (auto* ed : { &nameEd, &authorEd, &bankEd, &tagsEd, &notesEd }) { ed->setFont (font (13.0f)); addAndMakeVisible (ed); }
    notesEd.setMultiLine (true);
    notesEd.setReturnKeyStartsNewLine (true);
    tagsEd.setTextToShowWhenEmpty ("pad, dark, evolving", col::dim);
    authorEd.onTextChange = [this] { proc.presetInfo.author = authorEd.getText(); };
    bankEd.onTextChange = [this] { proc.presetInfo.bank = bankEd.getText(); };
    tagsEd.onTextChange = [this] { proc.presetInfo.tags = tagsEd.getText(); };
    notesEd.onTextChange = [this] { proc.presetInfo.notes = notesEd.getText(); };
    for (auto* b : { &saveBtn, &saveAsBtn, &revertBtn }) addAndMakeVisible (b);
    saveBtn.onClick = [this] { saveCurrent (false); };
    saveAsBtn.onClick = [this] { saveCurrent (true); };
    revertBtn.setTooltip ("Go back to the sound you had before browsing");
    revertBtn.onClick = [this]
    {
        if (! haveBefore) return;
        proc.setStateInformation (beforeBrowsing.getData(), (int) beforeBrowsing.getSize());
        proc.currentPresetName = beforeName;
        haveBefore = false;
        selected = -1;
        fineTune.captureBase();
        list.repaint(); map.repaint();
    };
    statusL.setFont (font (10.5f));
    statusL.setColour (Label::textColourId, col::sampler);
    addAndMakeVisible (statusL);

    sectionBox.addItemList (design::sectionNames(), 1);
    sectionBox.setSelectedItemIndex (design::secFilter, dontSendNotification);
    addAndMakeVisible (sectionBox);
    for (auto* b : { &secSave, &secLoad, &secCopy, &secPaste, &secInit, &secRandom }) addAndMakeVisible (b);
    auto sec = [this] { return juce::jmax (0, sectionBox.getSelectedItemIndex()); };
    secSave.onClick = [this, sec]
    {
        const int s = sec();
        chooser = std::make_unique<FileChooser> ("Save " + design::sectionNames()[s], design::subpresetFolder (s).getChildFile ("My " + design::sectionNames()[s] + ".ozsub"), "*.ozsub");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                              [this, s] (const FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == File()) return;
                                  if (! f.hasFileExtension ("ozsub")) f = f.withFileExtension ("ozsub");
                                  if (design::saveSection (proc, s, f)) statusL.setText ("Saved " + f.getFileName(), dontSendNotification);
                              });
    };
    secLoad.onClick = [this] { showLoadSectionMenu(); };
    secCopy.onClick = [this, sec] { design::sectionClipboard() = design::captureSection (proc, sec()); statusL.setText ("Copied " + design::sectionNames()[sec()], dontSendNotification); };
    secPaste.onClick = [this, sec] { if (design::sectionClipboard().isValid()) design::applySection (proc, design::sectionClipboard(), sec()); };
    secInit.onClick = [this, sec] { design::initSection (proc, sec()); };
    secRandom.onClick = [this, sec]
    {
        auto& m = *proc.mutator;
        const auto saved = m.locked;
        for (int s = 0; s < design::numSections; ++s) m.locked[(size_t) s] = s != sec();
        m.randomOnly (0.6f);
        m.locked = saved;
        fineTune.captureBase();
        refreshMutation();
    };
    secSave.setTooltip ("Subpresets: save just this section to reuse in other sounds");

    // ---- mutation ----
    for (int s = 0; s < design::numSections; ++s)
    {
        auto* b = lockButtons.add (new LockCell (design::sectionNames()[s]));
        b->onToggle = [this, s] { proc.mutator->locked[(size_t) s] = ! proc.mutator->locked[(size_t) s]; refreshMutation(); };
        b->onAmount = [this, s] (float v) { proc.mutator->sectionAmount[(size_t) s] = v; };
        addAndMakeVisible (b);
    }
    auto amt = [this] (Slider& s, Label& l, const String& text, float init, std::function<void (float)> set)
    {
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setRange (0.0, 1.0, 0.01);
        s.setValue (init, dontSendNotification);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::trackColourId, col::mod);
        s.onValueChange = [&s, set] { set ((float) s.getValue()); };
        addAndMakeVisible (s);
        l.setText (text, dontSendNotification);
        l.setFont (font (11.0f));
        l.setColour (Label::textColourId, col::text);
        addAndMakeVisible (l);
    };
    amt (mutAmount, mutL, "Mutation Amount", proc.mutator->mutationAmount, [this] (float v) { proc.mutator->mutationAmount = v; });
    amt (randAmount, randL, "Random Amount", proc.mutator->randomAmount, [this] (float v) { proc.mutator->randomAmount = v; });
    for (auto* b : { &mutateBtn, &retryBtn, &histBack, &histSave })
    {
        b->setColour (TextButton::buttonColourId, col::mod.withAlpha (0.16f));
        addAndMakeVisible (b);
    }
    mutateBtn.setTooltip ("Make a variation: moves toward a similar-sounding preset (Mutation Amount) plus fresh randomness (Random Amount)");
    retryBtn.setTooltip ("Didn't like it? Try another variation from the same starting point");
    mutateBtn.onClick = [this] { proc.mutator->mutate(); fineTune.captureBase(); refreshMutation(); };
    retryBtn.onClick = [this] { proc.mutator->retry(); fineTune.captureBase(); refreshMutation(); };
    histBack.setTooltip ("Back to the original sound");
    histBack.onClick = [this] { proc.mutator->goTo (0); fineTune.captureBase(); refreshMutation(); };
    histSave.setTooltip ("Save this variation as a preset");
    histSave.onClick = [this] { saveCurrent (true); };
    targetL.setFont (font (10.0f));
    targetL.setColour (Label::textColourId, col::dim);
    addAndMakeVisible (targetL);

    for (int i = 0; i < design::FineTune::numKnobs; ++i)
    {
        auto* s = fineKnobs.add (new Slider());
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setRange (-1.0, 1.0, 0.01);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s->setDoubleClickReturnValue (true, 0.0);
        s->setColour (Slider::rotarySliderFillColourId, col::mod);
        s->getProperties().set ("bipolar", true);
        s->onValueChange = [this, i, s] { fineTune.knob[i] = (float) s->getValue(); fineTune.apply(); };
        addAndMakeVisible (s);
        auto* l = fineLabels.add (new Label ({}, design::FineTune::names()[i]));
        l->setFont (font (10.5f));
        l->setJustificationType (Justification::centred);
        l->setColour (Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }
    history.setModel (&historyModel);
    history.setRowHeight (20);
    history.setColour (ListBox::backgroundColourId, col::bg);
    addAndMakeVisible (history);

    rebuild();
    fineTune.captureBase();
    refreshMutation();
    showInfo (-1);
    startTimerHz (15);
}

void BrowsePage::showExpansions (bool show)
{
    if (show) { expansions->refresh(); expansions->setBounds (getLocalBounds()); expansions->toFront (false); }
    expansions->setVisible (show);
}

BrowsePage::~BrowsePage() { stopTimer(); proc.roll.previewOff(); history.setModel (nullptr); }

void BrowsePage::visibilityChanged()
{
    if (isVisible()) { rebuild(); fineTune.captureBase(); refreshMutation(); showInfo (-1); }
    else proc.roll.previewOff();
}

void BrowsePage::loadFavourites()
{
    favourites.clear();
    for (auto& l : StringArray::fromLines (settingsFile().loadFileAsString())) if (l.isNotEmpty()) favourites.insert (l);
}

void BrowsePage::saveFavourites()
{
    StringArray a;
    for (auto& f : favourites) a.add (f);
    settingsFile().replaceWithText (a.joinIntoString ("\n"));
}

Colour BrowsePage::bankColour (const String& bank) const
{
    if (bank == "My Presets") return Colours::white;
    const auto it = std::find (banks.begin(), banks.end(), bank);
    const int i = it == banks.end() ? 0 : (int) (it - banks.begin());
    return Colour::fromHSV (std::fmod (i * 0.618034f, 1.0f), 0.55f, 0.98f, 1.0f);
}

bool BrowsePage::passes (const Entry& e) const
{
    if (hiddenBanks.count (e.bank)) return false;
    if (favOnlyBtn.getToggleState() && ! favourites.count (e.name)) return false;
    if (e.x < brightRange.getMinValue() - 1e-3 || e.x > brightRange.getMaxValue() + 1e-3) return false;
    if (e.y < aggroRange.getMinValue() - 1e-3 || e.y > aggroRange.getMaxValue() + 1e-3) return false;
    if (e.movement < moveRange.getMinValue() - 1e-3 || e.movement > moveRange.getMaxValue() + 1e-3) return false;
    if (! typeFilter.empty())
    {
        bool any = false;
        for (auto& t : typeFilter) any = any || e.types.contains (t);
        if (! any) return false;
    }
    for (auto& m : modeFilter) if (! e.modes.contains (m)) return false;
    const auto q = search.getText().trim();
    if (q.isNotEmpty() && ! e.name.containsIgnoreCase (q) && ! e.tags.containsIgnoreCase (q)
        && ! e.bank.containsIgnoreCase (q) && ! e.author.containsIgnoreCase (q)) return false;
    return true;
}

static void classifyFromXml (BrowsePage::Entry& e, const XmlElement& xml)
{
    std::map<String, float> vals;
    for (auto* p : xml.getChildWithTagNameIterator ("PARAM")) vals[p->getStringAttribute ("id")] = (float) p->getDoubleAttribute ("value");
    e.types = design::classifyTypes (e.name, e.bank, e.tags);
    e.modes = design::classifyModes ([&vals] (const char* id, float def) { auto it = vals.find (id); return it == vals.end() ? def : it->second; }, e.movement);
}

void BrowsePage::rebuild()
{
    entries.clear();
    banks.clear();
    const auto& fl = factoryPresets();
    for (int i = 0; i < (int) fl.size(); ++i)
    {
        const auto& f = fl[(size_t) i];
        Entry e;
        e.name = f.name; e.bank = f.category; e.author = "VIBRA-VII";
        e.tags = f.category.toLowerCase().replace (" & ", ", ");
        e.x = f.mapX; e.y = f.mapY; e.movement = f.movement;
        e.factoryIndex = i;
        e.types = design::classifyTypes (e.name, e.bank, {});
        e.modes = design::classifyModes ([&f] (const char* id, float def)
        {
            for (auto& [k, v] : f.values) if (k == id) return v;
            return def;
        }, e.movement);
        entries.push_back (e);
        if (std::find (banks.begin(), banks.end(), f.category) == banks.end()) banks.push_back (f.category);
    }
    for (auto& d : RangedDirectoryIterator (FourfoldProcessor::getUserPresetFolder(), false, "*.ozpreset"))
    {
        auto xml = XmlDocument::parse (d.getFile());
        if (xml == nullptr) continue;
        Entry e;
        e.name = d.getFile().getFileNameWithoutExtension();
        e.file = d.getFile();
        e.bank = xml->getStringAttribute ("bank", "My Presets");
        if (e.bank.isEmpty()) e.bank = "My Presets";
        e.author = xml->getStringAttribute ("author");
        e.tags = xml->getStringAttribute ("tags");
        e.notes = xml->getStringAttribute ("notes");
        e.measured = xml->hasAttribute ("mapX");
        const auto h = (uint32) e.name.hashCode();
        e.x = (float) xml->getDoubleAttribute ("mapX", (h & 0xffff) / 65535.0);
        e.y = (float) xml->getDoubleAttribute ("mapY", (h >> 16) / 65535.0);
        e.movement = (float) xml->getDoubleAttribute ("movement", 0.5);
        classifyFromXml (e, *xml);
        entries.push_back (e);
        if (std::find (banks.begin(), banks.end(), e.bank) == banks.end()) banks.push_back (e.bank);
    }
    bankCovers.clear();
    for (auto& pk : packs::installedPacks())
    {
        if (pk.cover.isValid()) bankCovers[pk.name] = pk.cover;
        for (auto& f : packs::presetsIn (pk))
        {
            auto xml = XmlDocument::parse (f);
            if (xml == nullptr) continue;
            Entry e;
            e.name = f.getFileNameWithoutExtension();
            e.file = f;
            e.bank = pk.name;
            e.author = xml->getStringAttribute ("author", pk.author);
            e.tags = xml->getStringAttribute ("tags");
            e.notes = xml->getStringAttribute ("notes");
            e.measured = xml->hasAttribute ("mapX");
            const auto h = (uint32) e.name.hashCode();
            e.x = (float) xml->getDoubleAttribute ("mapX", (h & 0xffff) / 65535.0);
            e.y = (float) xml->getDoubleAttribute ("mapY", (h >> 16) / 65535.0);
            e.movement = (float) xml->getDoubleAttribute ("movement", 0.5);
            classifyFromXml (e, *xml);
            entries.push_back (e);
        }
        if (std::find (banks.begin(), banks.end(), pk.name) == banks.end()) banks.push_back (pk.name);
    }
    rebuildBankButtons();
    refilter();
}

void BrowsePage::refilter()
{
    visible.clear();
    for (int i = 0; i < (int) entries.size(); ++i) if (passes (entries[(size_t) i])) visible.push_back (i);
    std::stable_sort (visible.begin(), visible.end(), [this] (int a, int b) { return entries[(size_t) a].name.compareNatural (entries[(size_t) b].name) < 0; });
    list.updateContent();
    list.repaint();
    map.repaint();
    countL.setText (String ((int) visible.size()) + " of " + String ((int) entries.size()), dontSendNotification);
}

void BrowsePage::rebuildBankButtons()
{
    bankButtons.clear();
    for (auto& b : banks)
    {
        auto* btn = bankButtons.add (new TextButton (b));
        btn->setToggleState (! hiddenBanks.count (b), dontSendNotification);
        btn->setColour (TextButton::buttonOnColourId, bankColour (b).withAlpha (0.22f));
        btn->setColour (TextButton::textColourOnId, bankColour (b).brighter (0.3f));
        btn->setTooltip ("Click to show / hide.  Shift-click: only this bank.  Double-click: only this bank.");
        btn->onClick = [this, b]
        {
            if (ModifierKeys::currentModifiers.isShiftDown() || ModifierKeys::currentModifiers.isCommandDown())
            {
                hiddenBanks.clear();
                for (auto& x : banks) if (x != b) hiddenBanks.insert (x);
            }
            else if (hiddenBanks.count (b)) hiddenBanks.erase (b);
            else hiddenBanks.insert (b);
            rebuildBankButtons();
            refilter();
        };
        bankHolder.addAndMakeVisible (btn);
    }
    resized();
}

// ---------------------------------------------------------------------------- list
void BrowsePage::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size())) return;
    const int idx = visible[(size_t) row];
    const auto& e = entries[(size_t) idx];
    const auto accent = bankColour (e.bank);
    const bool sel = idx == selected;
    if (sel)
    {
        g.setColour (accent.withAlpha (0.16f)); g.fillRect (0, 0, w, h);
        g.setColour (accent); g.fillRect (0, 0, 3, h);   // left accent bar
    }
    else if (row % 2) { g.setColour (Colour (0xff0f1118)); g.fillRect (0, 0, w, h); }

    int x = 10;
    // cover thumbnail: the bank's accent as a small gradient "cover" with its initial
    const int cs = h - 10;
    {
        auto cov = juce::Rectangle<float> ((float) x, (h - cs) * 0.5f, (float) cs, (float) cs);
        g.setGradientFill (ColourGradient (accent.brighter (0.12f), cov.getX(), cov.getY(), accent.darker (1.5f), cov.getRight(), cov.getBottom(), false));
        g.fillRoundedRectangle (cov, 4.0f);
        g.setColour (col::bg.withAlpha (0.3f)); g.drawRoundedRectangle (cov.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.setFont (font (cs * 0.5f, true));
        g.drawText (e.bank.substring (0, 1).toUpperCase(), cov.toNearestInt(), Justification::centred);
        x += cs + 8;
    }
    // waveform glyph in the bank/chakra colour
    {
        const float ww = 20.0f, wy = h * 0.5f, amp = h * 0.2f;
        Path wf; bool first = true;
        for (int i = 0; i <= 12; ++i)
        {
            const float px = (float) x + ww * (float) i / 12.0f;
            const float py = wy - std::sin ((float) i * 0.95f + (float) (idx % 7)) * amp * (0.45f + 0.55f * (float) ((idx * 5 + i) % 4) / 3.0f);
            if (first) { wf.startNewSubPath (px, py); first = false; } else wf.lineTo (px, py);
        }
        g.setColour (accent.withAlpha (sel ? 0.95f : 0.7f));
        g.strokePath (wf, PathStrokeType (1.3f));
        x += (int) ww + 10;
    }
    // name
    const int nameRight = (int) (w * 0.44);
    g.setColour (sel ? Colours::white : col::text);
    g.setFont (font (13.0f, sel));
    g.drawText (e.name, x, 0, nameRight - x - 8, h, Justification::centredLeft, true);
    // category pill
    {
        String cat = e.types.isEmpty() ? e.bank : e.types[0];
        if (cat.isEmpty()) cat = "Preset";
        const float pw = GlyphArrangement::getStringWidth (font (10.5f, true), cat) + 16.0f;
        auto pill = juce::Rectangle<float> ((float) nameRight, h * 0.5f - 9.0f, pw, 18.0f);
        g.setColour (accent.withAlpha (0.16f)); g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (accent.brighter (0.2f)); g.setFont (font (10.5f, true));
        g.drawText (cat, pill.toNearestInt(), Justification::centred);
    }
    // heart (far right) + author
    const int heartX = w - 26;
    const bool fav = favourites.count (e.name) > 0;
    g.setColour (fav ? col::filter : col::border.brighter (0.3f));
    g.setFont (font (14.0f));
    g.drawText (fav ? String::charToString (0x2665) : String::charToString (0x2661), heartX, 0, 22, h, Justification::centred);
    g.setColour (col::dim);
    g.setFont (font (11.5f));
    const int authX = (int) (w * 0.66);
    g.drawText (e.author, authX, 0, heartX - authX - 8, h, Justification::centredRight, true);
}

void BrowsePage::listBoxItemClicked (int row, const MouseEvent& ev)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size())) return;
    const int idx = visible[(size_t) row];
    if (ev.x > list.getWidth() - 34)   // the heart column on the right
    {
        const auto& n = entries[(size_t) idx].name;
        if (favourites.count (n)) favourites.erase (n); else favourites.insert (n);
        saveFavourites();
        list.repaint();
        return;
    }
    audition (idx);
}

void BrowsePage::listBoxItemDoubleClicked (int row, const MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) visible.size())) keep (visible[(size_t) row]);
}

void BrowsePage::audition (int i)
{
    if (i < 0 || i >= (int) entries.size()) return;
    if (! haveBefore)
    {
        beforeBrowsing.reset();
        proc.getStateInformation (beforeBrowsing);
        beforeName = proc.currentPresetName;
        haveBefore = true;
    }
    const auto& e = entries[(size_t) i];
    if (e.factoryIndex >= 0) proc.setCurrentProgram (e.factoryIndex);
    else proc.loadPresetFromFile (e.file);
    proc.mutator->clearHistory();
    selected = i;
    fineTune.captureBase();
    for (auto* k : fineKnobs) k->setValue (0.0, dontSendNotification);
    showInfo (i);
    refreshMutation();
    if (auditionBtn.getToggleState())
    {
        proc.roll.previewOff();
        proc.roll.preview (60.0f, 0.8f);
        previewOffAt = Time::getMillisecondCounterHiRes() + 1400.0;
    }
    list.repaint();
    map.repaint();
}

void BrowsePage::keep (int i)
{
    if (selected != i) audition (i);
    haveBefore = false;
    proc.roll.previewOff();
    statusL.setText ("Loaded " + entries[(size_t) i].name, dontSendNotification);
}

void BrowsePage::showInfo (int)
{
    shownPreset = proc.currentPresetName;
    nameEd.setText (proc.currentPresetName, dontSendNotification);
    const bool factory = proc.presetInfo.author == "VIBRA-VII";
    authorEd.setText (proc.presetInfo.author, dontSendNotification);
    bankEd.setText (proc.presetInfo.bank, dontSendNotification);
    tagsEd.setText (proc.presetInfo.tags, dontSendNotification);
    notesEd.setText (proc.presetInfo.notes, dontSendNotification);
    saveBtn.setEnabled (! factory);
    saveBtn.setTooltip (factory ? "Factory sounds can't be overwritten - use Save As" : "Save over this preset");
    repaint (rightArea);
}

void BrowsePage::saveCurrent (bool asNew)
{
    auto doSave = [this] (String name)
    {
        name = File::createLegalFileName (name.trim());
        if (name.isEmpty()) return;
        if (proc.presetInfo.author.isEmpty() || proc.presetInfo.author == "VIBRA-VII") proc.presetInfo.author = FourfoldProcessor::getDefaultAuthor();
        else FourfoldProcessor::setDefaultAuthor (proc.presetInfo.author);
        if (proc.presetInfo.bank.isEmpty() || factoryPresets().end() != std::find_if (factoryPresets().begin(), factoryPresets().end(),
                                                                                     [this] (auto& f) { return f.category == proc.presetInfo.bank; }))
            proc.presetInfo.bank = "My Presets";
        statusL.setText ("Measuring the sound for the map...", dontSendNotification);
        statusL.repaint();
        proc.presetInfo.measured = false;
        auto file = FourfoldProcessor::getUserPresetFolder().getChildFile (name + ".ozpreset");
        if (proc.savePresetToFile (file))
        {
            statusL.setText ("Saved " + name, dontSendNotification);
            haveBefore = false;
            rebuild();
            showInfo (-1);
        }
        else statusL.setText ("Couldn't save", dontSendNotification);
    };

    if (! asNew && proc.presetInfo.author != "VIBRA-VII") { doSave (proc.currentPresetName); return; }
    auto* w = new AlertWindow ("Save preset as", {}, MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.currentPresetName);
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    if (auto* top = findParentComponentOfClass<AudioProcessorEditor>()) { top->addAndMakeVisible (w); w->centreAroundComponent (top, w->getWidth(), w->getHeight()); }
    w->enterModalState (true, ModalCallbackFunction::create ([w, doSave] (int r) { if (r == 1) doSave (w->getTextEditorContents ("name")); }), true);
}

void BrowsePage::showLoadSectionMenu()
{
    const int s = juce::jmax (0, sectionBox.getSelectedItemIndex());
    auto files = design::subpresetFolder (s).findChildFiles (File::findFiles, false, "*.ozsub");
    files.sort();
    PopupMenu m;
    for (int i = 0; i < files.size(); ++i) m.addItem (i + 1, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) m.addItem (-1, "No saved " + design::sectionNames()[s] + " yet", false, false);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&secLoad), [this, s, files] (int r)
    {
        if (r > 0 && r <= files.size()) { design::loadSection (proc, s, files[r - 1]); fineTune.captureBase(); }
    });
}

void BrowsePage::refreshMutation()
{
    auto& m = *proc.mutator;
    for (int s = 0; s < lockButtons.size(); ++s)
    {
        lockButtons[s]->locked = m.locked[(size_t) s];
        lockButtons[s]->amount = m.sectionAmount[(size_t) s];
        lockButtons[s]->repaint();
    }
    history.updateContent();
    history.repaint();
    targetL.setText (m.lastTargetName.isNotEmpty() && m.size() > 1 ? "Last move: toward \"" + m.lastTargetName + "\"" : String ("Mutate to explore variations"),
                     dontSendNotification);
    shownHistory = m.position();
    nameEd.setText (proc.currentPresetName, dontSendNotification);
}

void BrowsePage::timerCallback()
{
    if (! isVisible()) return;
    if (previewOffAt > 0 && Time::getMillisecondCounterHiRes() > previewOffAt) { proc.roll.previewOff(); previewOffAt = 0; }
    if (shownPreset != proc.currentPresetName && proc.mutator->position() == shownHistory) showInfo (-1);
    if (proc.mutator->position() != shownHistory) refreshMutation();
}

// ---------------------------------------------------------------------------- layout
void BrowsePage::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (232);
    r.removeFromBottom (8);
    leftArea = r.removeFromLeft (200);
    r.removeFromLeft (8);
    rightArea = r.removeFromRight (250);
    r.removeFromRight (8);
    centreArea = r;

    lockArea = bottom.removeFromLeft (300); bottom.removeFromLeft (8);
    histArea = bottom.removeFromRight (280); bottom.removeFromRight (8);
    mutArea = bottom.removeFromLeft (bottom.getWidth() * 42 / 100); bottom.removeFromLeft (8);
    fineArea = bottom;

    // filters
    {
        auto a = leftArea.reduced (8, 8);
        a.removeFromTop (20);
        expansionsBtn.setBounds (a.removeFromTop (26));
        a.removeFromTop (6);
        auto row = a.removeFromTop (22);
        allBanks.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2)); row.removeFromLeft (4);
        noBanks.setBounds (row);
        a.removeFromTop (4);
        auto sliders = a.removeFromBottom (3 * 38);
        sliders.removeFromTop (0);
        a.removeFromBottom (6);
        bankView.setBounds (a);
        const int bw = a.getWidth() - 8;
        bankHolder.setSize (bw, bankButtons.size() * 22);
        for (int i = 0; i < bankButtons.size(); ++i) bankButtons[i]->setBounds (Rectangle<int> (0, i * 22, bw, 22).reduced (0, 1));
        for (auto [s, l] : { std::pair<Slider*, Label*> { &brightRange, &brightL }, { &aggroRange, &aggroL }, { &moveRange, &moveL } })
        {
            l->setBounds (sliders.removeFromTop (16));
            s->setBounds (sliders.removeFromTop (20));
            sliders.removeFromTop (2);
        }
    }
    // centre
    {
        auto a = centreArea.reduced (8, 8);
        auto top = a.removeFromTop (26);
        explorerBtn.setBounds (top.removeFromLeft (84)); top.removeFromLeft (8);
        auditionBtn.setBounds (top.removeFromRight (80)); top.removeFromRight (6);
        favOnlyBtn.setBounds (top.removeFromRight (90)); top.removeFromRight (6);
        countL.setBounds (top.removeFromRight (80));
        search.setBounds (top);
        a.removeFromTop (4);
        auto chipRow = [] (juce::OwnedArray<TextButton>& arr, Rectangle<int> row)
        {
            int total = 0;
            for (auto* b : arr) total += (int) GlyphArrangement::getStringWidth (font (11.0f), b->getButtonText()) + 16;
            const float scale = juce::jmin (1.0f, (float) row.getWidth() / (float) juce::jmax (1, total + 3 * arr.size()));
            for (auto* b : arr)
            {
                const int w = (int) ((GlyphArrangement::getStringWidth (font (11.0f), b->getButtonText()) + 16) * scale);
                b->setBounds (row.removeFromLeft (w));
                row.removeFromLeft (3);
            }
        };
        chipRow (typeChips, a.removeFromTop (21));
        a.removeFromTop (3);
        chipRow (modeChips, a.removeFromTop (21));
        a.removeFromTop (chipsH - 4 - 45);
        a.removeFromTop (22);   // column headings
        list.setBounds (a);
        map.setBounds (a);
    }
    // properties
    {
        auto a = rightArea.reduced (10, 8);
        a.removeFromTop (20);
        coverArea = a.removeFromTop (78);
        a.removeFromTop (6);
        for (auto [l, ed] : { std::pair<Label*, TextEditor*> { &nameL, &nameEd }, { &authorL, &authorEd }, { &bankL, &bankEd }, { &tagsL, &tagsEd } })
        {
            l->setBounds (a.removeFromTop (14));
            ed->setBounds (a.removeFromTop (24));
            a.removeFromTop (4);
        }
        auto sub = a.removeFromBottom (54);
        statusL.setBounds (a.removeFromBottom (18));
        auto btns = a.removeFromBottom (26);
        a.removeFromBottom (4);
        notesL.setBounds (a.removeFromTop (14));
        notesEd.setBounds (a);
        const int bw = btns.getWidth() / 3;
        saveBtn.setBounds (btns.removeFromLeft (bw).reduced (2, 0));
        saveAsBtn.setBounds (btns.removeFromLeft (bw).reduced (2, 0));
        revertBtn.setBounds (btns.reduced (2, 0));
        sub.removeFromTop (4);
        sectionBox.setBounds (sub.removeFromTop (22));
        sub.removeFromTop (4);
        const int sw = sub.getWidth() / 6;
        for (auto* b : { &secSave, &secLoad, &secCopy, &secPaste, &secInit, &secRandom }) b->setBounds (sub.removeFromLeft (sw).reduced (1, 0));
    }
    // module lock
    {
        auto a = lockArea.reduced (8, 8);
        a.removeFromTop (20);
        const int cols = 3, rows = (lockButtons.size() + cols - 1) / cols;
        const int bw = a.getWidth() / cols, bh = a.getHeight() / juce::jmax (1, rows);
        for (int i = 0; i < lockButtons.size(); ++i)
            lockButtons[i]->setBounds (Rectangle<int> (a.getX() + (i % cols) * bw, a.getY() + (i / cols) * bh, bw, bh).reduced (2));
    }
    // mutation
    {
        auto a = mutArea.reduced (12, 8);
        a.removeFromTop (28);
        for (auto [s, l] : { std::pair<Slider*, Label*> { &mutAmount, &mutL }, { &randAmount, &randL } })
        {
            auto row = a.removeFromTop (30);
            l->setBounds (row.removeFromLeft (110));
            s->setBounds (row);
            a.removeFromTop (6);
        }
        a.removeFromTop (6);
        auto row = a.removeFromTop (32);
        const int bw = juce::jmin (110, row.getWidth() / 2 - 6);
        mutateBtn.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (10);
        retryBtn.setBounds (row.removeFromLeft (bw));
        a.removeFromTop (8);
        targetL.setBounds (a.removeFromTop (18));
    }
    // fine tuning
    {
        auto a = fineArea.reduced (8, 8);
        a.removeFromTop (22);
        const int cols = 4;
        const int kw = a.getWidth() / cols, kh = a.getHeight() / 2;
        for (int i = 0; i < fineKnobs.size(); ++i)
        {
            auto cell = Rectangle<int> (a.getX() + (i % cols) * kw, a.getY() + (i / cols) * kh, kw, kh);
            fineLabels[i]->setBounds (cell.removeFromBottom (16));
            fineKnobs[i]->setBounds (cell.reduced (4, 2));
        }
    }
    // history
    {
        auto a = histArea.reduced (8, 8);
        a.removeFromTop (20);
        auto btns = a.removeFromBottom (26);
        a.removeFromBottom (4);
        history.setBounds (a);
        histBack.setBounds (btns.removeFromLeft (btns.getWidth() / 2 - 3));
        btns.removeFromLeft (6);
        histSave.setBounds (btns);
    }
    histBack.setButtonText ("Original");
    if (expansions != nullptr && expansions->isVisible()) expansions->setBounds (getLocalBounds());
}

void BrowsePage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    const std::pair<Rectangle<int>, const char*> panels[] = {
        { leftArea, "BANKS" }, { centreArea, "" }, { rightArea, "PROPERTIES" }, { lockArea, "MODULE LOCK" },
        { mutArea, "PRESET MUTATION" }, { fineArea, "FINE TUNING" }, { histArea, "MUTATION HISTORY" } };
    for (auto& [r, t] : panels)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (col::sampler);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (font (10.5f, true));
        g.drawText (t, r.reduced (10, 6).removeFromTop (16), Justification::centredLeft);
    }
    // collection cover for the current sound
    if (! coverArea.isEmpty())
    {
        const auto bank = proc.presetInfo.bank.isNotEmpty() ? proc.presetInfo.bank : String ("My Presets");
        auto c = coverArea.toFloat();
        auto it = bankCovers.find (bank);
        if (it != bankCovers.end())
        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addRoundedRectangle (c, 6.0f);
            g.reduceClipRegion (clip);
            g.drawImage (it->second, c, RectanglePlacement::fillDestination);
        }
        else packs::drawCover (g, c, bank, bankColour (bank));
        g.setColour (Colours::white);
        g.setFont (font (17.0f, true));
        g.drawFittedText (bank.toUpperCase(), coverArea.reduced (10, 6), Justification::bottomLeft, 2);
    }
    // list column headings
    if (! explorer)
    {
        auto h = centreArea.reduced (8, 8).withTrimmedTop (30 + chipsH).removeFromTop (18);
        const int w = h.getWidth();
        g.setColour (col::dim);
        g.setFont (font (10.0f, true));
        g.drawText ("NAME", h.getX() + 26, h.getY(), 100, 18, Justification::centredLeft);
        g.drawText ("COLLECTION", h.getX() + (int) (w * 0.36) + 10, h.getY(), 100, 18, Justification::centredLeft);
        g.drawText ("TYPE / CHARACTER", h.getX() + (int) (w * 0.58), h.getY(), 100, 18, Justification::centredLeft);
        g.drawText ("AUTHOR", h.getX() + (int) (w * 0.78), h.getY(), 100, 18, Justification::centredLeft);
    }
    else
    {
        auto h = centreArea.reduced (8, 8).withTrimmedTop (30 + chipsH).removeFromTop (18);
        g.setColour (col::dim);
        g.setFont (font (10.0f, true));
        g.drawText ("EXPLORER - every sound placed by how it sounds. Click to hear, double-click to keep.", h, Justification::centredLeft);
    }
}
} // namespace ff::gui
