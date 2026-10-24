#include "SynthVoice.h"

namespace ff
{
static inline float rd (std::atomic<float>* p) noexcept { return p->load (std::memory_order_relaxed); }
static inline float semisToRatio (float s) noexcept { return std::exp2 (s / 12.0f); }

void SynthVoice::setCurrentSampleRate (double sr)
{
    juce::MPESynthesiserVoice::setCurrentSampleRate (sr);
    if (sr <= 0) return;
    sampleRate = sr;
    ampEnv.setSampleRate (sr); env2.setSampleRate (sr); env3.setSampleRate (sr);
    filter.setSampleRate (sr);
    cloud.prepare (sr);
    fm.prepare (sr);
    filter2.setSampleRate (sr);
    for (auto& e : slots) for (auto& s : e) s.prepare (sr);
    resonator.prepare (sr);
}

void SynthVoice::noteStarted()
{
    auto& p = shared.p;
    const int midiNote = currentlyPlayingNote.initialNote;
    velocity = currentlyPlayingNote.noteOnVelocity.asUnsignedFloat();
    switch ((int) rd (p.velCurve))
    {
        case 1: velocity = std::sqrt (velocity); break;   // soft touch: loud comes easily
        case 2: velocity = velocity * velocity; break;    // hard touch
        case 3: velocity = 1.0f; break;                   // fixed
        default: break;
    }
    bendReset = true;
    // tuning: A4 reference + per-scale-degree offsets (ragas, maqamat, just)
    const int root = (int) rd (p.scaleRoot);
    tuneOffset = 12.0f * std::log2 (rd (p.tuneA4) / 440.0f)
               + scaleOffsetCents ((int) rd (p.scale), midiNote - root) * 0.01f;
    targetNote = (float) midiNote + tuneOffset;
    noteTime = 0.0f;
    const bool glideOn = rd (p.glide) > 0.001f && shared.lastNote >= 0.0f;
    note = glideOn ? shared.lastNote : targetNote;
    glideRate = std::abs (targetNote - note) / juce::jmax (0.001f, rd (p.glide));
    shared.lastNote = targetNote;

    randomValue = rng.nextFloat() * 2.0f - 1.0f;
    driftValue = 0.0f; driftTarget = rng.nextFloat() * 2.0f - 1.0f;
    scanOffset = 0.0f;

    // random start phases give the unison/analog layers a natural, un-phasey start
    for (auto& ph : wtPhase) ph = rng.nextFloat();
    analog.reset (rng.nextFloat());
    filter.reset();
    filter2.reset();
    fbState[0] = fbState[1] = 0.0f;
    moPhaseAcc = 0.0;
    for (auto& e : slots) for (auto& s : e) s.reset();
    cloud.reset (rng);
    resonator.noteOn (rng);
    fm.noteOn (fmSettings(), velocity, rng);
    for (auto& m : msegPlayer) m.start();

    for (int i = 0; i < 2; ++i)
    {
        lfo[i].reset (0.0f, rng);
        if (rd (p.lfoRetrig[i]) < 0.5f) lfo[i].phase = shared.freeLfoPhase[i];
    }

    Envelope* envs[3] = { &ampEnv, &env2, &env3 };
    for (int e = 0; e < 3; ++e)
    {
        envs[e]->reset();
        envs[e]->set (rd (p.env[e][0]), rd (p.env[e][1]), rd (p.env[e][2]), rd (p.env[e][3]));
        envs[e]->noteOn();
    }
}

void SynthVoice::noteStopped (bool allowTailOff)
{
    if (allowTailOff)
    {
        for (auto& m : msegPlayer) m.release();
        ampEnv.noteOff(); env2.noteOff(); env3.noteOff();
        fm.noteOff();
    }
    else
    {
        ampEnv.reset();
        clearCurrentNote();
        displayWtPos = -1.0f; displayGrPos = -1.0f;
    }
}


FmVoice::Settings SynthVoice::fmSettings() const
{
    auto& p = shared.p;
    FmVoice::Settings s;
    for (int i = 0; i < numFmOps; ++i)
    {
        auto& o = s.op[(size_t) i];
        o.ratio = rd (p.fmRatio[i]); o.fineCents = rd (p.fmFine[i]); o.level = rd (p.fmOpLevel[i]);
        o.wave = (int) rd (p.fmWave[i]); o.velSens = rd (p.fmVel[i]);
        o.a = rd (p.fmEnv[i][0]); o.d = rd (p.fmEnv[i][1]); o.s = rd (p.fmEnv[i][2]); o.r = rd (p.fmEnv[i][3]);
        for (int j = 0; j < numFmOps; ++j) s.matrix[i][j] = rd (p.fmMatrix[i][j]);
    }
    s.index = rd (p.fmIndex);
    return s;
}

float SynthVoice::warpPhase (float p, int mode, float amt) const noexcept
{
    if (amt <= 0.0001f || mode == 0) return p;
    switch (mode)
    {
        case 1: { const float q = p * (1.0f + amt * 7.0f); return q - std::floor (q); }          // Sync
        case 2: return std::pow (p, 1.0f / (1.0f + amt * 3.0f));                                    // Bend +
        case 3: return std::pow (p, 1.0f + amt * 3.0f);                                             // Bend -
        case 4: { const float m = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; return p + (m - p) * amt; } // Mirror
        case 5: { const float w = 0.5f - 0.48f * amt;                                               // Asym (CZ-style)
                  return p < w ? 0.5f * p / w : 0.5f + 0.5f * (p - w) / (1.0f - w); }
        case 6: { const float q = p * (1.0f + amt * 7.0f); return q - std::floor (q); }          // Formant (windowed below)
        case 7: { const float steps = 64.0f - amt * 60.0f; return std::floor (p * steps) / steps; } // Steps (lo-fi)
        default: return p;
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! isActive()) return;

    float tmpL[32], tmpR[32];
    while (num > 0)
    {
        const int n = juce::jmin (32, num);
        std::fill (tmpL, tmpL + n, 0.0f);
        std::fill (tmpR, tmpR + n, 0.0f);
        renderChunk (tmpL, tmpR, n);

        if (out.getNumChannels() >= 2)
        {
            out.addFrom (0, start, tmpL, n);
            out.addFrom (1, start, tmpR, n);
        }
        else
        {
            for (int i = 0; i < n; ++i) tmpL[i] = 0.5f * (tmpL[i] + tmpR[i]);
            out.addFrom (0, start, tmpL, n);
        }

        start += n; num -= n;
        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            displayWtPos = -1.0f; displayGrPos = -1.0f;
            return;
        }
    }
}

void SynthVoice::renderChunk (float* outL, float* outR, int n)
{
    auto& p = shared.p;
    const float secs = (float) n / (float) sampleRate;

    // ---- envelopes may have been tweaked live ----
    Envelope* envs[3] = { &ampEnv, &env2, &env3 };
    for (int e = 0; e < 3; ++e)
        envs[e]->set (rd (p.env[e][0]), rd (p.env[e][1]), rd (p.env[e][2]), rd (p.env[e][3]));

    // ---- modulation sources (control rate) ----
    float src[numModSources] {};
    src[srcEnv2] = env2.getValue();
    src[srcEnv3] = env3.getValue();
    src[srcVelocity] = velocity;
    src[srcModWheel] = shared.modWheel;
    src[srcAftertouch] = juce::jmax (currentlyPlayingNote.pressure.asUnsignedFloat(), shared.aftertouch);
    src[srcSlide] = juce::jlimit (-1.0f, 1.0f, (currentlyPlayingNote.timbre.asUnsignedFloat() - 0.5f) * 2.0f);
    src[srcKeyTrack] = juce::jlimit (-1.0f, 1.0f, (targetNote - 60.0f) / 36.0f);
    src[srcRandom] = randomValue;
    for (int i = 0; i < 4; ++i) src[srcMacro1 + i] = rd (p.macro[i]);
    for (int i = 0; i < 4; ++i)
        src[srcMseg1 + i] = shared.mseg[i] != nullptr ? msegPlayer[i].advance (*shared.mseg[i], secs, shared.bpm) : 0.0f;
    for (int i = 4; i < 8; ++i) src[srcMacro5 + i - 4] = rd (p.macro[i]);

    // First pass for LFO-rate destinations (an LFO can't modulate itself here)
    float mod[numModDests] {};
    auto accumulate = [&] (bool includeLfos)
    {
        std::fill (std::begin (mod), std::end (mod), 0.0f);
        for (int s = 0; s < numModSlots; ++s)
        {
            const int si = (int) rd (p.modSrc[s]);
            const int di = (int) rd (p.modDst[s]);
            if (si <= 0 || di <= 0 || si >= numModSources || di >= numModDests) continue;
            if (! includeLfos && (si == srcLfo1 || si == srcLfo2)) continue;
            mod[di] += rd (p.modAmt[s]) * src[si];
        }
    };
    accumulate (false);
    noteTime += secs;
    for (int i = 0; i < 2; ++i)
    {
        const float rate = rd (p.lfoRate[i]) * std::exp2 ((i == 0 ? mod[dstLfo1Rate] : mod[dstLfo2Rate]) * 4.0f);
        const float fade = rd (p.lfoFade[i]);
        const float fadeGain = fade > 0.001f ? juce::jmin (1.0f, noteTime / fade) : 1.0f;
        src[srcLfo1 + i] = lfo[i].advance ((int) rd (p.lfoShape[i]), rate, secs, rng) * fadeGain;
    }
    accumulate (true);
    if (shared.modView != nullptr)
        for (int i = 1; i < numModSources; ++i) shared.modView[i].store (src[i], std::memory_order_relaxed);

    // ---- pitch ----
    const float glideT = rd (p.glide);
    const int glideCurve = (int) rd (p.glideCurve);
    if (glideT > 0.001f)
    {
        if (glideCurve == 0)
        {
            const float coef = std::exp (-secs / (glideT * 0.35f));
            note = targetNote + (note - targetNote) * coef;
        }
        else
        {
            const float step = glideRate * secs;
            note = std::abs (targetNote - note) <= step ? targetNote : note + (targetNote > note ? step : -step);
        }
    }
    else note = targetNote;
    const float playNote = (glideCurve == 2 && glideT > 0.001f) ? targetNote + std::round (note - targetNote) : note;

    // slow random drift for the analog engine
    driftValue += (driftTarget - driftValue) * juce::jmin (1.0f, secs * 1.5f);
    if (std::abs (driftTarget - driftValue) < 0.05f) driftTarget = rng.nextFloat() * 2.0f - 1.0f;

    // per-note bend (MPE) plus the master pitch wheel, already in semitones; lightly smoothed
    const float bendTarget = (float) currentlyPlayingNote.totalPitchbendInSemitones;
    if (bendReset) { bendNow = bendTarget; bendReset = false; }
    bendNow += (bendTarget - bendNow) * 0.6f;
    const float bendSemis = bendNow;
    const float basePitch = playNote + bendSemis + mod[dstPitchAll] * 24.0f;
    const float sr = (float) sampleRate;
    auto hzFor = [] (float midi) { return 440.0f * std::exp2 ((midi - 69.0f) / 12.0f); };

    float L[32] {}, R[32] {};

    // ---- modulation oscillator (Massive-style), one sine per voice ----
    float moBuf[32] {};
    const float moRing = rd (p.moRing), moPhaseAmt = rd (p.moPhase), moPosAmt = rd (p.moPos), moFfm = rd (p.moFfm);
    const bool moActive = moRing > 0.0001f || moPhaseAmt > 0.0001f || moPosAmt > 0.0001f || moFfm > 0.0001f;
    if (moActive)
    {
        const double inc = hzFor (basePitch + rd (p.moPitch) + mod[dstMoPitch] * 24.0f) / sampleRate;
        for (int i = 0; i < n; ++i)
        {
            moBuf[i] = (float) std::sin (moPhaseAcc * juce::MathConstants<double>::twoPi);
            moPhaseAcc += inc; moPhaseAcc -= std::floor (moPhaseAcc);
        }
    }
    const int moRingDst = (int) rd (p.moRingDst);

    // ---- each engine is a channel: render alone, run its 2 module slots, then mix in ----
    float keepL[32], keepR[32];
    auto beginChannel = [&]
    {
        std::copy (L, L + n, keepL); std::copy (R, R + n, keepR);
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    };
    auto endChannel = [&] (int e, float channelPitch)
    {
        const float noteHz = hzFor (channelPitch);
        for (int sl = 0; sl < slotsPerEngine; ++sl)
        {
            const int type = (int) rd (p.slotType[e][sl]);
            if (type != 0) slots[e][sl].process (type, rd (p.slotA[e][sl]), rd (p.slotB[e][sl]), rd (p.slotMix[e][sl]), noteHz, L, R, n);
        }
        if (moRing > 0.0001f && (moRingDst == 0 || moRingDst == e + 1))
            for (int i = 0; i < n; ++i) { const float m = 1.0f - moRing + moRing * moBuf[i]; L[i] *= m; R[i] *= m; }
        for (int i = 0; i < n; ++i) { L[i] += keepL[i]; R[i] += keepR[i]; }
    };

    // ===================== WAVETABLE ENGINE =====================
    if (rd (p.wtOn) > 0.5f && shared.table != nullptr)
    {
        beginChannel();
        const auto& table = *shared.table;
        const float pitch = basePitch + rd (p.wtOct) * 12.0f + rd (p.wtSemi) + rd (p.wtFine) * 0.01f + mod[dstWtPitch] * 24.0f;
        const float baseInc = hzFor (pitch) / sr;
        const float pos = juce::jlimit (0.0f, 1.0f, rd (p.wtPos) + mod[dstWtPos]);
        const int warpMode = (int) rd (p.wtWarp);
        const float warpAmt = juce::jlimit (0.0f, 1.0f, rd (p.wtWarpAmt) + mod[dstWtWarp]);
        const int uni = juce::jlimit (1, maxUnison, (int) rd (p.wtUnison));
        const float detune = juce::jlimit (0.0f, 1.0f, rd (p.wtDetune) + mod[dstWtDetune]);
        const float spread = rd (p.wtSpread);
        const float level = juce::jlimit (0.0f, 1.0f, rd (p.wtLevel) + mod[dstWtLevel]);
        const float pan = juce::jlimit (-1.0f, 1.0f, rd (p.wtPan));
        const float uniGain = level / std::sqrt ((float) uni);
        displayWtPos = pos;

        for (int u = 0; u < uni; ++u)
        {
            const float spreadPos = uni > 1 ? ((float) u / (float) (uni - 1)) * 2.0f - 1.0f : 0.0f;   // -1..1
            const float cents = spreadPos * detune * 50.0f;
            const float inc = baseInc * std::exp2 (cents / 1200.0f);
            const float effInc = (warpMode == 1 || warpMode == 6) ? inc * (1.0f + warpAmt * 7.0f) : inc;
            const int level0 = WavetableBank::levelForIncrement (effInc);
            const float pp = juce::jlimit (-1.0f, 1.0f, pan + spreadPos * spread);
            const float gL = std::cos ((pp + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * uniGain;
            const float gR = std::sin ((pp + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * uniGain;
            float ph = wtPhase[u];
            if (warpMode == 6 && warpAmt > 0.0001f)          // Formant: synced read under a smooth window
            {
                for (int i = 0; i < n; ++i)
                {
                    const float w = (1.0f - warpAmt) + warpAmt * 4.0f * ph * (1.0f - ph) * 1.5f;
                    const float s = table.read (pos, warpPhase (ph, 6, warpAmt), level0) * w;
                    L[i] += s * gL; R[i] += s * gR;
                    ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                }
            }
            else if (warpMode == 8 && warpAmt > 0.0001f)     // Fold: triangle wavefolder
            {
                const float drive = 1.0f + warpAmt * 4.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float x = table.read (pos, ph, level0) * drive + 1.0f;
                    const float m = x - 4.0f * std::floor (x * 0.25f);   // 0..4
                    const float s = 1.0f - std::abs (m - 2.0f);
                    L[i] += s * gL; R[i] += s * gR;
                    ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                }
            }
            else if (moPhaseAmt > 0.0001f || moPosAmt > 0.0001f)   // audio-rate phase / position from the mod osc
            {
                for (int i = 0; i < n; ++i)
                {
                    float pp2 = ph + moBuf[i] * moPhaseAmt * 0.5f;
                    pp2 -= std::floor (pp2);
                    const float s = table.read (juce::jlimit (0.0f, 1.0f, pos + moBuf[i] * moPosAmt * 0.5f), warpPhase (pp2, warpMode, warpAmt), level0);
                    L[i] += s * gL; R[i] += s * gR;
                    ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                }
            }
            else
            {
                for (int i = 0; i < n; ++i)
                {
                    const float s = table.read (pos, warpPhase (ph, warpMode, warpAmt), level0);
                    L[i] += s * gL; R[i] += s * gR;
                    ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                }
            }
            wtPhase[u] = ph;
        }
        endChannel (0, pitch);
    }
    else displayWtPos = -1.0f;

    // ===================== ANALOG ENGINE =====================
    if (rd (p.anOn) > 0.5f)
    {
        beginChannel();
        const float pitch = basePitch + rd (p.anOct) * 12.0f + rd (p.anSemi) + rd (p.anFine) * 0.01f
                          + mod[dstAnPitch] * 24.0f + driftValue * rd (p.anDrift) * 0.12f;
        const float inc = hzFor (pitch) / sr;
        const float shape = juce::jlimit (0.0f, 2.0f, rd (p.anShape) + mod[dstAnShape] * 2.0f);
        const float pw = juce::jlimit (0.03f, 0.97f, rd (p.anPw) + mod[dstAnPw] * 0.5f);
        const float sub = rd (p.anSub);
        const float noise = rd (p.anNoise);
        const float tone = rd (p.anTone);
        const float level = juce::jlimit (0.0f, 1.0f, rd (p.anLevel) + mod[dstAnLevel]) * 0.7f;
        const float pan = rd (p.anPan);
        const float gL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * level;
        const float gR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * level;
        for (int i = 0; i < n; ++i)
        {
            float subS = 0.0f;
            float s = analog.process (inc, shape, pw, subS);
            s = s * tone + subS * sub + (rng.nextFloat() * 2.0f - 1.0f) * noise * 0.7f;
            L[i] += s * gL; R[i] += s * gR;
        }
        endChannel (1, pitch);
    }

    // ===================== GRANULAR ENGINE =====================
    if (rd (p.grOn) > 0.5f && shared.grains != nullptr)
    {
        const auto& gs = *shared.grains;
        beginChannel();
        scanOffset += rd (p.grScan) * secs * 0.1f;
        scanOffset -= std::floor (scanOffset);
        GrainCloud::Settings s;
        float position = rd (p.grPos) + mod[dstGrPos] + scanOffset;
        s.position = position - std::floor (position);
        s.sizeMs = juce::jlimit (5.0f, 1000.0f, rd (p.grSize) * std::exp2 (mod[dstGrSize] * 3.0f));
        s.density = juce::jlimit (0.5f, 200.0f, rd (p.grDensity) * std::exp2 (mod[dstGrDensity] * 3.0f));
        s.spray = juce::jlimit (0.0f, 1.0f, rd (p.grSpray) + mod[dstGrSpray]);
        s.pitchRandSemis = rd (p.grPitchRand);
        s.stereo = rd (p.grStereo);
        s.reverseChance = rd (p.grReverse);
        const float semis = basePitch - gs.rootMidiNote + rd (p.grOct) * 12.0f + rd (p.grSemi) + mod[dstGrPitch] * 24.0f;
        s.rate = (double) semisToRatio (semis) * gs.sampleRate / sampleRate;
        displayGrPos = s.position;

        float gL[32] {}, gR[32] {};
        cloud.render (gs, s, gL, gR, n, rng);
        const float level = juce::jlimit (0.0f, 1.0f, rd (p.grLevel) + mod[dstGrLevel]);
        for (int i = 0; i < n; ++i) { L[i] += gL[i] * level; R[i] += gR[i] * level; }
        endChannel (2, basePitch + rd (p.grOct) * 12.0f + rd (p.grSemi));
    }
    else displayGrPos = -1.0f;

    // ===================== RESONATOR ENGINE =====================
    if (rd (p.resOn) > 0.5f)
    {
        beginChannel();
        Resonator::Settings rs;
        rs.type = (int) rd (p.resType);
        rs.decay = juce::jlimit (0.02f, 30.0f, rd (p.resDecay) * std::exp2 (mod[dstResDecay] * 3.0f));
        rs.bright = juce::jlimit (0.0f, 1.0f, rd (p.resBright) + mod[dstResBright]);
        rs.damp = rd (p.resDamp);
        rs.strike = rd (p.resStrike);
        rs.bow = juce::jlimit (0.0f, 1.0f, rd (p.resBow) + mod[dstResBow]);
        rs.buzz = juce::jlimit (0.0f, 1.0f, rd (p.resBuzz) + mod[dstResBuzz]);
        rs.retrigHz = rd (p.resRetrig);
        const float pitch = basePitch + rd (p.resOct) * 12.0f + rd (p.resSemi) + mod[dstResPitch] * 24.0f;
        float tmp[32] {};
        resonator.process (rs, hzFor (pitch), tmp, n, rng);
        const float level = juce::jlimit (0.0f, 1.0f, rd (p.resLevel) + mod[dstResLevel]);
        const float pan = rd (p.resPan);
        const float gL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * level;
        const float gR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * level;
        for (int i = 0; i < n; ++i) { L[i] += tmp[i] * gL; R[i] += tmp[i] * gR; }
        endChannel (3, pitch);
    }

    // ===================== FM ENGINE =====================
    if (rd (p.fmOn) > 0.5f)
    {
        beginChannel();
        auto fs = fmSettings();
        fs.index = juce::jlimit (0.0f, 3.0f, fs.index * std::exp2 (mod[dstFmIndex] * 2.0f));
        const float pitch = basePitch + rd (p.fmOct) * 12.0f + rd (p.fmSemi) + mod[dstFmPitch] * 24.0f;
        float tmp[32] {};
        fm.process (fs, hzFor (pitch), tmp, n);
        const float level = juce::jlimit (0.0f, 1.0f, rd (p.fmLevel) + mod[dstFmLevel]) * 0.6f;
        const float pan = rd (p.fmPan);
        const float gL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * level;
        const float gR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f * level;
        for (int i = 0; i < n; ++i) { L[i] += tmp[i] * gL; R[i] += tmp[i] * gR; }
        endChannel (4, pitch);
    }

    // ===================== NOISE =====================
    const float noiseLvl = juce::jlimit (0.0f, 1.0f, rd (p.noiseLevel) + mod[dstNoiseLevel]);
    if (noiseLvl > 0.0001f)
    {
        const int colour = (int) rd (p.noiseColor);
        for (int i = 0; i < n; ++i) { const float s = noise.next (colour, rng) * noiseLvl * 0.5f; L[i] += s; R[i] += s; }
    }

    // ===================== FILTERS (dual, series / parallel, with feedback) =====================
    const float e2Start = env2.getValue();
    const bool f1 = rd (p.fltOn) > 0.5f, f2 = rd (p.flt2On) > 0.5f;
    const float fb = juce::jlimit (0.0f, 1.0f, rd (p.fbAmt) + mod[dstFeedback]);
    if (f1 || f2 || fb > 0.0001f)
    {
        const float cut1 = rd (p.fltCutoff) + rd (p.fltEnv) * e2Start + mod[dstCutoff] + rd (p.fltKey) * (targetNote - 60.0f) / 12.0f * 0.1003f;
        const float res1 = juce::jlimit (0.0f, 1.0f, rd (p.fltRes) + mod[dstResonance]);
        const float drv1 = juce::jlimit (0.0f, 1.0f, rd (p.fltDrive) + mod[dstFltDrive]);
        const int type1 = (int) rd (p.fltType);
        const float cut2 = rd (p.flt2Cutoff) + rd (p.flt2Env) * e2Start + mod[dstCutoff2];
        const float res2 = juce::jlimit (0.0f, 1.0f, rd (p.flt2Res) + mod[dstRes2]);
        const int type2 = (int) rd (p.flt2Type);
        const bool parallel = rd (p.fltRoute) > 0.5f;
        const float bal = rd (p.fltBalance);
        const bool ffm = moFfm > 0.0001f;
        if (f1 && ! ffm) filter.setParams (type1, cutoffToHz (cut1), res1, drv1);
        if (f2) filter2.setParams (type2, cutoffToHz (cut2), res2, rd (p.flt2Drive));
        const float fbGain = fb * 1.4f;
        for (int i = 0; i < n; ++i)
        {
            if (f1 && ffm) filter.setParams (type1, cutoffToHz (cut1 + moBuf[i] * moFfm * 0.35f), res1, drv1);
            for (int ch = 0; ch < 2; ++ch)
            {
                float* io = ch == 0 ? L : R;
                const float x = io[i] + std::tanh (fbState[ch] * fbGain) * 0.7f;
                float y;
                if (parallel)
                {
                    const float a = f1 ? filter.process (x, ch) : x;
                    const float b = f2 ? filter2.process (x, ch) : x;
                    y = (f1 && f2) ? a * (1.0f - bal) + b * bal : (f1 ? a : b);
                }
                else
                {
                    y = f1 ? filter.process (x, ch) : x;
                    if (f2) y = filter2.process (y, ch);
                }
                fbState[ch] = y;
                io[i] = y;
            }
        }
    }

    // ===================== AMP =====================
    const float velSens = rd (p.ampVel);
    const float velGain = 1.0f - velSens * (1.0f - velocity);
    const float ampMod = juce::jlimit (0.0f, 2.0f, 1.0f + mod[dstAmp]);
    const float pan = juce::jlimit (-1.0f, 1.0f, mod[dstPan]);
    const float pL = pan > 0.0f ? 1.0f - pan : 1.0f;
    const float pR = pan < 0.0f ? 1.0f + pan : 1.0f;
    const float g = velGain * ampMod * 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const float a = ampEnv.next();
        env2.next(); env3.next();
        outL[i] += L[i] * a * g * pL;
        outR[i] += R[i] * a * g * pR;
    }
}

} // namespace ff
