#include "PluginEditor.h"
#include "Presets.h"

using namespace ff;
using namespace ff::gui;

static constexpr int editorW = 1300, editorH = 830;

FourfoldEditor::FourfoldEditor (FourfoldProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      wtDisplay (p), anDisplay (p), grDisplay (p), resDisplay (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    // ---------------- top bar ----------------
    addAndMakeVisible (presetBox);
    presetBox.onChange = [this] { presetChosen(); };
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn }) addAndMakeVisible (b);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    saveBtn.onClick = [this] { savePreset(); };
    saveBtn.setTooltip ("Save the current sound as a preset (Documents/OZ-mosys/Presets)");
    makeKnob (topKnobs, "master", "Master", col::text);
    makeKnob (topKnobs, "glide", "Glide", col::text);
    makeKnob (topKnobs, "bend", "Bend", col::text);
    makeToggle (monoToggle, "voice_mode", "MONO", col::text);
    monoToggle.setTooltip ("Mono: one note at a time. New notes cut the last one (hi-hat choke, sliding 808s).");
    makeCombo (scaleBox, "scale", scaleNames());
    scaleBox.setTooltip ("Tuning system: ragas and maqamat with their real micro-tones, or just intonation");
    makeCombo (rootBox, "scale_root", noteNames());
    rootBox.setTooltip ("Root note the scale tuning is built on");
    makeKnob (tuneKnobs, "tune_a4", "A4 Hz", col::text);
    refreshPresetList();

    // ---------------- wavetable ----------------
    makeToggle (wtOn, "wt_on", "WAVETABLE", col::wavetable);
    makeCombo (wtTable, "wt_table", wavetableNames());
    makeCombo (wtWarp, "wt_warp", warpNames());
    wtWarp.setTooltip ("Warp mode: bends the waveform's phase for extra harmonics");
    addAndMakeVisible (wtLoad);
    wtLoad.setTooltip ("Import your own wavetable (.wav, 2048-sample frames like Serum tables, or a single cycle)");
    wtLoad.onClick = [this] { loadWavetable(); };
    addAndMakeVisible (wtDisplay);
    makeKnob (wtKnobs, "wt_pos", "Position", col::wavetable, dstWtPos);
    makeKnob (wtKnobs, "wt_warpamt", "Warp", col::wavetable, dstWtWarp);
    makeKnob (wtKnobs, "wt_unison", "Unison", col::wavetable);
    makeKnob (wtKnobs, "wt_detune", "Detune", col::wavetable, dstWtDetune);
    makeKnob (wtKnobs, "wt_spread", "Spread", col::wavetable);
    makeKnob (wtKnobs, "wt_oct", "Octave", col::wavetable);
    makeKnob (wtKnobs, "wt_semi", "Semi", col::wavetable, dstWtPitch);
    makeKnob (wtKnobs, "wt_fine", "Fine", col::wavetable);
    makeKnob (wtKnobs, "wt_pan", "Pan", col::wavetable);
    makeKnob (wtKnobs, "wt_level", "Level", col::wavetable, dstWtLevel);

    // ---------------- analog ----------------
    makeToggle (anOn, "an_on", "ANALOG", col::analog);
    addAndMakeVisible (anDisplay);
    makeKnob (anKnobs, "an_shape", "Shape", col::analog, dstAnShape);
    makeKnob (anKnobs, "an_pw", "Pulse W", col::analog, dstAnPw);
    makeKnob (anKnobs, "an_tone", "Osc", col::analog);
    makeKnob (anKnobs, "an_sub", "Sub", col::analog);
    makeKnob (anKnobs, "an_noise", "Noise", col::analog);
    makeKnob (anKnobs, "an_drift", "Drift", col::analog);
    makeKnob (anKnobs, "an_oct", "Octave", col::analog);
    makeKnob (anKnobs, "an_semi", "Semi", col::analog, dstAnPitch);
    makeKnob (anKnobs, "an_fine", "Fine", col::analog);
    makeKnob (anKnobs, "an_pan", "Pan", col::analog);
    makeKnob (anKnobs, "an_level", "Level", col::analog, dstAnLevel);

    // ---------------- granular ----------------
    makeToggle (grOn, "gr_on", "GRANULAR", col::granular);
    addAndMakeVisible (grLoad);
    grLoad.setTooltip ("Load any audio file to turn into texture (or drag one onto the display)");
    grLoad.onClick = [this] { loadGrainSample(); };
    makeCombo (grSource, "gr_source", grainSourceNames());
    grSource.setTooltip ("What the granular engine plays: a built-in texture or your own sample");
    addAndMakeVisible (grDisplay);
    grDisplay.onFileDropped = [this] (const juce::File& f) { loadGrainFile (f); };
    makeKnob (grKnobs, "gr_pos", "Position", col::granular, dstGrPos);
    makeKnob (grKnobs, "gr_scan", "Scan", col::granular);
    makeKnob (grKnobs, "gr_size", "Size", col::granular, dstGrSize);
    makeKnob (grKnobs, "gr_density", "Density", col::granular, dstGrDensity);
    makeKnob (grKnobs, "gr_spray", "Spray", col::granular, dstGrSpray);
    makeKnob (grKnobs, "gr_stereo", "Width", col::granular);
    makeKnob (grKnobs, "gr_pitchrand", "Pitch Rnd", col::granular);
    makeKnob (grKnobs, "gr_reverse", "Reverse", col::granular);
    makeKnob (grKnobs, "gr_oct", "Octave", col::granular);
    makeKnob (grKnobs, "gr_semi", "Semi", col::granular, dstGrPitch);
    makeKnob (grKnobs, "gr_level", "Level", col::granular, dstGrLevel);

    // ---------------- resonator ----------------
    makeToggle (resOn, "res_on", "RESONATOR", col::resonator);
    makeCombo (resType, "res_type", resonatorTypeNames());
    resType.setTooltip ("What is vibrating: strings, bowls, bells, drums, metal...");
    addAndMakeVisible (resDisplay);
    makeKnob (resKnobs, "res_decay", "Decay", col::resonator, dstResDecay);
    makeKnob (resKnobs, "res_bright", "Bright", col::resonator, dstResBright);
    makeKnob (resKnobs, "res_damp", "Damping", col::resonator);
    makeKnob (resKnobs, "res_strike", "Strike", col::resonator);
    makeKnob (resKnobs, "res_bow", "Bow/Rub", col::resonator, dstResBow);
    makeKnob (resKnobs, "res_buzz", "Buzz", col::resonator, dstResBuzz);
    makeKnob (resKnobs, "res_retrig", "Re-pluck", col::resonator);
    makeKnob (resKnobs, "res_oct", "Octave", col::resonator);
    makeKnob (resKnobs, "res_semi", "Semi", col::resonator, dstResPitch);
    makeKnob (resKnobs, "res_pan", "Pan", col::resonator);
    makeKnob (resKnobs, "res_level", "Level", col::resonator, dstResLevel);

    // ---------------- filter ----------------
    makeToggle (fltOn, "flt_on", "FILTER", col::filter);
    makeCombo (fltType, "flt_type", filterTypeNames());
    makeKnob (fltKnobs, "flt_cutoff", "Cutoff", col::filter, dstCutoff);
    makeKnob (fltKnobs, "flt_res", "Reso", col::filter, dstResonance);
    makeKnob (fltKnobs, "flt_drive", "Drive", col::filter, dstFltDrive);
    makeKnob (fltKnobs, "flt_env", "Env 2 Amt", col::filter);
    makeKnob (fltKnobs, "flt_key", "Key Trk", col::filter);

    // ---------------- envelopes ----------------
    const char* adsr[4] = { "_a", "_d", "_s", "_r" };
    const char* adsrLabel[4] = { "Attack", "Decay", "Sustain", "Release" };
    for (int i = 0; i < 4; ++i) makeKnob (ampKnobs, juce::String ("amp") + adsr[i], adsrLabel[i], col::text);
    makeKnob (ampKnobs, "amp_vel", "Velocity", col::text);
    for (int i = 0; i < 4; ++i) makeKnob (e2Knobs, juce::String ("e2") + adsr[i], adsrLabel[i], col::mod);
    for (int i = 0; i < 4; ++i) makeKnob (e3Knobs, juce::String ("e3") + adsr[i], adsrLabel[i], col::mod);

    // ---------------- LFOs ----------------
    for (int i = 0; i < 2; ++i)
    {
        const juce::String pre = "lfo" + juce::String (i + 1);
        makeCombo (lfoShape[i], pre + "_shape", lfoShapeNames());
        makeKnob (lfoKnobs, pre + "_rate", "Rate", col::mod, i == 0 ? dstLfo1Rate : dstLfo2Rate);
        makeKnob (lfoFadeKnobs, pre + "_fade", "Fade In", col::mod);
        makeToggle (lfoRetrig[i], pre + "_retrig", "RETRIG", col::mod);
        lfoRetrig[i].setTooltip ("On: the LFO restarts with every note. Off: it free-runs.");
    }

    // ---------------- macros ----------------

    // ---------------- mod matrix ----------------
    modStrip = std::make_unique<ff::gui::ModStrip> (proc);
    addAndMakeVisible (*modStrip);
    for (int i = 0; i < numModSlots; ++i)
    {
        auto& r = rows[(size_t) i];
        r.src.addItemList (modSourceNames(), 1);
        r.dst.addItemList (modDestNames(), 1);
        r.amt.setSliderStyle (juce::Slider::LinearHorizontal);
        r.amt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        r.amt.setColour (juce::Slider::trackColourId, col::mod);
        r.amt.getProperties().set ("bipolar", true);
        r.amt.setPopupDisplayEnabled (true, true, nullptr);
        r.amt.setDoubleClickReturnValue (true, 0.0);
        r.clear.setTooltip ("Clear this slot");
        r.clear.onClick = [this, i] { setParam (slotSrc (i), 0); setParam (slotDst (i), 0); setParam (slotAmt (i), 0); };
        for (juce::Component* c : { (juce::Component*) &r.src, (juce::Component*) &r.dst, (juce::Component*) &r.amt, (juce::Component*) &r.clear })
            addAndMakeVisible (c);
        r.srcA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, slotSrc (i), r.src);
        r.dstA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, slotDst (i), r.dst);
        r.amtA = std::make_unique<APVTS::SliderAttachment> (proc.apvts, slotAmt (i), r.amt);
    }

    // ---------------- fx ----------------
    makeCombo (driveType, "drive_type", driveTypeNames());
    driveType.setTooltip ("Drive character: Warm (tape), Rat (hard clip, mid bite), Fuzz");
    const std::pair<const char*, const char*> fxDefs[] = {
        { "fx_drive", "Drive" }, { "crush_bits", "Bits" }, { "crush_rate", "Downsample" },
        { "tape_wow", "Wow/Flutter" }, { "tape_age", "Tape Age" }, { "tape_hiss", "Hiss" },
        { "cho_mix", "Chorus" }, { "cho_rate", "Ch Rate" }, { "cho_depth", "Ch Depth" },
        { "ring_freq", "Ring Hz" }, { "ring_mix", "Ring Mix" }, { "rev_damp", "Rev Damp" },
        { "dly_mix", "Delay" }, { "dly_time", "Time" }, { "dly_fb", "Feedback" },
        { "dly_tone", "Tone" }, { "rev_mix", "Reverb" }, { "rev_size", "Size" } };
    for (auto& [id, label] : fxDefs) makeKnob (fxKnobs, id, label, col::fx);

    // ---------------- pages ----------------
    for (auto* t : { &synthTab, &patchTab, &fmTab, &envTab, &browseTab, &samplerTab, &rollTab })
    {
        t->setClickingTogglesState (false);
        t->setColour (juce::TextButton::buttonOnColourId, col::sampler.withAlpha (0.25f));
        addAndMakeVisible (t);
    }
    synthTab.onClick = [this] { showPage (0); };
    patchTab.onClick = [this] { showPage (1); };
    fmTab.onClick = [this] { showPage (2); };
    envTab.onClick = [this] { showPage (3); };
    browseTab.onClick = [this] { showPage (4); };
    samplerTab.onClick = [this] { showPage (5); };
    rollTab.onClick = [this] { showPage (6); };
    patchTab.setTooltip ("Signal flow: module slots on every engine, mod oscillator, noise, feedback and the dual filter");
    patchPage = std::make_unique<ff::gui::PatchPage> (proc, [this] (Knob& k) { registerPageKnob (k); });
    addChildComponent (*patchPage);
    fmTab.setTooltip ("FM engine: six operators and the FM matrix (FM8-style)");
    fmPage = std::make_unique<ff::gui::FmPage> (proc, [this] (Knob& k) { registerPageKnob (k); });
    addChildComponent (*fmPage);
    envTab.setTooltip ("Breakpoint envelopes (MSEG 1-4): draw shapes, loop them, sync them, send them anywhere");
    browseTab.setTooltip ("Preset browser, sound map, expansions and the mutator");
    envPage = std::make_unique<ff::gui::EnvelopePage> (proc);
    addChildComponent (*envPage);
    browsePage = std::make_unique<ff::gui::BrowsePage> (proc);
    addChildComponent (*browsePage);
    rollTab.setTooltip ("Draw notes as lines and bend them (per-note pitch, pressure and slide)");
    samplerTab.setTooltip ("Pad sampler: your own recordings, 4x4 pads and the mapper");
    samplerPage = std::make_unique<SamplerPage> (proc, proc.apvts);
    addChildComponent (*samplerPage);
    rollPage = std::make_unique<ff::gui::RollPage> (proc);
    addChildComponent (*rollPage);
    synthTab.setToggleState (true, juce::dontSendNotification);

    // ---------------- keyboard ----------------
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9dde6));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1a1c22));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, col::border);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::wavetable.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::wavetable.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);
    macroBar = std::make_unique<ff::gui::MacroBar> (proc, keyboard);
    addAndMakeVisible (*macroBar);

    setSize (editorW, editorH);
    setResizable (true, true);
    setResizeLimits (editorW * 3 / 4, editorH * 3 / 4, editorW * 2, editorH * 2);
    getConstrainer()->setFixedAspectRatio ((double) editorW / editorH);
    timerCallback();
    startTimerHz (30);
}

FourfoldEditor::~FourfoldEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
FourfoldEditor::Knob* FourfoldEditor::makeKnob (std::vector<std::unique_ptr<Knob>>& group, const juce::String& id,
                                                const juce::String& label, juce::Colour accent, int modDest)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, accent, modDest);
    if (modDest > 0) k->onModDropped = [this] (int s, int d) { assignModulation (s, d); };
    auto* raw = k.get();
    k->slider.onRightClick = [this, raw] { showModMenuFor (*raw); };
    addAndMakeVisible (*k);
    allKnobs.push_back (k.get());
    group.push_back (std::move (k));
    return group.back().get();
}

juce::ComboBox& FourfoldEditor::makeCombo (juce::ComboBox& box, const juce::String& id, const juce::StringArray& items)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    comboAttachments.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, box));
    return box;
}

juce::ToggleButton& FourfoldEditor::makeToggle (juce::ToggleButton& b, const juce::String& id, const juce::String& text, juce::Colour accent)
{
    b.setButtonText (text);
    b.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
    return b;
}

void FourfoldEditor::layoutGrid (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> area, int cols, int start, int count)
{
    if (count < 0) count = (int) knobs.size() - start;
    const int rowsN = (count + cols - 1) / cols;
    const int cw = area.getWidth() / cols;
    const int rh = area.getHeight() / juce::jmax (1, rowsN);
    for (int i = 0; i < count; ++i)
    {
        const int r = i / cols, c = i % cols;
        knobs[(size_t) (start + i)]->setBounds (area.getX() + c * cw, area.getY() + r * rh, cw, rh);
    }
}

// ---------------------------------------------------------------------------
void FourfoldEditor::paint (juce::Graphics& g)
{
    const float scale = (float) getWidth() / editorW;
    g.fillAll (col::bg);
    g.addTransform (juce::AffineTransform::scale (scale));

    // logo
    g.setColour (col::text);
    g.setFont (font (22.0f, true));
    const juce::String logo ("OZ-mosys");
    g.drawText (logo, logoArea, juce::Justification::centredLeft);
    const float logoW = juce::GlyphArrangement::getStringWidth (font (22.0f, true), logo);
    const juce::Colour dots[6] = { col::wavetable, col::analog, col::granular, col::resonator, ff::gui::fmCol, col::mod };
    for (int i = 0; i < 6; ++i)
    {
        g.setColour (dots[i]);
        g.fillEllipse ((float) logoArea.getX() + logoW + 7.0f + i * 8.5f, (float) logoArea.getCentreY() - 3.0f, 6.0f, 6.0f);
    }

    for (auto& pnl : panels)
    {
        auto r = pnl.r.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (pnl.accent);
        g.fillRoundedRectangle (r.getX() + 10.0f, r.getY(), 26.0f, 2.0f, 1.0f);
        if (pnl.title.isNotEmpty())
        {
            g.setColour (col::dim);
            g.setFont (font (11.0f, true));
            g.drawText (pnl.title, pnl.r.reduced (10, 6).removeFromTop (16), juce::Justification::centredLeft);
        }
    }

    // output meter
    auto m = meterArea.toFloat();
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (m, 3.0f);
    const float lvl = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load());
    const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
    const float frac = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
    g.setColour (lvl > 0.98f ? col::filter : col::mod);
    g.fillRoundedRectangle (m.withWidth (m.getWidth() * frac), 3.0f);
}

void FourfoldEditor::resized()
{
    // Everything is laid out at the design size and scaled as a whole.
    const float scale = (float) getWidth() / editorW;
    for (auto* c : getChildren())
        c->setTransform (juce::AffineTransform::scale (scale));

    panels.clear();
    const int gap = 8;
    auto area = juce::Rectangle<int> (0, 0, editorW, editorH).reduced (gap);

    // ---- top bar ----
    auto top = area.removeFromTop (46);
    logoArea = top.removeFromLeft (150);
    prevBtn.setBounds (top.removeFromLeft (28).reduced (0, 9));
    presetBox.setBounds (top.removeFromLeft (112).reduced (4, 9));
    nextBtn.setBounds (top.removeFromLeft (28).reduced (0, 9));
    top.removeFromLeft (6);
    saveBtn.setBounds (top.removeFromLeft (46).reduced (0, 9));
    top.removeFromLeft (8);
    synthTab.setBounds (top.removeFromLeft (54).reduced (0, 9));
    patchTab.setBounds (top.removeFromLeft (54).reduced (2, 9));
    fmTab.setBounds (top.removeFromLeft (38).reduced (0, 9));
    envTab.setBounds (top.removeFromLeft (46).reduced (2, 9));
    browseTab.setBounds (top.removeFromLeft (64).reduced (0, 9));
    samplerTab.setBounds (top.removeFromLeft (70).reduced (2, 9));
    rollTab.setBounds (top.removeFromLeft (46).reduced (0, 9));
    meterArea = top.removeFromRight (80).reduced (0, 19);
    top.removeFromRight (8);
    for (int i = 2; i >= 0; --i) topKnobs[(size_t) i]->setBounds (top.removeFromRight (52));
    top.removeFromRight (6);
    tuneKnobs[0]->setBounds (top.removeFromRight (52));
    top.removeFromRight (4);
    rootBox.setBounds (top.removeFromRight (48).reduced (0, 11));
    top.removeFromRight (4);
    scaleBox.setBounds (top.removeFromRight (96).reduced (0, 11));
    top.removeFromRight (8);
    monoToggle.setBounds (top.removeFromRight (58).reduced (0, 11));
    area.removeFromTop (4);

    // ---- engines row ----
    auto row1 = area.removeFromTop (282);
    const int engW = (row1.getWidth() - 3 * gap) / 4;
    auto wtR = row1.removeFromLeft (engW); row1.removeFromLeft (gap);
    auto anR = row1.removeFromLeft (engW); row1.removeFromLeft (gap);
    auto grR = row1.removeFromLeft (engW); row1.removeFromLeft (gap);
    auto rsR = row1;
    panels.push_back ({ wtR, {}, col::wavetable });
    panels.push_back ({ anR, {}, col::analog });
    panels.push_back ({ grR, {}, col::granular });
    panels.push_back ({ rsR, {}, col::resonator });

    {
        auto r = wtR.reduced (10, 8);
        auto head = r.removeFromTop (24);
        wtOn.setBounds (head.removeFromLeft (92));
        head.removeFromLeft (6);
        wtLoad.setBounds (head.removeFromRight (58));
        head.removeFromRight (4);
        wtTable.setBounds (head);
        r.removeFromTop (6);
        auto disp = r.removeFromTop (92);
        wtDisplay.setBounds (disp);
        r.removeFromTop (4);
        auto warpRow = r.removeFromTop (22);
        wtWarp.setBounds (warpRow.removeFromLeft (110));
        layoutGrid (wtKnobs, r, 5);
    }
    {
        auto r = anR.reduced (10, 8);
        auto head = r.removeFromTop (24);
        anOn.setBounds (head.removeFromLeft (96));
        r.removeFromTop (6);
        anDisplay.setBounds (r.removeFromTop (92));
        r.removeFromTop (26);
        layoutGrid (anKnobs, r, 6);
    }
    {
        auto r = grR.reduced (10, 8);
        auto head = r.removeFromTop (24);
        grOn.setBounds (head.removeFromLeft (92));
        grLoad.setBounds (head.removeFromRight (90));
        r.removeFromTop (6);
        grDisplay.setBounds (r.removeFromTop (92));
        r.removeFromTop (4);
        grSource.setBounds (r.removeFromTop (22).removeFromLeft (150));
        layoutGrid (grKnobs, r, 6);
    }
    {
        auto r = rsR.reduced (10, 8);
        auto head = r.removeFromTop (24);
        resOn.setBounds (head.removeFromLeft (100));
        head.removeFromLeft (6);
        resType.setBounds (head);
        r.removeFromTop (6);
        resDisplay.setBounds (r.removeFromTop (92));
        r.removeFromTop (4);
        layoutGrid (resKnobs, r.withTrimmedTop (22), 6);
    }
    area.removeFromTop (gap);

    // ---- filter / envelopes / LFOs ----
    auto row2 = area.removeFromTop (150);
    auto take = [&] (int w) { auto r = row2.removeFromLeft (w); row2.removeFromLeft (gap); return r; };
    auto fR = take (280), aR = take (250), e2R = take (180), e3R = take (180);
    const int lfoW = (row2.getWidth() - gap) / 2;
    auto l1R = row2.removeFromLeft (lfoW); row2.removeFromLeft (gap);
    auto l2R = row2;
    panels.push_back ({ fR, {}, col::filter });
    panels.push_back ({ aR, "AMP ENVELOPE", col::text });
    panels.push_back ({ e2R, "ENVELOPE 2", col::mod });
    panels.push_back ({ e3R, "ENVELOPE 3", col::mod });
    panels.push_back ({ l1R, "LFO 1", col::mod });
    panels.push_back ({ l2R, "LFO 2", col::mod });
    {
        auto r = fR.reduced (10, 8);
        auto head = r.removeFromTop (24);
        fltOn.setBounds (head.removeFromLeft (80));
        head.removeFromLeft (8);
        fltType.setBounds (head);
        r.removeFromTop (14);
        layoutGrid (fltKnobs, r.removeFromTop (78), 5);
    }
    auto envLayout = [] (std::vector<std::unique_ptr<Knob>>& k, juce::Rectangle<int> r, int cols)
    {
        r = r.reduced (8, 8);
        r.removeFromTop (34);
        layoutGrid (k, r.removeFromTop (78), cols);
    };
    envLayout (ampKnobs, aR, 5);
    envLayout (e2Knobs, e2R, 4);
    envLayout (e3Knobs, e3R, 4);
    for (int i = 0; i < 2; ++i)
    {
        auto r = (i == 0 ? l1R : l2R).reduced (8, 8);
        auto head = r.removeFromTop (18);
        lfoRetrig[i].setBounds (head.removeFromRight (52));
        r.removeFromTop (4);
        lfoShape[i].setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        auto kr = r.removeFromTop (78);
        lfoKnobs[(size_t) i]->setBounds (kr.removeFromLeft (kr.getWidth() / 2));
        lfoFadeKnobs[(size_t) i]->setBounds (kr);
    }
    area.removeFromTop (gap);

    // ---- macros / matrix / fx ----
    auto row3 = area.removeFromTop (240);
    auto fxR = row3.removeFromRight (440); row3.removeFromRight (gap);
    auto mxR = row3;
    panels.push_back ({ mxR, "MOD MATRIX  -  drag a source onto any knob", col::mod });
    panels.push_back ({ fxR, "EFFECTS", col::fx });
    {
        auto r = mxR.reduced (10, 8);
        r.removeFromTop (22);
        modStrip->setBounds (r.removeFromTop (44));
        r.removeFromTop (6);
        const int colW = (r.getWidth() - 10) / 2;
        const int rowH = r.getHeight() / 5;
        for (int i = 0; i < numModSlots; ++i)
        {
            auto cell = juce::Rectangle<int> (r.getX() + (i / 5) * (colW + 10), r.getY() + (i % 5) * rowH, colW, rowH).reduced (0, 3);
            auto& row = rows[(size_t) i];
            row.clear.setBounds (cell.removeFromRight (20));
            cell.removeFromRight (2);
            row.src.setBounds (cell.removeFromLeft (74));
            cell.removeFromLeft (3);
            row.dst.setBounds (cell.removeFromLeft (98));
            cell.removeFromLeft (3);
            row.amt.setBounds (cell);
        }
    }
    {
        auto r = fxR.reduced (8, 8);
        driveType.setBounds (r.removeFromTop (20).removeFromRight (90));
        r.removeFromTop (2);
        layoutGrid (fxKnobs, r, 6);
    }
    area.removeFromTop (gap);

    macroBar->setBounds (area);
    keyboard.setBounds (macroBar->keyboardArea());
    samplerPage->setBounds (juce::Rectangle<int> (gap, 46 + gap + 4, editorW - 2 * gap, area.getY() - gap - (46 + gap + 4)));
    rollPage->setBounds (samplerPage->getBounds());
    envPage->setBounds (samplerPage->getBounds());
    fmPage->setBounds (samplerPage->getBounds());
    patchPage->setBounds (samplerPage->getBounds());
    browsePage->setBounds (samplerPage->getBounds());
    keyboard.setKeyWidth (juce::jmax (12.0f, (float) keyboard.getWidth() / 50.0f));
}

// ---------------------------------------------------------------------------
void FourfoldEditor::timerCallback()
{
    wtDisplay.update();
    anDisplay.update();
    grDisplay.update();
    resDisplay.update();

    // mod rings: sum of amounts routed to each destination
    float sums[numModDests] {};
    for (int i = 0; i < numModSlots; ++i)
    {
        const int s = (int) proc.apvts.getRawParameterValue (slotSrc (i))->load();
        const int d = (int) proc.apvts.getRawParameterValue (slotDst (i))->load();
        if (s > 0 && d > 0 && d < numModDests) sums[d] += proc.apvts.getRawParameterValue (slotAmt (i))->load();
    }
    if (++midiTick % 6 == 0)   // MIDI learn badges, a few times a second
    {
        const auto links = proc.midiLearn->assignments();
        const auto learning = proc.midiLearn->learningParamId();
        auto upd = [&] (Knob* k)
        {
            int cc = -1;
            for (auto& a : links) if (a.paramId == k->paramId) { cc = a.cc; break; }
            k->setMidiState (cc, learning.isNotEmpty() && learning == k->paramId);
        };
        for (auto* k : allKnobs) upd (k);
        for (auto* k : macroBar->knobs) upd (k);
        macroBar->setLinks (links);
    }
    for (auto* k : allKnobs)
        if (k->dest > 0)
        {
            float amt = sums[k->dest];
            // pitch destinations span +-24 semitones; show them relative to the knob's range
            if (k->dest == dstWtPitch || k->dest == dstAnPitch || k->dest == dstGrPitch || k->dest == dstResPitch) amt *= 2.0f;
            if (k->dest == dstAnShape) amt *= 1.0f;
            k->setModAmount (juce::jlimit (-1.0f, 1.0f, amt));
        }

    if (! ignorePresetChange && presetBox.getText() != proc.currentPresetName)
    {
        juce::ScopedValueSetter<bool> svs (ignorePresetChange, true);
        presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
    }

    repaint (meterArea.transformedBy (juce::AffineTransform::scale ((float) getWidth() / editorW)).expanded (2));
}

// ---------------------------------------------------------------------------
void FourfoldEditor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void FourfoldEditor::assignModulation (int source, int dest)
{
    int freeSlot = -1;
    for (int i = 0; i < numModSlots; ++i)
    {
        const int s = (int) proc.apvts.getRawParameterValue (slotSrc (i))->load();
        const int d = (int) proc.apvts.getRawParameterValue (slotDst (i))->load();
        if (s == source && d == dest) return;   // already routed
        if (freeSlot < 0 && (s == 0 || d == 0)) freeSlot = i;
    }
    if (freeSlot < 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Mod matrix full",
                                                "All " + juce::String (numModSlots) + " slots are in use. Clear one with its x button first.");
        return;
    }
    setParam (slotSrc (freeSlot), (float) source);
    setParam (slotDst (freeSlot), (float) dest);
    setParam (slotAmt (freeSlot), 0.5f);
}

void FourfoldEditor::registerPageKnob (Knob& k)
{
    if (k.dest > 0) k.onModDropped = [this] (int s, int d) { assignModulation (s, d); };
    auto* raw = &k;
    k.slider.onRightClick = [this, raw] { showModMenuFor (*raw); };
    allKnobs.push_back (raw);
}

void FourfoldEditor::showModMenuFor (Knob& k)
{
    juce::PopupMenu menu;
    if (k.dest > 0)
    {
        menu.addSectionHeader ("Modulating " + modDestNames()[k.dest]);
        std::vector<int> slots;
        for (int i = 0; i < numModSlots; ++i)
        {
            const int s = (int) proc.apvts.getRawParameterValue (slotSrc (i))->load();
            const int d = (int) proc.apvts.getRawParameterValue (slotDst (i))->load();
            if (s > 0 && d == k.dest)
            {
                const float a = proc.apvts.getRawParameterValue (slotAmt (i))->load();
                menu.addItem (i + 1, "Remove " + modSourceNames()[s] + "  (" + juce::String (juce::roundToInt (a * 100)) + "%)");
                slots.push_back (i);
            }
        }
        if (slots.empty()) menu.addItem (-1, "Nothing yet - drag a mod source onto this knob", false, false);
    }
    ff::gui::addMidiItems (menu, *proc.midiLearn, k.paramId);
    juce::Component::SafePointer<Knob> safe (&k);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&k), [this, safe] (int r)
    {
        if (safe == nullptr) return;
        if (ff::gui::handleMidiItem (r, *proc.midiLearn, safe->paramId, safe.getComponent())) return;
        if (r > 0 && r <= numModSlots) { const int i = r - 1; setParam (slotSrc (i), 0); setParam (slotDst (i), 0); setParam (slotAmt (i), 0); }
    });
}

// ---------------------------------------------------------------------------
void FourfoldEditor::refreshPresetList()
{
    juce::ScopedValueSetter<bool> svs (ignorePresetChange, true);
    presetBox.clear (juce::dontSendNotification);
    const auto& f = factoryPresets();
    juce::String lastCategory;
    for (int i = 0; i < (int) f.size(); ++i)
    {
        if (f[(size_t) i].category != lastCategory)
        {
            lastCategory = f[(size_t) i].category;
            presetBox.addSectionHeading (lastCategory);
        }
        presetBox.addItem (f[(size_t) i].name, i + 1);
    }

    userPresetFiles.clear();
    for (auto& e : juce::RangedDirectoryIterator (FourfoldProcessor::getUserPresetFolder(), false, "*.ozpreset"))
        userPresetFiles.push_back (e.getFile());
    std::sort (userPresetFiles.begin(), userPresetFiles.end());
    if (! userPresetFiles.empty())
    {
        presetBox.addSectionHeading ("My Presets");
        for (int i = 0; i < (int) userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[(size_t) i].getFileNameWithoutExtension(), 1000 + i);
    }
    presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
}

void FourfoldEditor::presetChosen()
{
    if (ignorePresetChange) return;
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    if (id < 1000) proc.setCurrentProgram (id - 1);
    else if (id - 1000 < (int) userPresetFiles.size()) proc.loadPresetFromFile (userPresetFiles[(size_t) (id - 1000)]);
}

void FourfoldEditor::stepPreset (int delta)
{
    std::vector<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.push_back (presetBox.getItemId (i));
    if (ids.empty()) return;
    int idx = -1;
    for (int i = 0; i < (int) ids.size(); ++i)
        if (presetBox.getItemText (i) == proc.currentPresetName) idx = i;
    idx = (idx + delta + (int) ids.size()) % (int) ids.size();
    presetBox.setSelectedId (ids[(size_t) idx], juce::sendNotificationSync);
}

void FourfoldEditor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Save preset", FourfoldProcessor::getUserPresetFolder().getChildFile (proc.currentPresetName + ".ozpreset"), "*.ozpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("ozpreset")) f = f.withFileExtension ("ozpreset");
            proc.savePresetToFile (f);
            refreshPresetList();
        });
}

void FourfoldEditor::loadWavetable()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile() && ! proc.loadUserWavetable (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't import", "That file couldn't be read as a wavetable.");
    });
}

void FourfoldEditor::loadGrainSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sample for the granular engine", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) loadGrainFile (f);
    });
}

void FourfoldEditor::loadGrainFile (const juce::File& f)
{
    if (proc.loadGrainSample (f))
        setParam ("gr_on", 1.0f);
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load", "That audio file couldn't be read.");
}

void FourfoldEditor::showPage (int page)
{
    juce::Component* pages[] = { nullptr, patchPage.get(), fmPage.get(), envPage.get(), browsePage.get(), samplerPage.get(), rollPage.get() };
    juce::TextButton* tabs[] = { &synthTab, &patchTab, &fmTab, &envTab, &browseTab, &samplerTab, &rollTab };
    for (int i = 0; i < 7; ++i)
    {
        if (pages[i] != nullptr) { pages[i]->setVisible (i == page); if (i == page) pages[i]->toFront (true); }
        tabs[i]->setToggleState (i == page, juce::dontSendNotification);
    }
}
