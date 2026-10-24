#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "design/SoundFeatures.h"

FourfoldProcessor::FourfoldProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FourfoldState", ff::createLayout())
{
    shared.p.bind (apvts);
    shared.modView = modView;

    // Built-in tables and textures are generated once and shared by every instance.
    static const auto tables = []
    {
        std::array<ff::WavetableBank::Ptr, ff::numBuiltInTables> t;
        for (int i = 0; i < ff::numBuiltInTables; ++i) t[(size_t) i] = ff::WavetableBank::createBuiltIn (i);
        return t;
    }();
    static const std::array<ff::GrainSource::Ptr, 4> grains = { ff::GrainSource::createBuiltIn(), ff::GrainSource::createMonkChant(),
                                                                ff::GrainSource::createBreath(), ff::GrainSource::createRain() };
    builtInTables = tables;
    builtInGrains = grains;

    for (int i = 0; i < 12; ++i)
    {
        auto* v = new ff::SynthVoice (shared, 1000 + i * 37);
        voices.push_back (v);
        synth.addVoice (v);
    }
    synth.setVoiceStealingEnabled (true);

    // Mono mode: one voice, so a new note cuts the last (hi-hat choke, 808 slides)
    {
        auto* v = new ff::SynthVoice (shared, 4242);
        voices.push_back (v);
        monoSynth.addVoice (v);
    }
    monoSynth.setVoiceStealingEnabled (true);
    updateZoneLayout (2);
    refreshSharedPointers();

    sampler = std::make_unique<ff::SamplerEngine> (apvts, keyboardState);
    for (int i = 0; i < numMsegs; ++i) setMseg (i, defaultMseg (i));
    mutator = std::make_unique<ff::design::Mutator> (*this);
    midiLearn = std::make_unique<ff::design::MidiLearn> (*this);
}

bool FourfoldProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void FourfoldProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    monoSynth.setCurrentPlaybackSampleRate (sampleRate);
    synth.setMinimumRenderingSubdivisionSize (32, false);
    monoSynth.setMinimumRenderingSubdivisionSize (32, false);
    fx.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    sampler->prepare (sampleRate, samplesPerBlock);
    roll.prepare (sampleRate);
    samplerMidi.ensureSize (8192);
    samplerBuf.setSize (2, juce::jmax (512, samplesPerBlock));
    keyboardState.reset();
}

// ---------------------------------------------------------------------------
void FourfoldProcessor::refreshSharedPointers()
{
    const juce::SpinLock::ScopedTryLockType lock (swapLock);
    if (! lock.isLocked()) return;   // the editor is swapping data right now; keep last block's

    const int t = (int) apvts.getRawParameterValue ("wt_table")->load();
    if (t >= ff::numBuiltInTables)
        audioTable = userTable != nullptr ? userTable : builtInTables[0];
    else
        audioTable = builtInTables[(size_t) juce::jlimit (0, ff::numBuiltInTables - 1, t)];

    audioGrains = selectedGrains();
}

ff::GrainSource::Ptr FourfoldProcessor::selectedGrains() const
{
    const int src = (int) apvts.getRawParameterValue ("gr_source")->load();
    if (src >= ff::grainSourceUser) return userGrains != nullptr ? userGrains : builtInGrains[0];
    return builtInGrains[(size_t) juce::jlimit (0, 3, src)];
}

void FourfoldProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    midiLearn->processMidi (midi);

    refreshSharedPointers();
    if (swapLock.tryEnter())
    {
        for (int i = 0; i < numMsegs; ++i) msegAudio[i] = msegShapes[i];
        swapLock.exit();
    }
    for (int i = 0; i < numMsegs; ++i) shared.mseg[i] = msegAudio[i].get();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) shared.bpm = *b;
    shared.table = audioTable.get();
    shared.grains = audioGrains.get();

    // free-running LFO phases (used when "retrigger" is off)
    const double secs = n / juce::jmax (1.0, getSampleRate());
    for (int i = 0; i < 2; ++i)
    {
        const float rate = apvts.getRawParameterValue ("lfo" + juce::String (i + 1) + "_rate")->load();
        shared.freeLfoPhase[i] = std::fmod (shared.freeLfoPhase[i] + (float) (rate * secs), 1.0f);
    }

    const bool mono = apvts.getRawParameterValue ("voice_mode")->load() > 0.5f;
    if (mono != wasMono)
    {
        synth.turnOffAllVoices (false);
        monoSynth.turnOffAllVoices (false);
        wasMono = mono;
    }
    // ROLL page: drawn lines become MPE notes (each on its own channel)
    roll.process (midi, n, getPlayHead(), (int) apvts.getRawParameterValue ("bend")->load());

    // MPE: channel 1 = normal keyboard (Bend knob range), channels 2-16 = one note each (+/-48 st)
    if (const int bendRange = (int) apvts.getRawParameterValue ("bend")->load(); bendRange != lastMasterBend)
        updateZoneLayout (bendRange);
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController() && m.getControllerNumber() == 1) shared.modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isChannelPressure() && m.getChannel() == 1) shared.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    }

    // sampler page: off / layered with the synth / sampler only
    const int smpPlay = (int) apvts.getRawParameterValue ("smp_play")->load();
    (mono ? monoSynth : synth).renderNextBlock (buffer, smpPlay == 2 ? emptyMidi : midi, 0, n);
    {
        auto& s = mono ? monoSynth : synth;
        int active = 0;
        for (int v = 0; v < s.getNumVoices(); ++v) if (s.getVoice (v)->isActive()) ++active;
        activeVoices.store (active, std::memory_order_relaxed);
    }

    const bool smpThroughFx = apvts.getRawParameterValue ("smp_fx")->load() > 0.5f;
    if (samplerBuf.getNumSamples() < n) samplerBuf.setSize (2, n, false, false, true);
    samplerBuf.clear (0, n);
    // the sampler plays the ROLL's notes at their starting pitch (its bend range is the normal one)
    samplerMidi.clear();
    if (smpPlay > 0)
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isPitchWheel() && m.getChannel() > 1) continue;
            samplerMidi.addEvent (m, meta.samplePosition);
        }
    sampler->render (samplerBuf, samplerMidi, n);
    if (smpThroughFx)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.addFrom (ch, 0, samplerBuf, juce::jmin (ch, 1), 0, n);

    if (buffer.getNumChannels() >= 2)
    {
        auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        ff::EffectsChain::Settings s;
        s.drive = g ("fx_drive");
        s.driveType = (int) g ("drive_type");
        s.crushBits = g ("crush_bits"); s.crushRate = g ("crush_rate");
        s.tapeWow = g ("tape_wow"); s.tapeHiss = g ("tape_hiss"); s.tapeAge = g ("tape_age");
        s.ringFreq = g ("ring_freq"); s.ringMix = g ("ring_mix");
        s.eqBass = g ("eq_bass"); s.eqTreble = g ("eq_treble");
        s.choMix = g ("cho_mix"); s.choRate = g ("cho_rate"); s.choDepth = g ("cho_depth");
        s.dlyMix = g ("dly_mix"); s.dlyTimeMs = g ("dly_time"); s.dlyFb = g ("dly_fb"); s.dlyTone = g ("dly_tone");
        s.revMix = g ("rev_mix"); s.revSize = g ("rev_size"); s.revDamp = g ("rev_damp");
        fx.process (buffer, s);
    }

    if (! smpThroughFx)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.addFrom (ch, 0, samplerBuf, juce::jmin (ch, 1), 0, n);

    const float master = apvts.getRawParameterValue ("master")->load();
    buffer.applyGain (master * master);

    // safety: catch any runaway values before they reach the speakers
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = std::isfinite (d[i]) ? juce::jlimit (-2.0f, 2.0f, d[i]) : 0.0f;
    }

    outputLevel = juce::jmax (outputLevel.load() * 0.85f, buffer.getMagnitude (0, n));
}

// ---------------------------------------------------------------------------
bool FourfoldProcessor::loadUserWavetable (const juce::File& file)
{
    auto t = ff::WavetableBank::fromAudioFile (file);
    if (t == nullptr) return false;
    {
        const juce::SpinLock::ScopedLockType lock (swapLock);
        if (userTable != nullptr) graveyard.add (userTable.get());
        userTable = t;
    }
    userTableFile = file;
    if (auto* p = apvts.getParameter ("wt_table"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) ff::numBuiltInTables));
    return true;
}

bool FourfoldProcessor::loadGrainSample (const juce::File& file)
{
    auto g = ff::GrainSource::fromFile (file);
    if (g == nullptr) return false;
    {
        const juce::SpinLock::ScopedLockType lock (swapLock);
        if (userGrains != nullptr) graveyard.add (userGrains.get());
        userGrains = g;
    }
    grainFile = file;
    if (auto* p = apvts.getParameter ("gr_source"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) ff::grainSourceUser));
    return true;
}

void FourfoldProcessor::resetGrainSample()
{
    const juce::SpinLock::ScopedLockType lock (swapLock);
    if (userGrains != nullptr) graveyard.add (userGrains.get());
    userGrains = nullptr;
    grainFile = juce::File();
}

juce::String FourfoldProcessor::getUserTableName() const { return userTable != nullptr ? userTable->name : juce::String ("(none loaded)"); }
juce::String FourfoldProcessor::getGrainSourceName() const { return selectedGrains()->name; }

ff::WavetableBank::Ptr FourfoldProcessor::getDisplayTable() const
{
    const juce::SpinLock::ScopedLockType lock (swapLock);
    const int t = (int) apvts.getRawParameterValue ("wt_table")->load();
    if (t >= ff::numBuiltInTables) return userTable != nullptr ? userTable : builtInTables[0];
    return builtInTables[(size_t) juce::jlimit (0, ff::numBuiltInTables - 1, t)];
}

ff::GrainSource::Ptr FourfoldProcessor::getDisplayGrains() const
{
    const juce::SpinLock::ScopedLockType lock (swapLock);
    return selectedGrains();
}

float FourfoldProcessor::getLiveWtPos() const
{
    for (auto* v : voices) { const float p = v->displayWtPos.load(); if (p >= 0.0f) return p; }
    return -1.0f;
}

float FourfoldProcessor::getLiveGrPos() const
{
    for (auto* v : voices) { const float p = v->displayGrPos.load(); if (p >= 0.0f) return p; }
    return -1.0f;
}

// ---------------------------------------------------------------------------
int FourfoldProcessor::getNumPrograms() { return (int) ff::factoryPresets().size(); }

const juce::String FourfoldProcessor::getProgramName (int index)
{
    const auto& l = ff::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) l.size()) ? l[(size_t) index].name : juce::String();
}

void FourfoldProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    ff::applyFactoryPreset (apvts, index);
    const auto& fp = ff::factoryPresets()[(size_t) index];
    for (int i = 0; i < numMsegs; ++i) setMseg (i, defaultMseg (i));
    if (fp.mseg1.isNotEmpty()) setMseg (0, ff::MsegShape::fromString (fp.mseg1));
    if (fp.mseg2.isNotEmpty()) setMseg (1, ff::MsegShape::fromString (fp.mseg2));
    for (int i = 0; i < 8; ++i) macroNames.set (i, "Macro " + juce::String (i + 1));
    currentPresetName = getProgramName (index);
    presetInfo = { "OZ-mosys", fp.category, fp.category.toLowerCase().replace (" & ", ", "), {}, fp.mapX, fp.mapY, fp.movement, true };
}

// ---------------------------------------------------------------------------
void FourfoldProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userTableFile", userTableFile.getFullPathName(), nullptr);
    state.setProperty ("grainFile", grainFile.getFullPathName(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    sampler->saveTo (state);
    state.removeChild (state.getChildWithName ("Msegs"), nullptr);
    state.appendChild (msegsToTree(), nullptr);
    state.removeChild (state.getChildWithName ("Roll"), nullptr);
    state.appendChild (roll.getPattern()->toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FourfoldProcessor::applyStateTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (apvts.state.getType())) return;
    apvts.replaceState (tree);

    const juce::File wt (tree.getProperty ("userTableFile").toString());
    if (wt.existsAsFile()) { auto t = ff::WavetableBank::fromAudioFile (wt); if (t != nullptr) { const juce::SpinLock::ScopedLockType l (swapLock); userTable = t; userTableFile = wt; } }

    const juce::String gpath = tree.getProperty ("grainFile").toString();
    if (gpath.isNotEmpty() && juce::File (gpath).existsAsFile()) loadGrainSample (juce::File (gpath));
    else resetGrainSample();

    currentPresetName = tree.getProperty ("presetName", "Init").toString();
    sampler->restoreFrom (tree);
    msegsFromTree (tree.getChildWithName ("Msegs"));
    if (auto rt = tree.getChildWithName ("Roll"); rt.isValid())
        roll.setPattern (ff::roll::Pattern::fromTree (rt));
}

void FourfoldProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyStateTree (juce::ValueTree::fromXml (*xml));
}

juce::File FourfoldProcessor::getUserPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("OZ-mosys").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

bool FourfoldProcessor::savePresetToFile (const juce::File& file)
{
    currentPresetName = file.getFileNameWithoutExtension();
    juce::MemoryBlock mb;
    auto state = apvts.copyState();
    state.setProperty ("userTableFile", userTableFile.getFullPathName(), nullptr);
    state.setProperty ("grainFile", grainFile.getFullPathName(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    sampler->saveTo (state);
    state.removeChild (state.getChildWithName ("Msegs"), nullptr);
    state.appendChild (msegsToTree(), nullptr);
    if (! presetInfo.measured) measureCurrentSound();
    state.setProperty ("author", presetInfo.author, nullptr);
    state.setProperty ("bank", presetInfo.bank, nullptr);
    state.setProperty ("tags", presetInfo.tags, nullptr);
    state.setProperty ("notes", presetInfo.notes, nullptr);
    state.setProperty ("mapX", presetInfo.mapX, nullptr);
    state.setProperty ("mapY", presetInfo.mapY, nullptr);
    state.setProperty ("movement", presetInfo.movement, nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

void FourfoldProcessor::measureCurrentSound()
{
    const auto audio = ff::design::renderAudition (*this, 48000.0);
    const auto f = ff::design::analyse (audio, 48000.0);
    ff::placeOnMap (f.brightness, f.aggression, f.movement, presetInfo.mapX, presetInfo.mapY, presetInfo.movement);
    presetInfo.measured = true;
}

juce::String FourfoldProcessor::getDefaultAuthor()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "OZ-mosys"; o.folderName = "OZ-mosys"; o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile f (o);
    return f.getValue ("defaultAuthor", juce::SystemStats::getFullUserName());
}

void FourfoldProcessor::setDefaultAuthor (const juce::String& a)
{
    juce::PropertiesFile::Options o;
    o.applicationName = "OZ-mosys"; o.folderName = "OZ-mosys"; o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile f (o);
    f.setValue ("defaultAuthor", a);
    f.saveIfNeeded();
}

bool FourfoldProcessor::loadPresetFromFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        applyStateTree (tree);
        currentPresetName = file.getFileNameWithoutExtension();
        presetInfo.author = tree["author"].toString();
        presetInfo.bank = tree.getProperty ("bank", "My Presets").toString();
        presetInfo.tags = tree["tags"].toString();
        presetInfo.notes = tree["notes"].toString();
        presetInfo.mapX = (float) tree.getProperty ("mapX", 0.5f);
        presetInfo.mapY = (float) tree.getProperty ("mapY", 0.5f);
        presetInfo.movement = (float) tree.getProperty ("movement", 0.5f);
        presetInfo.measured = tree.hasProperty ("mapX");
        return true;
    }
    return false;
}

void FourfoldProcessor::setMseg (int index, const ff::MsegShape& shape)
{
    auto copy = shape;
    copy.sanitise();
    auto p = std::make_shared<const ff::MsegShape> (copy);
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        msegShapes[juce::jlimit (0, numMsegs - 1, index)] = p;
    }
    msegKeep.push_back (p);
    while (msegKeep.size() > 64) msegKeep.pop_front();
}

ff::MsegShape FourfoldProcessor::defaultMseg (int index)
{
    ff::MsegShape m;
    switch (index)
    {
        case 0: m = ff::MsegShape::swell(); break;
        case 1: m = ff::MsegShape::rhythm(); break;
        case 2: m.points = { { 0.0, 0.0f, -0.3f }, { 2.0, 1.0f, 0.0f } }; break;
        default: m.points = { { 0.0, 1.0f, 0.7f }, { 0.4, 0.15f, 0.3f }, { 1.5, 0.0f, 0.0f } }; break;
    }
    m.name = "MSEG " + juce::String (index + 1);
    m.sanitise();
    return m;
}

ff::MsegPtr FourfoldProcessor::getMseg (int index) const
{
    const juce::SpinLock::ScopedLockType sl (swapLock);
    return msegShapes[juce::jlimit (0, numMsegs - 1, index)];
}

juce::ValueTree FourfoldProcessor::msegsToTree() const
{
    juce::ValueTree t ("Msegs");
    for (int i = 0; i < numMsegs; ++i)
    {
        t.setProperty ("m" + juce::String (i + 1), getMseg (i)->toString(), nullptr);
        t.setProperty ("n" + juce::String (i + 1), getMseg (i)->name, nullptr);
    }
    t.setProperty ("macroNames", macroNames.joinIntoString ("|"), nullptr);
    return t;
}

void FourfoldProcessor::msegsFromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    for (int i = 0; i < numMsegs; ++i)
    {
        const juce::String key = "m" + juce::String (i + 1);
        auto m = t.hasProperty (key) ? ff::MsegShape::fromString (t[key].toString()) : defaultMseg (i);
        m.name = t.getProperty ("n" + juce::String (i + 1), m.name).toString();
        setMseg (i, m);
    }
    if (t.hasProperty ("macroNames"))
    {
        auto names = juce::StringArray::fromTokens (t["macroNames"].toString(), "|", "");
        for (int i = 0; i < juce::jmin (8, names.size()); ++i) macroNames.set (i, names[i]);
    }
}

void FourfoldProcessor::updateZoneLayout (int masterBendRange)
{
    lastMasterBend = masterBendRange;
    juce::MPEZoneLayout layout;
    layout.setLowerZone (15, 48, juce::jmax (0, masterBendRange));
    synth.setZoneLayout (layout);
    monoSynth.setZoneLayout (layout);
}

juce::AudioProcessorEditor* FourfoldProcessor::createEditor() { return new FourfoldEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FourfoldProcessor(); }
