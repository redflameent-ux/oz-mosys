# Reading undocumented binary presets

Formats like DUNE 3 (`.fxp`), Serum, Nexus and Spire store their parameters as opaque binary
blobs with no names - nothing an importer can read directly. `tools/PluginProbe.cpp` is where we
work out how to read them anyway. It is an opt-in, local-only console tool
(`cmake -DFOURFOLD_BUILD_TOOLS=ON`) and is never built in CI.

Two approaches live in it. The first is the one that sounded obvious; the second is the one that
actually worked for DUNE.

## 1. The plugin-oracle (host the plugin) - did NOT work for DUNE

The idea: every VST3/AU plugin exposes its parameters to a host *by name*, so host the real plugin
headlessly, load a preset into it with `setStateInformation`, and read `getParameters()` back as a
labelled dump.

It does not work for DUNE 3. DUNE's VST3, hosted through JUCE, **ignores a VST2 `.fxp` fed to
`setStateInformation`** - it is a silent no-op, with the inner chunk or the whole file alike. It
also ignores host parameter *writes* (`setValueNotifyingHost` leaves `getStateInformation`
byte-identical), and its own state is an opaque base64 blob. Proof: dumping two very different
presets (a bass and a pad) produced byte-identical parameter output except the preset name (which
PluginProbe takes from the filename). So the dump is just DUNE's default state, the same for every
preset.

The JSON dump mode is kept only as an inspection aid for plugins whose VST3 *does* accept their
old chunk. **Do not trust it to reflect a DUNE preset.**

```
PluginProbe <plugin.vst3|.dll> <preset.fxp | folder>      # JSON dump (see caveat)
PluginProbe <plugin.vst3|.dll> --state <outfile> [<i> <v>] # dump plugin state, for RE
```

## 2. Direct `.fxp` parsing (works for DUNE, needs no plugin)

A DUNE `.fxp` is a VST2 `FPCh` wrapper (fxID `DNE3`) around a small header and a **zlib** stream.
Inflated, it is a tag-based binary (reversed FourCCs: `info`, `sess`, `prog`, `pres`, `name`,
`para`...). The `para` block holds the parameters:

```
'arap' | int32 (8) | int32 count | int32 baseId | count * { uint32 id ; float value }
```

where each record's id low-16 bits is DUNE's internal parameter id and the high-16 bits is a
voice / sub index (we take voice 0). The values are the preset's real settings, normalised 0..1
(except a few counts such as "Voices Amount").

The id -> name map was recovered from a parameter-name table inside `DUNE3.vst3` itself: 44-byte
entries of `[int32 id][int32 flag][char name[36]]`, laid out in the host's parameter-enumeration
order. `kDuneParams` in `PluginProbe.cpp` lists the ids for the ~40 parameters the OZ mapping uses,
each validated to read sane values across several banks.

```
PluginProbe --fxp2oz "<PackName>" <preset.fxp | folder>
```

writes `.ozpreset` cousins into `Documents/OZ-mosys/Expansions/<PackName>`, mirroring the source
sub-folders. The `arap` block is located by searching for its tag (its offset moves in
wavetable-heavy presets), and only files with fxID `DNE3` are read (DUNE 2, fxID `DUE2`, is a
different format and is skipped).

### What is carried across (and what is not)

`convertDune()` builds a close cousin, not a clone:

- **Faithful:** filter cutoff / resonance / drive / env amount / key-track, amp + filter
  envelopes, oscillator pan, reverb / delay / chorus / distortion on-off and sends, arp on-off.
- **Approximate:** oscillator level is coarse (DUNE's osc routing is complex, so we take the
  louder of mixer / amount and never import a silent preset); tuning is proportional, and
  "Osc 1 Semi" is skipped because its id collides and reads garbage.
- **Not mapped:** the mod matrix, LFO routings, per-voice detail, and the ~1500 parameters beyond
  the ~40 core ones.

## Other synths

The same direct-parse approach would need redoing per synth: each has its own container, its own
id space and its own name table (and some, like Serum, may actually accept the oracle). Only DUNE 3
is implemented here.
