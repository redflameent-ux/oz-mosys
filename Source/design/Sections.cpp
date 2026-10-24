#include "Sections.h"
#include "../PluginProcessor.h"

namespace ff::design
{
const juce::StringArray& sectionNames()
{
    static const juce::StringArray names { "Wavetable", "Analog", "Granular", "Resonator", "Filter", "Amp Envelope",
                                           "Envelope 2", "Envelope 3", "LFOs", "MSEGs", "Mod Matrix", "Macros",
                                           "Effects", "Voice & Tuning", "FM" };
    return names;
}

bool paramInSection (const juce::String& id, int section)
{
    auto starts = [&id] (const char* p) { return id.startsWith (p); };
    switch (section)
    {
        case secWavetable: return starts ("wt_");
        case secAnalog:    return starts ("an_");
        case secGranular:  return starts ("gr_");
        case secResonator: return starts ("res_");
        case secFilter:    return starts ("flt_");
        case secAmpEnv:    return starts ("amp_");
        case secEnv2:      return starts ("e2_");
        case secEnv3:      return starts ("e3_");
        case secLfos:      return starts ("lfo1_") || starts ("lfo2_");
        case secMsegs:     return false;   // shapes, not parameters
        case secMatrix:    return id.startsWith ("mod") && id.length() > 3 && juce::CharacterFunctions::isDigit (id[3]);
        case secMacros:    return starts ("macro");
        case secEffects:   return starts ("fx_drive") || starts ("drive_type") || starts ("crush_") || starts ("tape_")
                                  || starts ("ring_") || starts ("cho_") || starts ("dly_") || starts ("rev_") || starts ("eq_") || starts ("rk");
        case secVoice:     return id.startsWith ("ch_") || id.startsWith ("arp_") || id == "glide" || id == "bend" || id == "voice_mode" || id == "tune_a4"
                                  || id == "scale" || id == "scale_root" || id == "master";
        case secFm:        return id.startsWith ("fm");
        default:           return false;
    }
}

int sectionOfParam (const juce::String& id)
{
    for (int s = 0; s < numSections; ++s) if (paramInSection (id, s)) return s;
    return -1;
}

std::vector<juce::RangedAudioParameter*> sectionParams (juce::AudioProcessorValueTreeState& state, int section)
{
    std::vector<juce::RangedAudioParameter*> out;
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (paramInSection (rp->getParameterID(), section)) out.push_back (rp);
    return out;
}

void setNormalised (juce::RangedAudioParameter* p, float v)
{
    p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    p->endChangeGesture();
}

juce::ValueTree captureSection (FourfoldProcessor& proc, int section)
{
    juce::ValueTree t ("OZSection");
    t.setProperty ("section", sectionNames()[section], nullptr);
    for (auto* p : sectionParams (proc.apvts, section))
    {
        juce::ValueTree pv ("P");
        pv.setProperty ("id", p->getParameterID(), nullptr);
        pv.setProperty ("v", p->convertFrom0to1 (p->getValue()), nullptr);
        t.appendChild (pv, nullptr);
    }
    if (section == secMsegs) t.appendChild (proc.msegsToTree(), nullptr);
    return t;
}

void applySection (FourfoldProcessor& proc, const juce::ValueTree& t, int onlySection)
{
    for (auto pv : t)
    {
        if (pv.hasType ("Msegs"))
        {
            if (onlySection < 0 || onlySection == secMsegs) proc.msegsFromTree (pv);
            continue;
        }
        const juce::String id = pv["id"].toString();
        if (onlySection >= 0 && ! paramInSection (id, onlySection)) continue;
        if (auto* p = proc.apvts.getParameter (id))
            setNormalised (p, p->convertTo0to1 ((float) pv["v"]));
    }
}

juce::File subpresetFolder (int section)
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("OZ-mosys")
                .getChildFile ("Subpresets").getChildFile (sectionNames()[section]);
    f.createDirectory();
    return f;
}

bool saveSection (FourfoldProcessor& proc, int section, const juce::File& file)
{
    if (auto xml = captureSection (proc, section).createXml())
        return xml->writeTo (file);
    return false;
}

bool loadSection (FourfoldProcessor& proc, int section, const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        applySection (proc, juce::ValueTree::fromXml (*xml), section);
        return true;
    }
    return false;
}

void initSection (FourfoldProcessor& proc, int section)
{
    for (auto* p : sectionParams (proc.apvts, section)) setNormalised (p, p->getDefaultValue());
    if (section == secMsegs)
        for (int i = 0; i < FourfoldProcessor::numMsegs; ++i) proc.setMseg (i, FourfoldProcessor::defaultMseg (i));
}

juce::ValueTree& sectionClipboard()
{
    static juce::ValueTree clip;
    return clip;
}
} // namespace ff::design
