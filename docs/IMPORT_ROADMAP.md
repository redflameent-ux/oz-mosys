# Preset import roadmap (gap analysis)

What it would take to raise import fidelity from the source synths, ranked by how much
of a real library each change helps. This is a living document: the percentages come from
scanning an actual installed preset library, and should be re-measured as the importers change.

## Method

For each source synth:

1. **Read the format.** Decode the preset file and list every parameter / feature it can carry.
2. **Measure active usage.** Across a large random sample of real presets, count how often each
   feature is *actually in use* (active + non-default) — not merely present. Many synths write
   every module into every preset even when it is switched off, so "present" wildly overstates usage.
3. **Classify each gap.** Every feature a preset uses is one of:
   - **Covered** - the importer already maps it.
   - **Buildable** - OZ-mosys does not represent it, but a parameter / engine could be added, then mapped.
   - **Sample wall** - the sound depends on sample/audio data that lives only inside the source
     synth's library. No OZ parameter can recover it; the only routes are (a) substitute the closest
     OZ engine (what we do today) or (b) render the audio out of the source synth, which needs it installed.
4. **Rank** the buildable gaps by `usage% x fidelity gain`, and build biggest-win-first.

The two readable, labelled formats (Omnisphere XML, Arturia Boost archives) can be analysed directly.
The unlabelled binaries (FM8 `.nfm8`, native Absynth `.nabs`) have no parameter names on disk, so a true
usage count there needs the **plugin-oracle harness** (below) to name the parameters first.

---

## Omnisphere

Readable XML, so the synth engine comes across without Omnisphere installed. Measured over **500 random
`.prt_omn` patches** from the installed library.

| Feature | Active usage | Status | Note |
|---|---:|---|---|
| Arpeggiator (+ step seq) | 21.4% | **Covered** | mode, rate, octave, swing, gate, per-step pattern |
| Sampled soundsource (`OSC kind != 0`) | 20.4% | **Sample wall** | substituted by closest OZ engine from the soundsource name |
| Waveshaper (`WAVESHAPER act`) | 17.8% | **Buildable** | no OZ waveshaper stage yet |
| 3rd / 4th layer | 12.4% | **Buildable** | importer reads only 2 of up to 4 layers |
| Organ drawbars (`TONEWHEEL`) | 9.2% | **Buildable** | organ is substituted; drawbar mix dropped |
| Per-layer Dist | ~0% | Skip | effectively unused in this library |
| Harmonia (additive `HARM`) | not yet measured | **Buildable?** | needs an `OSC kind == Harmonia` correlation to count honestly |

### Omnisphere backlog (build in this order)

1. **Waveshaper stage** (~18%). Add a waveshaper to the OZ voice (drive / shape / mix), then map
   `WAVESHAPER act/type/dpth/gain/mix`. Single highest-value Omnisphere gap.
2. **3rd / 4th layer** (~12%). OZ has five engines; the importer caps Omnisphere at two layers.
   Map layers C/D onto the spare engines when a patch uses them.
3. **Tonewheel / drawbar organ** (~9%). A drawbar oscillator mode, mapped from `TONEWHEEL drw1..drw9`,
   would make organ patches faithful instead of a wavetable approximation.
4. **Harmonia (additive)** - measure first, then an additive/harmonic oscillator mode if usage warrants.

The **sample wall (~20%)** is not on this list on purpose: no parameter closes it. Options are the
current name-match substitution, or a sample-playback engine fed by audio the user captures/renders
from Omnisphere (which defeats the "no install" goal).

---

## Arturia (Pigments / Analog Lab / V Collection)

Boost text archives (`.pgtx`, `.labx` - a ZIP of per-preset archives, ~315 presets per bank). Fully
labelled, so real parameters are readable without the plugin.

- **Pigments** - **mostly covered.** The importer already reads oscillator, filter cutoff/resonance,
  the three envelopes, LFO rate and the FX dry/wet. Buildable extension: more oscillator detail and the
  full mod matrix.
- **Analog Lab / V Collection** - **partly covered.** Each underlying engine (Jup-8V, DX7, CS-80...) names
  its own parameters, but Analog Lab also writes a **unified macro layer** that is the same across engines:
  `Slot1_Macro1/2`, `Slot1_Cutoff`, `Slot1_Reverb/Delay/Chorus`, `Slot1_ArpSeq_*`, `Slot1_MasterVolume`,
  `Slot1_Pan`. Today only a generic cutoff/res guess is mapped.
  - **Buildable (high value, low risk):** map the unified macro layer in full - the two macros, cutoff,
    the three FX sends and the arp. One mapping covers every Analog Lab preset regardless of engine.
  - **Buildable (large, optional):** per-engine parameter maps for the most common V Collection instruments,
    for faithful oscillator/filter detail beyond the macro layer.

---

## Native Instruments (FM8, Absynth)

- **Absynth `.ksd` (SoundShell)** - metadata **covered** (real name, author, bank, Type/Timbre/Genre tags
  via the zlib-wrapped XML). Synth parameters themselves are stored unlabelled.
- **FM8 `.nfm8` / native Absynth `.nabs`** - open container (decoded), but **0 readable parameter labels**
  on disk: values are positional with no names. A usage count or a faithful map is not possible from the
  files alone.
  - The only reliable route is the **plugin-oracle harness** below. Until then these stay name/type rebuilds.

---

## Cross-cutting: the plugin-oracle harness

The general tool for any plugin, and the only path for the unlabelled NI formats (and for the
Omnisphere sample wall, if ever wanted):

A small **headless VST3/AU host** (a JUCE console app, built by the existing CI target pattern, run on a
machine that has the synths installed and licensed) that:

1. loads the source plugin,
2. enumerates every parameter **by name** through the plugin API,
3. for the unlabelled binaries, sets one parameter at a time and saves, so a **diff mapper** can recover
   `file offset -> parameter name` permanently for that synth + version,
4. optionally renders a preset to audio for fingerprint-based matching of sampled sounds.

This needs the plugins present at run time (so it runs on the user's machine, not CI), but it unlocks real
parameters for FM8/native-Absynth and raises Pigments/Analog Lab/Omnisphere to whatever the plugin exposes.

---

## Priority summary (biggest wins first)

| # | Build | Synth | Helps | Risk |
|--:|---|---|---|---|
| 1 | Map the Analog Lab unified macro layer | Arturia | every Analog Lab preset | low |
| 2 | Waveshaper stage + mapping | Omnisphere | ~18% of patches | low |
| 3 | 3rd / 4th layer import | Omnisphere | ~12% of patches | medium |
| 4 | Tonewheel / drawbar organ mode | Omnisphere | ~9% of patches | medium |
| 5 | Plugin-oracle harness | FM8 / Absynth (all) | unlabelled formats | high (needs installed plugins) |
| 6 | Measure + maybe add Harmonia additive | Omnisphere | TBD | medium |

Items 1-4 need no new runtime and no installed source synths. Item 5 is the big structural piece that
unlocks the formats we currently can only name-match.
