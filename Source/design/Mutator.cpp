#include "Mutator.h"
#include "../PluginProcessor.h"
#include "../Presets.h"

namespace ff::design
{
Mutator::Mutator (FourfoldProcessor& p) : proc (p)
{
    locked[secVoice] = true;     // tuning / mono / glide stay put unless you unlock them
    locked[secMacros] = true;
}

Mutator::Snapshot Mutator::capture() const
{
    Snapshot s;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! rp->getParameterID().startsWith ("smp_"))
                s.values[rp->getParameterID()] = rp->getValue();
    for (int i = 0; i < FourfoldProcessor::numMsegs; ++i) s.msegs.add (proc.getMseg (i)->toString());
    s.label = proc.currentPresetName;
    return s;
}

void Mutator::restore (const Snapshot& s)
{
    for (auto& [id, v] : s.values)
        if (auto* p = proc.apvts.getParameter (id))
            if (std::abs (p->getValue() - v) > 1.0e-6f) setNormalised (p, v);
    for (int i = 0; i < juce::jmin (FourfoldProcessor::numMsegs, s.msegs.size()); ++i)
    {
        auto m = MsegShape::fromString (s.msegs[i]);
        m.name = proc.getMseg (i)->name;
        proc.setMseg (i, m);
    }
    proc.currentPresetName = s.label;
}

void Mutator::push (const Snapshot& s)
{
    if (index + 1 < (int) history.size()) history.erase (history.begin() + index + 1, history.end());
    history.push_back (s);
    if (history.size() > 64) history.erase (history.begin());
    index = (int) history.size() - 1;
}

void Mutator::back()
{
    if (! canGoBack()) return;
    restore (history[(size_t) --index]);
}

void Mutator::forward()
{
    if (! canGoForward()) return;
    restore (history[(size_t) ++index]);
}

Mutator::Snapshot Mutator::targetFor (int presetIndex)
{
    const auto original = capture();
    const auto name = proc.currentPresetName;
    const int prog = proc.getCurrentProgram();
    proc.setCurrentProgram (juce::jlimit (0, proc.getNumPrograms() - 1, presetIndex));
    auto target = capture();
    restore (original);
    proc.currentPresetName = name;
    proc.setCurrentProgramIndexOnly (prog);
    return target;
}

static bool isToggle (juce::RangedAudioParameter* p)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) return c->choices.size() == 2 && c->choices[0] == "Off";
    return false;
}

void Mutator::randomise (Snapshot& s, float amountIn)
{
    const float aBase = juce::jlimit (0.0f, 1.0f, amountIn);
    auto gauss = [this] { return (float) (rng.nextDouble() + rng.nextDouble() + rng.nextDouble() - 1.5) * 1.15f; };

    for (auto& [id, v] : s.values)
    {
        const int sec = sectionOfParam (id);
        if (sec < 0 || locked[(size_t) sec] || id == "master" || sec == secMatrix) continue;
        auto* p = proc.apvts.getParameter (id);
        if (p == nullptr) continue;
        const float a = aBase * sectionAmount[(size_t) sec];
        if (a <= 0.0001f) continue;

        if (isToggle (p))
        {
            if (rng.nextFloat() < a * 0.15f) v = v > 0.5f ? 0.0f : 1.0f;
        }
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
        {
            if (rng.nextFloat() < a * 0.3f) v = p->convertTo0to1 ((float) rng.nextInt (c->choices.size()));
        }
        else if (dynamic_cast<juce::AudioParameterInt*> (p) != nullptr)
        {
            if (rng.nextFloat() < a * 0.3f)
            {
                const float step = 1.0f / juce::jmax (1.0f, p->getNormalisableRange().end - p->getNormalisableRange().start);
                v = juce::jlimit (0.0f, 1.0f, v + step * (float) (rng.nextInt (5) - 2));
            }
        }
        else
            v = juce::jlimit (0.0f, 1.0f, v + gauss() * a * 0.35f);
    }

    // mod matrix: wiggle amounts, retarget some, add new routes
    float a = aBase * sectionAmount[secMatrix];
    if (! locked[secMatrix] && a > 0.0001f)
    {
        static const int sources[] = { srcEnv2, srcEnv3, srcLfo1, srcLfo2, srcVelocity, srcRandom, srcMseg1, srcMseg2, srcKeyTrack };
        for (int i = 0; i < numModSlots; ++i)
        {
            auto* ps = proc.apvts.getParameter (slotSrc (i));
            auto* pd = proc.apvts.getParameter (slotDst (i));
            auto* pa = proc.apvts.getParameter (slotAmt (i));
            float& sv = s.values[slotSrc (i)];
            float& dv = s.values[slotDst (i)];
            float& av = s.values[slotAmt (i)];
            const bool active = ps->convertFrom0to1 (sv) > 0.5f && pd->convertFrom0to1 (dv) > 0.5f;
            if (active)
            {
                av = juce::jlimit (0.0f, 1.0f, av + gauss() * a * 0.15f);
                if (rng.nextFloat() < a * 0.2f) dv = pd->convertTo0to1 ((float) (1 + rng.nextInt (numModDests - 1)));
            }
            else if (rng.nextFloat() < a * 0.25f)
            {
                sv = ps->convertTo0to1 ((float) sources[rng.nextInt ((int) std::size (sources))]);
                dv = pd->convertTo0to1 ((float) (1 + rng.nextInt (numModDests - 1)));
                const float amt = (0.1f + 0.4f * rng.nextFloat()) * (rng.nextBool() ? 1.0f : -1.0f) * juce::jmax (0.3f, a);
                av = pa->convertTo0to1 (amt);
            }
        }
    }

    // breakpoint envelopes: nudge levels, timing and curves
    a = aBase * sectionAmount[secMsegs];
    if (! locked[secMsegs] && a > 0.0001f)
        for (auto& str : s.msegs)
        {
            auto m = MsegShape::fromString (str);
            for (size_t i = 0; i < m.points.size(); ++i)
            {
                auto& pt = m.points[i];
                pt.level = juce::jlimit (0.0f, 1.0f, pt.level + gauss() * a * 0.3f);
                pt.curve = juce::jlimit (-1.0f, 1.0f, pt.curve + gauss() * a * 0.5f);
                if (i > 0) pt.time *= std::exp (gauss() * a * 0.35f);
            }
            if (rng.nextFloat() < a * 0.15f) m.loop = ! m.loop;
            if (m.stepper)
                for (int i = 0; i < m.numSteps; ++i)
                {
                    m.stepLevel[(size_t) i] = juce::jlimit (0.0f, 1.0f, m.stepLevel[(size_t) i] + gauss() * a * 0.4f);
                    if (rng.nextFloat() < a * 0.3f) m.stepShape[(size_t) i] = rng.nextInt (MsegShape::numStepShapes);
                }
            m.sanitise();
            str = m.toString();
        }
}

void Mutator::morph (Snapshot& s, const Snapshot& target, float amountIn)
{
    const float aBase = juce::jlimit (0.0f, 1.0f, amountIn);
    for (auto& [id, v] : s.values)
    {
        const int sec = sectionOfParam (id);
        if (sec < 0 || locked[(size_t) sec] || id == "master") continue;
        const float a = aBase * sectionAmount[(size_t) sec];
        auto it = target.values.find (id);
        if (it == target.values.end()) continue;
        auto* p = proc.apvts.getParameter (id);
        if (p == nullptr) continue;
        const bool stepped = dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr || dynamic_cast<juce::AudioParameterInt*> (p) != nullptr;
        if (stepped) { if (a >= 0.5f) v = it->second; }
        else v = v + (it->second - v) * a;
    }
    const float a = aBase * sectionAmount[secMsegs];
    if (! locked[secMsegs])
    {
        auto blend = [a] (const juce::String& from, const juce::String& to)
        {
            auto m0 = MsegShape::fromString (from), m1 = MsegShape::fromString (to);
            if (m0.points.size() != m1.points.size()) return a >= 0.5f ? to : from;
            for (size_t i = 0; i < m0.points.size(); ++i)
            {
                m0.points[i].time += (m1.points[i].time - m0.points[i].time) * a;
                m0.points[i].level += (m1.points[i].level - m0.points[i].level) * a;
                m0.points[i].curve += (m1.points[i].curve - m0.points[i].curve) * a;
            }
            if (a >= 0.5f) { m0.loop = m1.loop; m0.sustain = m1.sustain; m0.loopStart = m1.loopStart; m0.sync = m1.sync; }
            m0.sanitise();
            return m0.toString();
        };
        for (int i = 0; i < juce::jmin (s.msegs.size(), target.msegs.size()); ++i)
            s.msegs.set (i, blend (s.msegs[i], target.msegs[i]));
    }
}

void Mutator::keepPlayable (Snapshot& s)
{
    auto real = [this, &s] (const char* id) { auto* p = proc.apvts.getParameter (id); return p->convertFrom0to1 (s.values[id]); };
    auto setReal = [this, &s] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); s.values[id] = p->convertTo0to1 (v); };

    // at least one engine on, and audible
    const char* engines[] = { "wt_on", "an_on", "gr_on", "res_on", "fm_on" };
    const char* levels[] = { "wt_level", "an_level", "gr_level", "res_level", "fm_level" };
    bool any = false;
    for (int i = 0; i < 5; ++i)
        if (real (engines[i]) > 0.5f) { any = true; if (real (levels[i]) < 0.3f) setReal (levels[i], 0.3f + rng.nextFloat() * 0.3f); }
    if (! any) { setReal ("wt_on", 1.0f); setReal ("wt_level", 0.7f); }

    // don't let the filter or the envelope swallow the sound
    if (real ("flt_on") > 0.5f && real ("flt_env") < 0.1f && real ("flt_cutoff") < 0.3f) setReal ("flt_cutoff", 0.3f + rng.nextFloat() * 0.3f);
    if (real ("amp_s") < 0.05f && real ("amp_d") < 0.08f) setReal ("amp_d", 0.25f);
    if (real ("amp_a") > 4.0f) setReal ("amp_a", 2.0f + rng.nextFloat() * 2.0f);
    // keep feedback and wet effects in a sane range
    if (real ("dly_fb") > 0.8f) setReal ("dly_fb", 0.8f);
    if (real ("rev_mix") > 0.75f) setReal ("rev_mix", 0.75f);
    if (real ("flt_res") > 0.85f && real ("flt_drive") > 0.5f) setReal ("flt_res", 0.8f);
}

int Mutator::pickSimilarPreset()
{
    // a preset that sounds close to this one (nearby on the sound map), picked at random
    const auto& list = factoryPresets();
    const float x = proc.presetInfo.mapX, y = proc.presetInfo.mapY;
    std::vector<std::pair<float, int>> near;
    for (int i = 1; i < (int) list.size(); ++i)
    {
        if (list[(size_t) i].name == proc.currentPresetName.upToFirstOccurrenceOf (" M", false, false)) continue;
        const float d = std::hypot (list[(size_t) i].mapX - x, list[(size_t) i].mapY - y);
        near.push_back ({ d, i });
    }
    std::sort (near.begin(), near.end());
    const int pick = rng.nextInt (juce::jmin (8, (int) near.size()));
    return near.empty() ? 1 : near[(size_t) pick].second;
}

void Mutator::mutateFrom (const Snapshot& from)
{
    auto next = from;
    if (mutationAmount > 0.001f)
    {
        const int t = pickSimilarPreset();
        lastTargetName = proc.getProgramName (t);
        morph (next, targetFor (t), mutationAmount * 0.6f);
    }
    if (randomAmount > 0.001f) randomise (next, randomAmount);
    keepPlayable (next);
    ++mutationCount;
    next.label = from.label.upToFirstOccurrenceOf (" M", false, false) + " M " + juce::String (mutationCount);
    restore (next);
    push (next);
}

void Mutator::mutate()
{
    auto current = capture();
    if (history.empty() || index < 0) push (current);
    else history[(size_t) index].values = current.values, history[(size_t) index].msegs = current.msegs;   // keep hand edits
    base = history[(size_t) index];
    mutateFrom (base);
}

void Mutator::retry()
{
    if (history.empty() || base.values.empty()) { mutate(); return; }
    // replace the latest variation with a new one from the same starting point
    if (index > 0 && index == (int) history.size() - 1) { history.pop_back(); --index; }
    --mutationCount;
    mutateFrom (base);
}

void Mutator::randomOnly (float amountIn)
{
    auto current = capture();
    if (history.empty() || index < 0) push (current);
    auto next = current;
    randomise (next, amountIn);
    keepPlayable (next);
    ++mutationCount;
    next.label = current.label.upToFirstOccurrenceOf (" M", false, false) + " M " + juce::String (mutationCount);
    restore (next);
    push (next);
}

void Mutator::goTo (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) history.size())) return;
    index = i;
    restore (history[(size_t) i]);
}
} // namespace ff::design
