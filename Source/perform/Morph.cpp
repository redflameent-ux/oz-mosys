#include "Morph.h"

namespace ff::perf
{
MorphEngine::MorphEngine (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& s) : proc (p), apvts (s)
{
    pOn = apvts.getRawParameterValue ("morph_on");
    pX = apvts.getRawParameterValue ("morph_x");
    pY = apvts.getRawParameterValue ("morph_y");
    pMode = apvts.getRawParameterValue ("orb_mode");
    pSpeed = apvts.getRawParameterValue ("orb_speed");
    pDepth = apvts.getRawParameterValue ("orb_depth");
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

bool MorphEngine::morphable (const juce::String& id)
{
    for (auto* skip : { "morph_", "orb_", "smp_", "ch_", "arp_", "mac", "master", "tune_a4", "scale", "voice_mode", "poly" })
        if (id.startsWith (skip)) return false;
    if (id.startsWith ("rk") && (id.endsWith ("_type") || id == "rk_split")) return false;   // never swap effects mid-morph
    return true;
}

MorphEngine::Values MorphEngine::captureNow() const
{
    Values v;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (morphable (rp->getParameterID()))
                v[rp->getParameterID()] = rp->getValue();
    return v;
}

void MorphEngine::setCorner (int i, const Values& v) { corners[(size_t) juce::jlimit (0, 3, i)] = v; dirty = true; }
void MorphEngine::clearCorners() { for (auto& c : corners) c.clear(); dirty = true; }

void MorphEngine::grab (float nx, float ny)
{
    held = true;
    setXY (nx, ny);
}

void MorphEngine::release (float nvx, float nvy)
{
    held = false;
    vx = juce::jlimit (-3.0f, 3.0f, nvx);
    vy = juce::jlimit (-3.0f, 3.0f, nvy);
    if (recording) recording = false;
}

void MorphEngine::setXY (float nx, float ny)
{
    x = juce::jlimit (0.0f, 1.0f, nx);
    y = juce::jlimit (0.0f, 1.0f, ny);
    for (auto [param, v] : { std::pair<const char*, float> { "morph_x", x }, { "morph_y", y } })
        if (auto* rp = apvts.getParameter (param); rp != nullptr && std::abs (rp->getValue() - v) > 1.0e-4f)
            rp->setValueNotifyingHost (v);
}

void MorphEngine::moveOrb (float dt)
{
    const int mode = (int) pMode->load();
    const float speed = pSpeed->load(), depth = pDepth->load();
    if (held || mode == 0 || pOn->load() < 0.5f)
    {
        // follow the parameters (host automation, knobs)
        x = pX->load(); y = pY->load();
        return;
    }
    const float rate = 0.05f + speed * speed * 2.5f;   // roughly "trips across the pad per second"
    float nx = x, ny = y;
    if (mode == 1)   // Dice: pick a spot, glide there, pick another
    {
        diceClock -= dt * rate;
        if (diceClock <= 0.0)
        {
            const float r = depth * 0.5f;
            target = { 0.5f + (rng.nextFloat() * 2.0f - 1.0f) * r, 0.5f + (rng.nextFloat() * 2.0f - 1.0f) * r };
            diceClock = 1.0;
        }
        const float k = 1.0f - std::exp (-dt * rate * 6.0f);
        nx += (target.x - nx) * k;
        ny += (target.y - ny) * k;
    }
    else if (mode == 2)   // Inertia: keeps the throw going, bounces off the walls
    {
        if (std::abs (vx) + std::abs (vy) < 0.02f) { const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi; vx = std::cos (a) * 0.4f; vy = std::sin (a) * 0.4f; }
        const float boost = 0.3f + speed * 2.0f;
        nx += vx * dt * boost;
        ny += vy * dt * boost;
        const float lo = 0.5f - depth * 0.5f, hi = 0.5f + depth * 0.5f;
        if (nx < lo) { nx = lo + (lo - nx); vx = std::abs (vx); }
        if (nx > hi) { nx = hi - (nx - hi); vx = -std::abs (vx); }
        if (ny < lo) { ny = lo + (lo - ny); vy = std::abs (vy); }
        if (ny > hi) { ny = hi - (ny - hi); vy = -std::abs (vy); }
    }
    else if (mode == 3)   // Attractor: a slow wobbling orbit round the middle
    {
        phase += dt * rate * juce::MathConstants<double>::twoPi * 0.5;
        const double r = depth * 0.5 * (0.75 + 0.25 * std::sin (phase * 0.37));
        nx = (float) (0.5 + r * std::cos (phase));
        ny = (float) (0.5 + r * std::sin (phase * 1.0 + 0.3 * std::sin (phase * 0.21)));
    }
    else if (mode == 4)   // Recorded path, looped
    {
        if (path.size() < 2) return;
        pathPos += dt * 30.0 * (0.25 + speed * 2.0);   // recorded at 30 points a second
        while (pathPos >= (double) path.size()) pathPos -= (double) path.size();
        const auto i = (size_t) pathPos;
        const auto a = path[i], b = path[(i + 1) % path.size()];
        const float f = (float) (pathPos - (double) i);
        nx = a.x + (b.x - a.x) * f;
        ny = a.y + (b.y - a.y) * f;
    }
    setXY (nx, ny);
}

void MorphEngine::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.2, (now - lastTime) * 0.001);
    lastTime = now;
    if (recording && held) path.push_back ({ x, y });
    if (path.size() > 30 * 60) path.erase (path.begin());

    moveOrb (dt);
    if (pOn->load() > 0.5f && hasCorner (0) && (dirty || std::abs (x - lastAppliedX) > 1.0e-4f || std::abs (y - lastAppliedY) > 1.0e-4f))
        apply();
    if (onMoved) onMoved();
}

void MorphEngine::apply()
{
    lastAppliedX = x; lastAppliedY = y; dirty = false;
    const float w[4] = { (1 - x) * y, x * y, (1 - x) * (1 - y), x * (1 - y) };   // A top-left, B top-right, C bottom-left, D bottom-right
    int nearest = 0;
    for (int i = 1; i < 4; ++i) if (w[i] > w[nearest]) nearest = i;
    const auto& a = corners[0];
    for (auto& [id, va] : a)
    {
        auto* p = apvts.getParameter (id);
        if (p == nullptr) continue;
        float vals[4];
        bool same = true;
        for (int i = 0; i < 4; ++i)
        {
            auto it = corners[(size_t) i].find (id);
            vals[i] = it != corners[(size_t) i].end() ? it->second : va;
            same = same && std::abs (vals[i] - va) < 1.0e-6f;
        }
        if (same) continue;
        float v;
        const bool smooth = dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr;
        if (smooth) v = vals[0] * w[0] + vals[1] * w[1] + vals[2] * w[2] + vals[3] * w[3];
        else v = vals[nearest];
        if (std::abs (p->getValue() - v) > 1.0e-5f) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    }
}

juce::String MorphEngine::toString (const Values& v)
{
    juce::String s;
    for (auto& [id, val] : v) s << id << '=' << juce::String (val, 5) << ';';
    return s;
}

MorphEngine::Values MorphEngine::fromString (const juce::String& s)
{
    Values v;
    for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
        if (tok.containsChar ('='))
            v[tok.upToFirstOccurrenceOf ("=", false, false)] = tok.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
    return v;
}

void MorphEngine::saveTo (juce::ValueTree& state) const
{
    state.removeChild (state.getChildWithName ("Morph"), nullptr);
    juce::ValueTree m ("Morph");
    for (int i = 0; i < 4; ++i) m.setProperty ("c" + juce::String (i), toString (corners[(size_t) i]), nullptr);
    juce::String p;
    for (auto& pt : path) p << juce::String (pt.x, 3) << ',' << juce::String (pt.y, 3) << ' ';
    m.setProperty ("path", p, nullptr);
    state.appendChild (m, nullptr);
}

void MorphEngine::restoreFrom (const juce::ValueTree& state)
{
    clearCorners();
    path.clear();
    auto m = state.getChildWithName ("Morph");
    if (! m.isValid()) return;
    for (int i = 0; i < 4; ++i) corners[(size_t) i] = fromString (m.getProperty ("c" + juce::String (i)).toString());
    for (auto& tok : juce::StringArray::fromTokens (m.getProperty ("path").toString(), " ", ""))
        path.push_back ({ tok.upToFirstOccurrenceOf (",", false, false).getFloatValue(), tok.fromFirstOccurrenceOf (",", false, false).getFloatValue() });
    lastAppliedX = lastAppliedY = -1.0f;   // don't jump the sound on load: the saved values already match
    lastAppliedX = pX->load(); lastAppliedY = pY->load(); x = lastAppliedX; y = lastAppliedY;
    dirty = false;
}
} // namespace ff::perf
