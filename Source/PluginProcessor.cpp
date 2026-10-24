#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

FourfoldProcessor::FourfoldProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FourfoldState", ff::createLayout())
{
    shared.p.bind (apvts);

    for (int i = 0; i < ff::numBuiltInTables; ++i)
        builtInTables[(size_t) i] = ff::WavetableBank::createBuiltIn (i);
    builtInGrains = ff::GrainSource::createBuiltIn();

    synth.addSound (new ff::SynthSound());
    for (int i = 0; i < 12; ++i)
    {
        auto* v = new ff::SynthVoice (shared, 1000 + i * 37);
        voices.push_back (v);
        synth.addVoice (v);
    }
    synth.setNoteStealingEnabled (true);
    refreshSharedPointers();
}

bool FourfoldProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void FourfoldProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    fx.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
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

    audioGrains = userGrains != nullptr ? userGrains : builtInGrains;
}

void FourfoldProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    refreshSharedPointers();
    shared.table = audioTable.get();
    shared.grains = audioGrains.get();

    // free-running LFO phases (used when "retrigger" is off)
    const double secs = n / juce::jmax (1.0, getSampleRate());
    for (int i = 0; i < 2; ++i)
    {
        const float rate = apvts.getRawParameterValue ("lfo" + juce::String (i + 1) + "_rate")->load();
        shared.freeLfoPhase[i] = std::fmod (shared.freeLfoPhase[i] + (float) (rate * secs), 1.0f);
    }

    synth.renderNextBlock (buffer, midi, 0, n);

    if (buffer.getNumChannels() >= 2)
    {
        auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        ff::EffectsChain::Settings s;
        s.drive = g ("fx_drive");
        s.choMix = g ("cho_mix"); s.choRate = g ("cho_rate"); s.choDepth = g ("cho_depth");
        s.dlyMix = g ("dly_mix"); s.dlyTimeMs = g ("dly_time"); s.dlyFb = g ("dly_fb"); s.dlyTone = g ("dly_tone");
        s.revMix = g ("rev_mix"); s.revSize = g ("rev_size"); s.revDamp = g ("rev_damp");
        fx.process (buffer, s);
    }

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
juce::String FourfoldProcessor::getGrainSourceName() const { return userGrains != nullptr ? userGrains->name : builtInGrains->name; }

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
    return userGrains != nullptr ? userGrains : builtInGrains;
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
    currentPresetName = getProgramName (index);
}

// ---------------------------------------------------------------------------
void FourfoldProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userTableFile", userTableFile.getFullPathName(), nullptr);
    state.setProperty ("grainFile", grainFile.getFullPathName(), nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
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
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

bool FourfoldProcessor::loadPresetFromFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        applyStateTree (juce::ValueTree::fromXml (*xml));
        currentPresetName = file.getFileNameWithoutExtension();
        return true;
    }
    return false;
}

juce::AudioProcessorEditor* FourfoldProcessor::createEditor() { return new FourfoldEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FourfoldProcessor(); }
