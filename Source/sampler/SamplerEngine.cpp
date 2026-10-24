#include "SamplerEngine.h"

namespace ff
{
static constexpr int samplerVoices = 48;
static constexpr int chunk = 32;

SamplerEngine::SamplerEngine (juce::AudioProcessorValueTreeState& state, juce::MidiKeyboardState& keys)
    : keyboardState (keys), apvts (state)
{
    formats.registerBasicFormats();
    auto g = [this] (const char* id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    P.play = g ("smp_play"); P.level = g ("smp_level"); P.tune = g ("smp_tune"); P.fine = g ("smp_fine");
    P.pan = g ("smp_pan"); P.attack = g ("smp_attack"); P.decay = g ("smp_decay"); P.sustain = g ("smp_sustain");
    P.release = g ("smp_release"); P.letRing = g ("smp_letring"); P.vel = g ("smp_vel"); P.choke = g ("smp_choke");

    for (int i = 0; i < samplerVoices; ++i)
        synth.addVoice (new sol::SamplerVoice (settings));
    synth.addSound (new sol::AnySound());
    synth.setMinimumRenderingSubdivisionSize (chunk, false);
    retired.reserve (64);
    for (auto& gl : noteGlow) gl = 0.0f;

    rescanLibrary();
    if (! instrumentList.empty())
        loadNow (instrumentList.front());   // the small built-in demo until something else is picked
}

SamplerEngine::~SamplerEngine()
{
    loader.removeAllJobs (true, 10000);
}

void SamplerEngine::prepare (double sampleRate, int block)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    buffer.setSize (2, juce::jmax (block, 512));
    gainL = gainR = juce::Decibels::decibelsToGain (P.level->load());
}

void SamplerEngine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, int n)
{
    // a newly loaded instrument? silence the old one and swap it in
    if (hasPending.load() && swapLock.tryEnter())
    {
        synth.allNotesOff (0, false);
        if (current != nullptr && retired.size() < retired.capacity()) retired.push_back (std::move (current));
        current = std::move (pending);
        pending = nullptr;
        hasPending = false;
        swapLock.exit();
    }
    if (panicRequested.exchange (false)) synth.allNotesOff (0, false);

    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn()) lastNote = meta.getMessage().getNoteNumber();

    settings.instrument = current.get();
    settings.articulation = articulation.load();
    settings.attackSec = P.attack->load() / 1000.0f;
    settings.decaySec = P.decay->load() / 1000.0f;
    settings.sustain = P.sustain->load();
    settings.releaseSec = P.release->load();
    settings.letRing = P.letRing->load() > 0.5f;
    settings.velSens = P.vel->load();
    settings.bendRange = 2.0f;
    settings.tuneSemis = P.tune->load() + P.fine->load() / 100.0f;

    if (buffer.getNumSamples() < n) buffer.setSize (2, n, false, false, true);
    buffer.clear (0, n);
    if (P.choke->load() > 0.5f)
    {
        // choke: every new note fades out whatever is still playing, at the exact sample
        int cursor = 0;
        for (const auto meta : midi)
        {
            if (! meta.getMessage().isNoteOn()) continue;
            const int p = juce::jlimit (0, n, meta.samplePosition);
            if (p > cursor) { synth.renderNextBlock (buffer, midi, cursor, p - cursor); cursor = p; }
            for (int v = 0; v < synth.getNumVoices(); ++v)
                if (auto* sv = dynamic_cast<sol::SamplerVoice*> (synth.getVoice (v))) if (sv->isVoiceActive()) sv->choke();
        }
        if (cursor < n) synth.renderNextBlock (buffer, midi, cursor, n - cursor);
    }
    else
        synth.renderNextBlock (buffer, midi, 0, n);

    const float g = juce::Decibels::decibelsToGain (P.level->load());
    const float pan = juce::jlimit (-1.0f, 1.0f, P.pan->load());
    const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float gl = g * std::cos (a) * juce::MathConstants<float>::sqrt2, gr = g * std::sin (a) * juce::MathConstants<float>::sqrt2;
    if (std::abs (pan) < 1.0e-4f) gl = gr = g;

    auto* L = out.getWritePointer (0);
    auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const float* bl = buffer.getReadPointer (0);
    const float* br = buffer.getReadPointer (1);
    for (int k = 0; k < n; ++k)
    {
        const float t = (float) (k + 1) / (float) n;
        const float cl = gainL + (gl - gainL) * t, cr = gainR + (gr - gainR) * t;
        if (R != nullptr) { L[k] += bl[k] * cl; R[k] += br[k] * cr; }
        else              L[k] += 0.5f * (bl[k] * cl + br[k] * cr);
    }
    gainL = gl; gainR = gr;

    // for the pads: how loud each note is still ringing
    float glow[128] = {};
    for (int v = 0; v < synth.getNumVoices(); ++v)
        if (auto* voice = dynamic_cast<sol::SamplerVoice*> (synth.getVoice (v)); voice != nullptr && voice->isVoiceActive())
        {
            const int note = voice->getCurrentlyPlayingNote();
            if (note >= 0 && note < 128) glow[note] = juce::jmax (glow[note], voice->getLevel());
        }
    for (int k = 0; k < 128; ++k) noteGlow[(size_t) k] = glow[k];
}

// ------------------------------------------------------------------------------ library
void SamplerEngine::rescanLibrary()
{
    auto root = sol::Library::getLibraryFolder();
    sol::Library::ensureLibraryExists (root);
    instrumentList = sol::Library::scan (root);

    // tidy up instruments the audio thread has finished with (message thread only)
    const juce::SpinLock::ScopedLockType sl (swapLock);
    retired.clear();
}

int SamplerEngine::findInList (const juce::File& folder) const
{
    for (int i = 0; i < (int) instrumentList.size(); ++i)
        if (instrumentList[(size_t) i].folder == folder) return i;
    return -1;
}

void SamplerEngine::selectInstrument (int, int index)
{
    if (index >= 0 && index < (int) instrumentList.size())
        queueLoad (instrumentList[(size_t) index]);
}

void SamplerEngine::loadFolderIntoLayer (int, const juce::File& folder)
{
    rescanLibrary();
    if (int i = findInList (folder); i >= 0) selectInstrument (0, i);
    else queueLoad ({ folder.getFileName(), "Library", folder });
}

void SamplerEngine::reloadLayersUsing (const juce::File& folder)
{
    if (info.folder == folder) queueLoad (info);
}

void SamplerEngine::queueLoad (const sol::InstrumentInfo& newInfo)
{
    if (info.folder != newInfo.folder || info.name != newInfo.name) articulation = 0;
    info = newInfo;
    const int gen = ++generation;
    ++loadsInFlight;
    setStatus ("Loading " + newInfo.name + "...");
    loader.addJob ([this, newInfo, gen]
    {
        juce::String err;
        auto inst = sol::Library::load (newInfo, formats, err);
        hand (inst, err, gen);
        --loadsInFlight;
    });
}

void SamplerEngine::loadNow (const sol::InstrumentInfo& newInfo)
{
    info = newInfo;
    juce::String err;
    hand (sol::Library::load (newInfo, formats, err), err, ++generation);
}

void SamplerEngine::hand (sol::InstrumentPtr inst, const juce::String& err, int gen)
{
    if (gen != generation.load()) return;   // something newer was picked meanwhile
    std::lock_guard<std::mutex> g (displayMutex);
    if (inst == nullptr) { status = "Couldn't load that instrument. " + err; return; }
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        pending = inst;
        hasPending = true;
    }
    display = inst;
    status = err.isEmpty() ? juce::String() : ("Some files were skipped: " + err.replace ("\n", " "));
}

sol::InstrumentPtr SamplerEngine::getDisplayInstrument (int)
{
    std::lock_guard<std::mutex> g (displayMutex);
    return display;
}

juce::String SamplerEngine::getStatus()
{
    std::lock_guard<std::mutex> g (displayMutex);
    return status;
}

void SamplerEngine::setStatus (const juce::String& s)
{
    std::lock_guard<std::mutex> g (displayMutex);
    status = s;
}

// ------------------------------------------------------------------------------ state
void SamplerEngine::saveTo (juce::ValueTree& state) const
{
    juce::ValueTree s ("Sampler");
    s.setProperty ("folder", info.folder.getFullPathName(), nullptr);
    s.setProperty ("name", info.name, nullptr);
    s.setProperty ("category", info.category, nullptr);
    s.setProperty ("articulation", articulation.load(), nullptr);
    s.setProperty ("padBase", padBaseNote, nullptr);
    state.removeChild (state.getChildWithName ("Sampler"), nullptr);
    state.appendChild (s, nullptr);
}

void SamplerEngine::restoreFrom (const juce::ValueTree& state)
{
    auto s = state.getChildWithName ("Sampler");
    if (! s.isValid()) return;
    padBaseNote = juce::jlimit (0, 112, (int) s.getProperty ("padBase", 48));
    const juce::String path = s.getProperty ("folder").toString();
    const int art = (int) s.getProperty ("articulation", 0);
    if (path.isNotEmpty() && juce::File (path).isDirectory())
    {
        sol::InstrumentInfo i { s.getProperty ("name").toString(), s.getProperty ("category").toString(), juce::File (path) };
        if (i.folder != info.folder) queueLoad (i);
    }
    articulation = art;
}

} // namespace ff
