#pragma once
#include <juce_dsp/juce_dsp.h>

class FourfoldProcessor;

// =====================================================================================
//  Measures what a sound is like, for the BROWSE map:
//   brightness  - where the energy sits in the spectrum (dark ... bright)
//   aggression  - high-frequency bite, noisiness and a sharp attack (soft ... aggressive)
//   movement    - how much the tone and level change while a note is held
// =====================================================================================
namespace ff::design
{
struct SoundFeatures { float brightness = 0, aggression = 0, movement = 0; };

SoundFeatures analyse (const juce::AudioBuffer<float>& audio, double sampleRate);

// Plays a short chord through the processor's current settings and measures it.
// Runs offline on the calling thread (about a tenth of a second).
juce::AudioBuffer<float> renderAudition (FourfoldProcessor&, double sampleRate, double seconds = 3.0);
} // namespace ff::design
