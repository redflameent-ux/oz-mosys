#include "SamplerPage.h"
#include "../PluginProcessor.h"

using namespace juce;
using namespace ff::gui;

static void askName (Component* from, const String& titleText, const String& initial, std::function<void (String)> done)
{
    auto* top = from->findParentComponentOfClass<AudioProcessorEditor>();
    auto* w = new AlertWindow (titleText, {}, MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", initial);
    w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    if (top != nullptr) { top->addAndMakeVisible (w); w->centreAroundComponent (top, w->getWidth(), w->getHeight()); }
    w->enterModalState (true, ModalCallbackFunction::create ([w, done] (int r)
    {
        if (r == 1 && done) done (w->getTextEditorContents ("name").trim());
    }), true);
}

SamplerPage::SamplerPage (FourfoldProcessor& p, AudioProcessorValueTreeState& state)
    : owner (p), proc (*p.sampler), apvts (state)
{
    setOpaque (true);

    for (auto* b : { &libraryButton, &rescanButton, &newButton, &octDown, &octUp }) addAndMakeVisible (b);
    libraryButton.setTooltip ("Choose which folder is your sample library");
    rescanButton.setTooltip ("Look for new instruments in the library folder");
    newButton.setTooltip ("Make a new empty instrument, then drop recordings onto the mapper");
    libraryButton.onClick = [this] { chooseLibraryFolder(); };
    rescanButton.onClick = [this] { proc.rescanLibrary(); refreshList(); };
    newButton.onClick = [this] { newInstrument(); };
    octDown.onClick = [this] { proc.padBaseNote = jmax (0, proc.padBaseNote - 12); pads.repaint(); };
    octUp.onClick = [this] { proc.padBaseNote = jmin (112, proc.padBaseNote + 12); pads.repaint(); };

    browser.setRowHeight (26);
    browser.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    browser.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    addAndMakeVisible (browser);
    addAndMakeVisible (pads);
    addAndMakeVisible (mapView);

    for (auto* l : { &title, &subtitle, &statusLabel, &padRangeLabel }) { addAndMakeVisible (l); l->setInterceptsMouseClicks (false, false); }
    title.setFont (font (20.0f, true));
    title.setColour (Label::textColourId, col::text);
    subtitle.setFont (font (12.0f));
    subtitle.setColour (Label::textColourId, col::dim);
    statusLabel.setFont (font (11.0f));
    statusLabel.setColour (Label::textColourId, col::sampler);
    padRangeLabel.setFont (font (11.0f));
    padRangeLabel.setColour (Label::textColourId, col::dim);
    padRangeLabel.setJustificationType (Justification::centred);

    playMode.addItemList ({ "Off", "Layered with synth", "Sampler only" }, 1);
    playMode.setTooltip ("Off: only the synth plays. Layered: synth and samples together. Sampler only: just the samples.");
    addAndMakeVisible (playMode);
    comboAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "smp_play", playMode));
    for (auto* t : { &throughFx, &letRing, &choke })
    {
        t->setColour (ToggleButton::tickColourId, col::sampler);
        addAndMakeVisible (t);
    }
    throughFx.setTooltip ("On: samples go through the Effects section with the synth. Off: they stay dry.");
    letRing.setTooltip ("On: recordings ring out after you let go. Off: Release decides.");
    buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, "smp_fx", throughFx));
    buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, "smp_letring", letRing));
    choke.setTooltip ("On: each new note cuts off the one before (like an MPC). Great for sliced loops.");
    buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, "smp_choke", choke));
    addAndMakeVisible (sliceButton);
    sliceButton.setTooltip ("Chop a drum loop (or any loop) into slices, one per pad");
    sliceButton.onClick = [this]
    {
        slicer = std::make_unique<SlicerPanel> (owner);
        slicer->onClose = [sp = SafePointer<SamplerPage> (this)] { MessageManager::callAsync ([sp] { if (sp) sp->slicer.reset(); }); };
        slicer->onMade = [this] (const File& folder)
        {
            proc.rescanLibrary();
            refreshList();
            const int idx = proc.findInList (folder);
            if (idx >= 0) proc.selectInstrument (0, idx); else proc.loadFolderIntoLayer (0, folder);
            if (auto* p = apvts.getParameter ("smp_choke")) p->setValueNotifyingHost (1.0f);
            makeSureItPlays();
            mapView.setTarget (proc.getLayerInfo (0), 0);
            pads.repaint();
        };
        addAndMakeVisible (*slicer);
        slicer->setBounds (getLocalBounds());
    };

    const std::pair<const char*, const char*> defs[] = {
        { "smp_level", "Level" }, { "smp_tune", "Tune" }, { "smp_fine", "Fine" }, { "smp_pan", "Pan" }, { "smp_vel", "Touch" },
        { "smp_attack", "Attack" }, { "smp_decay", "Decay" }, { "smp_sustain", "Sustain" }, { "smp_release", "Release" } };
    for (auto& [id, label] : defs)
    {
        knobs.push_back (std::make_unique<Knob> (apvts, id, label, col::sampler));
        addAndMakeVisible (*knobs.back());
    }

    refreshList();
    startTimerHz (20);
}

SamplerPage::~SamplerPage() { stopTimer(); }

void SamplerPage::visibilityChanged()
{
    if (isVisible())
    {
        proc.rescanLibrary();
        refreshList();
        shown = nullptr;    // force a refresh
        mapView.setTarget (proc.getLayerInfo (0), proc.getArticulation (0));
        timerCallback();
    }
}

// ---------------------------------------------------------------------------- layout
void SamplerPage::resized()
{
    auto r = getLocalBounds();
    if (slicer != nullptr) slicer->setBounds (r);
    const int gap = 8;
    leftArea = r.removeFromLeft (250);
    r.removeFromLeft (gap);
    rightArea = r.removeFromRight (590);
    r.removeFromRight (gap);
    centreArea = r;

    {
        auto a = leftArea.reduced (10, 8);
        a.removeFromTop (22);   // heading
        auto row = a.removeFromTop (26);
        const int bw = (row.getWidth() - 8) / 3;
        libraryButton.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (4);
        rescanButton.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (4);
        newButton.setBounds (row);
        a.removeFromTop (4);
        sliceButton.setBounds (a.removeFromTop (26));
        a.removeFromTop (6);
        statusLabel.setBounds (a.removeFromBottom (18));
        a.removeFromBottom (70);   // drop zone (painted)
        browser.setBounds (a);
    }
    {
        auto a = centreArea.reduced (12, 8);
        title.setBounds (a.removeFromTop (28));
        subtitle.setBounds (a.removeFromTop (18));
        a.removeFromTop (6);
        auto row = a.removeFromTop (24);
        playMode.setBounds (row.removeFromLeft (170));
        a.removeFromTop (6);
        auto artRow = a.removeFromTop (artButtons.isEmpty() ? 0 : 26);
        if (! artButtons.isEmpty())
        {
            const int bw = jmin (110, artRow.getWidth() / artButtons.size());
            for (auto* b : artButtons) { b->setBounds (artRow.removeFromLeft (bw).reduced (2, 0)); }
            a.removeFromTop (6);
        }
        auto bottom = a.removeFromBottom (26);
        octDown.setBounds (bottom.removeFromLeft (30));
        octUp.setBounds (bottom.removeFromRight (30));
        padRangeLabel.setBounds (bottom);
        a.removeFromBottom (6);
        const int side = jmin (a.getWidth(), a.getHeight());
        pads.setBounds (a.withSizeKeepingCentre (side, side));
    }
    {
        auto a = rightArea;
        auto controls = a.removeFromBottom (128);
        a.removeFromBottom (gap);
        mapView.setBounds (a);
        knobArea = controls;
        auto c = controls.reduced (10, 8);
        c.removeFromTop (18);   // heading
        auto toggles = c.removeFromRight (110);
        const int th = toggles.getHeight() / 3;
        letRing.setBounds (toggles.removeFromTop (th).reduced (4, 3));
        choke.setBounds (toggles.removeFromTop (th).reduced (4, 3));
        throughFx.setBounds (toggles.reduced (4, 3));
        const int kw = c.getWidth() / (int) knobs.size();
        for (auto& k : knobs) k->setBounds (c.removeFromLeft (kw));
    }
}

void SamplerPage::paint (Graphics& g)
{
    g.fillAll (col::bg);
    auto panelAt = [&g] (Rectangle<int> r, const String& heading)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (col::sampler);
        g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY(), 26.0f, 2.0f, 1.0f);
        if (heading.isNotEmpty())
        {
            g.setColour (col::dim);
            g.setFont (font (11.0f, true));
            g.drawText (heading, r.reduced (10, 6).removeFromTop (16), Justification::centredLeft);
        }
    };
    panelAt (leftArea, "SAMPLE LIBRARY");
    panelAt (centreArea, {});
    panelAt (knobArea, "SAMPLER SOUND");

    // drop zone at the bottom of the library panel
    auto drop = leftArea.reduced (10, 8).withTrimmedBottom (22);
    drop = drop.removeFromBottom (64).toNearestInt();
    Path dash;
    dash.addRoundedRectangle (drop.toFloat().reduced (1.0f), 6.0f);
    PathStrokeType stroke (1.2f);
    const float lens[] = { 5.0f, 4.0f };
    Path dashed;
    stroke.createDashedStroke (dashed, dash, lens, 2);
    g.setColour (dragging ? col::sampler : col::border.brighter (0.3f));
    g.fillPath (dashed);
    g.setColour (dragging ? col::sampler : col::dim);
    g.setFont (font (11.5f));
    g.drawFittedText ("Drop a folder or WAV files here\n(sub-folders become articulations)", drop.reduced (6), Justification::centred, 2);
}

void SamplerPage::paintOverChildren (Graphics& g)
{
    if (dragging)
    {
        g.setColour (col::sampler.withAlpha (0.08f));
        g.fillRect (getLocalBounds());
    }
}

// ---------------------------------------------------------------------------- browser
int SamplerPage::getNumRows() { return (int) rows.size(); }

void SamplerPage::refreshList()
{
    rows.clear();
    String lastCat;
    const auto& list = proc.getInstrumentList();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (list[(size_t) i].category != lastCat)
        {
            lastCat = list[(size_t) i].category;
            rows.push_back ({ true, lastCat.toUpperCase(), -1 });
        }
        rows.push_back ({ false, list[(size_t) i].name, i });
    }
    browser.updateContent();
    browser.repaint();
}

void SamplerPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) rows.size()) return;
    auto& r = rows[(size_t) row];
    if (r.header)
    {
        g.setColour (col::dim);
        g.setFont (font (10.0f, true));
        g.drawFittedText (r.label, 6, 0, w - 12, h, Justification::bottomLeft, 1);
        return;
    }
    const auto& info = proc.getInstrumentList()[(size_t) r.index];
    const auto cur = proc.getLayerInfo (0);
    const bool active = cur.folder == info.folder && cur.name == info.name;
    if (active)
    {
        g.setColour (col::panelHi);
        g.fillRoundedRectangle (2.0f, 2.0f, (float) w - 4, (float) h - 4, 5.0f);
        g.setColour (col::sampler);
        g.fillRoundedRectangle (2.0f, 6.0f, 3.0f, (float) h - 12, 1.5f);
    }
    g.setColour (active ? col::sampler : col::text);
    g.setFont (font (13.0f));
    g.drawText (r.label, 14, 0, w - 20, h, Justification::centredLeft, true);
}

void SamplerPage::listBoxItemClicked (int row, const MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size() && ! rows[(size_t) row].header)
    {
        proc.selectInstrument (0, rows[(size_t) row].index);
        makeSureItPlays();
        browser.repaint();
        mapView.setTarget (proc.getLayerInfo (0), 0);
    }
}

void SamplerPage::makeSureItPlays()
{
    if (auto* p = apvts.getParameter ("smp_play"); p != nullptr && p->getValue() < 0.01f)
        p->setValueNotifyingHost (p->convertTo0to1 (2.0f));   // Off -> Sampler only
}

void SamplerPage::chooseLibraryFolder()
{
    chooser = std::make_unique<FileChooser> ("Choose your sample library folder", sol::Library::getLibraryFolder());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [this] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.isDirectory())
                              {
                                  sol::Library::setLibraryFolder (f);
                                  proc.rescanLibrary();
                                  refreshList();
                              }
                          });
}

void SamplerPage::newInstrument (std::function<void (File)> then)
{
    askName (this, "Name the new instrument", "", [this, then] (String name)
    {
        if (name.isEmpty()) return;
        auto folder = sol::Library::createInstrument (name);
        proc.loadFolderIntoLayer (0, folder);
        makeSureItPlays();
        refreshList();
        mapView.setTarget (proc.getLayerInfo (0), 0);
        if (then) then (folder);
    });
}

// ---------------------------------------------------------------------------- drops
bool SamplerPage::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).isDirectory() || sol::Library::isAudioFile (File (f))) return true;
    return false;
}

void SamplerPage::importFolderAsInstrument (const StringArray& files)
{
    File first (files[0]);
    auto suggested = first.isDirectory() ? first.getFileName() : first.getParentDirectory().getFileName();
    askName (this, "Name the new instrument", suggested, [this, files] (String name)
    {
        if (name.isEmpty()) return;
        auto folder = sol::Library::createInstrument (name);
        proc.loadFolderIntoLayer (0, folder);
        makeSureItPlays();
        refreshList();
        mapView.setTarget (proc.getLayerInfo (0), 0);
        mapView.addFiles (files);
    });
}

void SamplerPage::filesDropped (const StringArray& files, int x, int)
{
    dragging = false;
    repaint();
    if (files.isEmpty()) return;

    // dropped on the library side (or the demo is loaded): make a new instrument from it
    const auto target = proc.getLayerInfo (0);
    if (x < leftArea.getRight() + 4 || target.isDemo() || ! target.folder.isDirectory())
    {
        importFolderAsInstrument (files);
        return;
    }
    // dropped on the mapper / pads: add to the instrument being edited
    mapView.addFiles (files);
}

// ---------------------------------------------------------------------------- refresh
void SamplerPage::rebuildArticulationButtons()
{
    artButtons.clear();
    auto inst = proc.getDisplayInstrument (0);
    if (inst != nullptr && inst->numArticulations() > 1)
        for (int i = 0; i < inst->numArticulations(); ++i)
        {
            auto* b = artButtons.add (new TextButton (inst->articulations[(size_t) i].name));
            b->onClick = [this, i] { proc.setArticulation (0, i); };
            addAndMakeVisible (b);
        }
    resized();
}

void SamplerPage::refreshInstrumentViews()
{
    auto inst = proc.getDisplayInstrument (0);
    const int artIndex = proc.getArticulation (0);
    const bool instChanged = inst != shown;
    if (! instChanged && artIndex == shownArt) return;

    if (instChanged) rebuildArticulationButtons();
    shown = inst;
    shownArt = artIndex;
    for (int i = 0; i < artButtons.size(); ++i)
        artButtons[i]->setToggleState (i == artIndex, dontSendNotification);

    const sol::Articulation* a = inst != nullptr ? inst->get (artIndex) : nullptr;
    pads.setArticulation (a, inst);
    if (a == nullptr) { title.setText ("Loading...", dontSendNotification); subtitle.setText ({}, dontSendNotification); return; }

    // pads follow the recordings
    if (instChanged && ! a->zones.empty())   // start the pads at the octave of the lowest recording
    {
        int lowRoot = 127;
        for (auto& z : a->zones) lowRoot = jmin (lowRoot, z.rootNote);
        proc.padBaseNote = jlimit (0, 112, lowRoot - lowRoot % 12);
    }

    int lo = 127, hi = 0;
    for (auto& z : a->zones) { lo = jmin (lo, z.rootNote); hi = jmax (hi, z.rootNote); }
    title.setText (inst->name, dontSendNotification);
    subtitle.setText (a->zones.empty() ? String ("No recordings yet - drop some onto the mapper")
                      : (inst->numArticulations() > 1 ? a->name + "  |  " : String())
                        + String ((int) a->zones.size()) + (a->zones.size() == 1 ? " recording" : " recordings")
                        + "  |  " + (lo == hi ? sol::noteName (lo) : sol::noteName (lo) + " to " + sol::noteName (hi)),
                      dontSendNotification);
}

void SamplerPage::timerCallback()
{
    if (! isVisible()) return;
    refreshInstrumentViews();
    pads.repaint();
    padRangeLabel.setText ("Pads " + sol::noteName (proc.padBaseNote) + " - " + sol::noteName (proc.padBaseNote + 15), dontSendNotification);
    auto st = proc.isLoading() ? String ("Loading...") : proc.getStatus();
    if (statusLabel.getText() != st) statusLabel.setText (st, dontSendNotification);
    if (mapView.getTarget().folder != proc.getLayerInfo (0).folder || mapView.getTarget().name != proc.getLayerInfo (0).name)
        mapView.setTarget (proc.getLayerInfo (0), proc.getArticulation (0));
}
