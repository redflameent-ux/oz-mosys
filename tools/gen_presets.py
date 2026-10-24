#!/usr/bin/env python3
"""Generates Source/Presets.cpp from the preset specs below.

Values use the same units the knobs show (cutoff 0..1, times in seconds, etc.).
Routes are (source, destination, amount) using the ModSource / ModDest names.
'master' values are filled in by the level-matching pass (levels.json).
"""
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Tables: 0 Basic(0 sine .33 tri .66 saw 1 square) 1 Harmonic Sweep 2 Vocal 3 Digital 4 Pulse Width 5 Dark Organ
# Analog shape: 0 tri, 1 saw, 2 pulse. Filter: 0 Ladder24, 1 LP12, 2 BP, 3 HP, 4 Notch
# LFO shapes: 0 sine 1 tri 2 saw 3 square 4 S&H 5 smooth random. Warp: 0 off 1 sync 2 bend+ 3 bend- 4 mirror 5 asym

P = []
def p(cat, name, values, routes=()):
    P.append((cat, name, values, list(routes)))

# ============================== DARK / CINEMATIC ==============================
C = "Dark & Cinematic"
p(C, "Catacomb Choir", dict(wt_table=2, wt_pos=0.7, wt_unison=5, wt_detune=0.3, wt_level=0.55,
    gr_on=1, gr_pos=0.4, gr_size=260, gr_density=30, gr_spray=0.3, gr_stereo=1, gr_level=0.6,
    flt_type=1, flt_cutoff=0.6, amp_a=2.0, amp_s=1, amp_r=4.0, lfo1_rate=0.08,
    cho_mix=0.35, rev_mix=0.55, rev_size=0.95),
  [("Lfo1", "WtPos", 0.3)])
p(C, "Black Cathedral", dict(wt_table=5, wt_pos=0.4, wt_unison=3, wt_detune=0.12, flt_cutoff=0.55, flt_res=0.2,
    amp_a=0.8, amp_s=1, amp_r=3.0, lfo2_rate=5.0, rev_mix=0.6, rev_size=0.95, rev_damp=0.3),
  [("Lfo2", "PitchAll", 0.003)])
p(C, "Rust Drone", dict(wt_on=0, gr_on=1, gr_pos=0.15, gr_size=450, gr_density=60, gr_spray=0.5, gr_pitchrand=0.3,
    gr_reverse=0.5, gr_stereo=1, gr_oct=-1, gr_level=0.9, flt_cutoff=0.5, flt_res=0.35, flt_drive=0.4,
    amp_a=2.5, amp_s=1, amp_r=5, lfo1_shape=5, lfo1_rate=0.1, rev_mix=0.7, rev_size=0.9),
  [("Lfo1", "Cutoff", 0.2)])
p(C, "Ghost Signal", dict(wt_table=3, wt_pos=0.8, wt_warp=1, wt_warpamt=0.3, flt_type=2, flt_cutoff=0.6, flt_res=0.5,
    amp_a=0.3, amp_s=1, amp_r=1.5, lfo1_shape=4, lfo1_rate=6, dly_mix=0.4, dly_time=450, dly_fb=0.6, rev_mix=0.4),
  [("Lfo1", "WtWarp", 0.4)])
p(C, "Funeral Strings", dict(wt_table=0, wt_pos=0.66, wt_unison=7, wt_detune=0.3, wt_level=0.6,
    an_on=1, an_shape=1, an_drift=0.6, an_level=0.4, flt_cutoff=0.5, flt_res=0.1, flt_env=0.15,
    e2_a=1.5, e2_d=3, e2_s=0.5, amp_a=0.9, amp_s=1, amp_r=2.5, cho_mix=0.5, rev_mix=0.5, rev_size=0.85))
p(C, "Abyss Swell", dict(wt_table=0, wt_pos=0.0, wt_oct=-1, wt_level=0.6, an_on=1, an_shape=0, an_oct=-2, an_sub=0.8,
    an_level=0.6, flt_cutoff=0.35, amp_a=3, amp_s=1, amp_r=4, fx_drive=0.3, rev_mix=0.3))
p(C, "Hollow Bones", dict(wt_table=2, wt_pos=0.9, wt_warp=4, wt_warpamt=0.5, wt_level=0.5,
    gr_on=1, gr_reverse=0.8, gr_size=150, gr_density=25, gr_level=0.7, flt_type=3, flt_cutoff=0.35,
    amp_a=0.6, amp_s=1, amp_r=2.5, lfo1_rate=0.2, rev_mix=0.65, rev_size=0.9),
  [("Lfo1", "GrPos", 0.3)])
p(C, "Shadow Bell", dict(wt_table=1, wt_pos=0.7, flt_type=1, flt_cutoff=0.85, amp_a=0.001, amp_d=2.5, amp_s=0, amp_r=3,
    e2_a=0, e2_d=1.5, e2_s=0, dly_mix=0.3, dly_time=500, dly_fb=0.45, rev_mix=0.5),
  [("Env2", "WtPos", -0.4)])
p(C, "Lantern Fog", dict(wt_on=0, gr_on=1, gr_pos=0.5, gr_size=600, gr_density=80, gr_spray=0.8, gr_stereo=1, gr_scan=0.3,
    gr_level=0.9, flt_type=1, flt_cutoff=0.55, amp_a=4, amp_s=1, amp_r=6, rev_mix=0.8, rev_size=0.95))
p(C, "Iron Heartbeat", dict(wt_on=0, an_on=1, an_shape=2, an_pw=0.3, an_oct=-1, an_level=0.8, flt_cutoff=0.3, flt_res=0.5,
    flt_env=0.35, e2_a=0, e2_d=0.25, e2_s=0, amp_a=0.001, amp_d=0.5, amp_s=0, amp_r=0.4, dly_mix=0.25, dly_time=500, rev_mix=0.3))
p(C, "Dread Riser", dict(wt_table=3, wt_pos=0.1, wt_unison=4, wt_detune=0.3, amp_a=0.5, amp_s=1, amp_r=2,
    e3_a=6, e3_d=1, e3_s=1, e3_r=2, flt_cutoff=0.4, rev_mix=0.5),
  [("Env3", "PitchAll", 0.5), ("Env3", "WtPos", 0.8), ("Env3", "Cutoff", 0.4)])
p(C, "Cold Mirror", dict(wt_table=4, wt_pos=0.5, wt_unison=4, wt_detune=0.2, flt_type=1, flt_cutoff=0.65,
    amp_a=0.6, amp_s=1, amp_r=2, lfo1_shape=1, lfo1_rate=0.3, cho_mix=0.6, rev_mix=0.5),
  [("Lfo1", "WtPos", 0.4)])
p(C, "Night Procession", dict(wt_table=5, wt_pos=0.6, wt_oct=-1, wt_level=0.6, an_on=1, an_shape=1, an_level=0.5,
    flt_cutoff=0.4, flt_res=0.25, amp_a=1.2, amp_s=1, amp_r=2.5, lfo2_rate=0.15, fx_drive=0.2, rev_mix=0.55),
  [("Lfo2", "Cutoff", 0.15)])

# ================================ REGGAE & DUB ================================
C = "Reggae & Dub"
p(C, "Skank Organ", dict(wt_table=5, wt_pos=0.55, flt_type=1, flt_cutoff=0.7, amp_a=0.001, amp_d=0.18, amp_s=0, amp_r=0.12,
    amp_vel=0.6, cho_mix=0.3, rev_mix=0.15))
p(C, "Bubble Organ", dict(wt_table=5, wt_pos=0.35, flt_cutoff=0.6, amp_a=0.002, amp_d=0.25, amp_s=0.2, amp_r=0.1,
    lfo2_rate=6, cho_mix=0.4, rev_mix=0.15),
  [("Lfo2", "Amp", 0.15)])
p(C, "Rockers Bass", dict(wt_on=0, an_on=1, an_shape=0.2, an_oct=-1, an_sub=0.7, an_drift=0.2, an_level=0.9,
    flt_cutoff=0.28, flt_res=0.1, amp_a=0.002, amp_d=0.6, amp_s=0.7, amp_r=0.15, glide=0.03, fx_drive=0.15, rev_mix=0))
p(C, "Steppers Sub", dict(wt_table=0, wt_pos=0, wt_oct=-1, an_on=1, an_shape=0, an_oct=-1, an_level=0.4,
    flt_cutoff=0.25, amp_a=0.002, amp_s=1, amp_r=0.15, fx_drive=0.2, rev_mix=0))
p(C, "Dub Siren", dict(wt_on=0, an_on=1, an_shape=2, an_pw=0.5, an_level=0.7, flt_type=1, flt_cutoff=0.75,
    lfo1_shape=1, lfo1_rate=1.2, lfo2_rate=0.1, amp_s=1, amp_r=0.6,
    dly_mix=0.5, dly_fb=0.7, dly_time=375, rev_mix=0.35),
  [("Lfo1", "PitchAll", 0.3), ("Lfo2", "Lfo1Rate", 0.5)])
p(C, "Echo Melodica", dict(wt_table=2, wt_pos=0.35, wt_level=0.6, an_on=1, an_shape=2, an_pw=0.45, an_level=0.4,
    flt_type=1, flt_cutoff=0.65, flt_res=0.2, amp_a=0.04, amp_s=0.9, amp_r=0.3, lfo2_rate=5,
    dly_mix=0.45, dly_fb=0.55, dly_time=500, rev_mix=0.3),
  [("Lfo2", "PitchAll", 0.003)])
p(C, "Spring Stab", dict(wt_table=0, wt_pos=0.66, wt_unison=3, wt_detune=0.15, flt_type=2, flt_cutoff=0.62, flt_res=0.3,
    amp_a=0.001, amp_d=0.25, amp_s=0, amp_r=0.3, dly_mix=0.5, dly_fb=0.6, dly_tone=0.4, rev_mix=0.5))
p(C, "Version Pad", dict(wt_table=2, wt_pos=0.5, wt_level=0.5, gr_on=1, gr_level=0.6, flt_type=1, flt_cutoff=0.55,
    amp_a=0.5, amp_s=1, amp_r=2, lfo1_rate=0.2, dly_mix=0.3, dly_time=600, dly_fb=0.5, rev_mix=0.45),
  [("Lfo1", "Cutoff", 0.1)])
p(C, "Roots Clavinet", dict(wt_on=0, an_on=1, an_shape=2, an_pw=0.2, an_level=0.8, flt_cutoff=0.45, flt_res=0.3,
    flt_env=0.35, e2_d=0.2, e2_s=0, amp_a=0.001, amp_d=0.35, amp_s=0.1, amp_r=0.1, amp_vel=0.8, cho_mix=0.2, rev_mix=0.1))
p(C, "Melodica Lead", dict(wt_table=0, wt_pos=1.0, wt_level=0.3, an_on=1, an_shape=2, an_pw=0.4, an_level=0.7,
    flt_type=1, flt_cutoff=0.7, amp_a=0.03, amp_s=0.9, amp_r=0.25, lfo2_rate=5.5, rev_mix=0.2),
  [("Lfo2", "PitchAll", 0.004)])
p(C, "Dub Space Pluck", dict(wt_table=1, wt_pos=0.05, flt_cutoff=0.45, flt_env=0.45, e2_d=0.25, e2_s=0,
    amp_a=0.001, amp_d=0.3, amp_s=0, amp_r=0.5, dly_mix=0.55, dly_fb=0.65, dly_time=375, dly_tone=0.35, rev_mix=0.4))
p(C, "Lovers Keys", dict(wt_table=5, wt_pos=0.2, wt_level=0.35, an_on=1, an_shape=0.3, an_level=0.7, flt_cutoff=0.55,
    amp_a=0.002, amp_d=1.2, amp_s=0.4, amp_r=0.4, amp_vel=0.7, cho_mix=0.5, rev_mix=0.25))
p(C, "Wobble Dub Bass", dict(wt_table=0, wt_pos=0.66, wt_oct=-1, wt_level=0.6, an_on=1, an_shape=0, an_oct=-2, an_sub=0.8,
    an_level=0.5, flt_cutoff=0.3, flt_res=0.4, lfo1_rate=2, amp_s=1, amp_r=0.15, fx_drive=0.2, rev_mix=0),
  [("Lfo1", "Cutoff", 0.3)])

# ============================= TRANCE / ELECTRONIC =============================
C = "Trance & Electronic"
p(C, "Hyper Supersaw", dict(wt_table=0, wt_pos=0.66, wt_unison=7, wt_detune=0.45, wt_spread=1, an_on=1, an_shape=1,
    an_level=0.3, flt_type=1, flt_cutoff=0.8, amp_a=0.01, amp_s=1, amp_r=0.6, cho_mix=0.3, rev_mix=0.35, dly_mix=0.2))
p(C, "Euphoria Pluck", dict(wt_table=0, wt_pos=0.66, wt_unison=5, wt_detune=0.25, flt_cutoff=0.35, flt_res=0.25,
    flt_env=0.55, e2_d=0.22, e2_s=0, amp_a=0.001, amp_d=0.4, amp_s=0, amp_r=0.35,
    dly_mix=0.35, dly_time=375, dly_fb=0.45, rev_mix=0.3))
p(C, "Acid Line", dict(wt_on=0, an_on=1, an_shape=1, an_oct=-1, an_level=0.9, flt_cutoff=0.3, flt_res=0.8, flt_env=0.45,
    flt_drive=0.4, e2_d=0.18, e2_s=0, amp_a=0.001, amp_d=0.3, amp_s=0.6, amp_r=0.08, glide=0.08, fx_drive=0.2, rev_mix=0.05),
  [("Velocity", "Cutoff", 0.3)])
p(C, "Gated Stab", dict(wt_table=3, wt_pos=0.4, wt_unison=3, wt_detune=0.2, flt_type=1, flt_cutoff=0.7,
    lfo1_shape=3, lfo1_rate=8, amp_s=1, amp_r=0.3, rev_mix=0.3),
  [("Lfo1", "Amp", 1.0)])
p(C, "Trance Arp Saw", dict(wt_table=4, wt_pos=0.4, flt_cutoff=0.45, flt_env=0.35, e2_d=0.15, e2_s=0,
    amp_a=0.001, amp_d=0.2, amp_s=0, amp_r=0.2, dly_mix=0.45, dly_fb=0.5, dly_time=250, rev_mix=0.2))
p(C, "Uplifter Pad", dict(wt_table=1, wt_pos=0.3, wt_unison=5, wt_detune=0.3, flt_type=1, flt_cutoff=0.85, amp_a=1.5, amp_s=1, amp_r=3,
    lfo1_rate=0.05, cho_mix=0.4, rev_mix=0.55),
  [("Lfo1", "WtPos", 0.3)])
p(C, "Reese Engine", dict(wt_table=0, wt_pos=0.66, wt_unison=4, wt_detune=0.5, an_on=1, an_shape=1, an_fine=15,
    an_level=0.5, flt_cutoff=0.5, flt_res=0.3, lfo2_rate=0.3, amp_s=1, amp_r=0.2, fx_drive=0.35, rev_mix=0.05),
  [("Lfo2", "WtDetune", 0.2)])
p(C, "Laser Lead", dict(wt_table=3, wt_pos=0.7, wt_warp=1, wt_warpamt=0.4, glide=0.05, flt_type=1, flt_cutoff=0.85,
    e2_d=0.3, e2_s=0, amp_s=0.9, amp_r=0.25, dly_mix=0.3, rev_mix=0.2),
  [("Env2", "WtWarp", 0.5)])
p(C, "Chiptune Pulse", dict(wt_on=0, an_on=1, an_shape=2, an_pw=0.25, an_drift=0, an_level=0.7, flt_on=0,
    lfo1_shape=1, lfo1_rate=0.5, amp_a=0.001, amp_s=0.8, amp_r=0.05, rev_mix=0.1),
  [("Lfo1", "AnPw", 0.3)])
p(C, "Sidechain Pump", dict(wt_table=0, wt_pos=0.66, wt_unison=5, wt_detune=0.3, flt_type=1, flt_cutoff=0.7,
    lfo1_shape=2, lfo1_rate=2, lfo1_retrig=0, amp_a=0.05, amp_s=1, amp_r=0.8, rev_mix=0.3),
  [("Lfo1", "Amp", -0.5)])
p(C, "Rave Organ 90s", dict(wt_table=5, wt_pos=0.9, an_on=1, an_shape=2, an_pw=0.5, an_level=0.4, flt_cutoff=0.65,
    amp_a=0.001, amp_d=0.5, amp_s=0.3, amp_r=0.2, cho_mix=0.3, rev_mix=0.25))
p(C, "Neural Bass", dict(wt_table=3, wt_pos=0.3, wt_oct=-1, wt_warp=5, wt_warpamt=0.4, flt_cutoff=0.4, flt_res=0.45,
    flt_drive=0.5, lfo1_shape=4, lfo1_rate=8, amp_s=1, amp_r=0.1, fx_drive=0.35, rev_mix=0),
  [("Lfo1", "WtPos", 0.3)])
p(C, "Starfield Keys", dict(wt_table=2, wt_pos=0.2, wt_level=0.5, an_on=1, an_shape=0, an_level=0.6, flt_type=1,
    flt_cutoff=0.6, flt_env=0.2, e2_d=0.6, e2_s=0, amp_a=0.002, amp_d=1.5, amp_s=0.3, amp_r=0.8, amp_vel=0.7,
    dly_mix=0.3, rev_mix=0.45))

# ============================== HIP-HOP / TRAP ===============================
C = "Hip-Hop & Trap"
p(C, "808 Boom", dict(wt_table=0, wt_pos=0, wt_oct=-1, flt_on=0, e2_a=0, e2_d=0.08, e2_s=0,
    amp_a=0.001, amp_d=1.5, amp_s=0.6, amp_r=0.4, glide=0.12, fx_drive=0.45, rev_mix=0),
  [("Env2", "PitchAll", 0.25)])
p(C, "808 Distorted", dict(wt_table=0, wt_pos=0.1, wt_oct=-1, an_on=1, an_shape=0, an_oct=-1, an_sub=0.5, an_level=0.4,
    flt_cutoff=0.5, e2_a=0, e2_d=0.08, e2_s=0, amp_a=0.001, amp_d=1.5, amp_s=0.6, amp_r=0.4, glide=0.12,
    fx_drive=0.8, rev_mix=0),
  [("Env2", "PitchAll", 0.25)])
p(C, "Dark Trap Bell", dict(wt_table=1, wt_pos=0.75, flt_type=1, flt_cutoff=0.92, amp_a=0.001, amp_d=1.2, amp_s=0, amp_r=1.2,
    dly_mix=0.25, dly_time=375, rev_mix=0.4))
p(C, "Glass Bells", dict(wt_table=3, wt_pos=0.2, flt_type=1, flt_cutoff=0.85, amp_a=0.001, amp_d=1.8, amp_s=0, amp_r=1.5,
    cho_mix=0.2, dly_mix=0.2, rev_mix=0.45))
p(C, "Haunted Keys", dict(wt_table=2, wt_pos=0.1, wt_level=0.4, an_on=1, an_shape=0, an_level=0.7, flt_cutoff=0.5,
    amp_a=0.002, amp_d=2, amp_s=0.2, amp_r=0.7, amp_vel=0.7, lfo2_rate=0.3, rev_mix=0.4),
  [("Lfo2", "PitchAll", 0.002)])
p(C, "Tape Wobble Rhodes", dict(wt_on=0, an_on=1, an_shape=0.15, an_sub=0.3, an_level=0.8, flt_cutoff=0.45,
    amp_a=0.002, amp_d=2, amp_s=0.3, amp_r=0.5, lfo1_shape=5, lfo1_rate=3, lfo1_retrig=0, cho_mix=0.4, fx_drive=0.15, rev_mix=0.2),
  [("Lfo1", "PitchAll", 0.003)])
p(C, "Choir Loop", dict(wt_on=0, gr_on=1, gr_pos=0.4, gr_size=180, gr_density=30, gr_level=0.9, flt_type=1, flt_cutoff=0.65,
    amp_a=0.3, amp_s=1, amp_r=1.5, lfo1_rate=0.1, rev_mix=0.5),
  [("Lfo1", "GrPos", 0.3)])
p(C, "Trap Flute", dict(wt_table=0, wt_pos=0.2, an_on=1, an_shape=0, an_noise=0.15, an_level=0.3, flt_type=1, flt_cutoff=0.7,
    amp_a=0.05, amp_s=0.9, amp_r=0.3, lfo2_rate=5, rev_mix=0.35),
  [("Lfo2", "PitchAll", 0.003)])
p(C, "Drill Slide Bass", dict(wt_table=0, wt_pos=0.15, wt_oct=-1, glide=0.25, flt_cutoff=0.5, amp_s=0.9, amp_r=0.2,
    fx_drive=0.55, rev_mix=0))
p(C, "Lo-Fi Pad", dict(wt_table=4, wt_pos=0.3, wt_unison=3, wt_detune=0.15, flt_cutoff=0.4, flt_res=0.15,
    amp_a=0.4, amp_s=1, amp_r=1.2, lfo1_rate=0.2, fx_drive=0.25, cho_mix=0.5, rev_mix=0.35),
  [("Lfo1", "Cutoff", 0.08)])
p(C, "Trap Brass", dict(wt_table=0, wt_pos=0.66, wt_unison=3, wt_detune=0.15, an_on=1, an_shape=1, an_level=0.5,
    flt_cutoff=0.35, flt_env=0.4, e2_a=0.08, e2_d=0.6, e2_s=0.4, amp_a=0.05, amp_s=0.9, amp_r=0.25, fx_drive=0.2, rev_mix=0.3))
p(C, "Vox Chop Lead", dict(wt_table=2, wt_pos=0.5, flt_type=1, flt_cutoff=0.8, lfo1_shape=4, lfo1_rate=4,
    amp_a=0.005, amp_s=0.8, amp_r=0.2, dly_mix=0.25, rev_mix=0.3),
  [("Lfo1", "WtPos", 0.5)])
p(C, "Mafia Strings", dict(wt_table=0, wt_pos=0.66, wt_unison=5, wt_detune=0.25, an_on=1, an_shape=1, an_level=0.4,
    flt_cutoff=0.55, flt_env=0.25, e2_d=0.3, e2_s=0, amp_a=0.003, amp_d=0.4, amp_s=0.2, amp_r=0.3, rev_mix=0.35))

# ----------------------------------------------------------------------------
SRC = dict(Env2="srcEnv2", Env3="srcEnv3", Lfo1="srcLfo1", Lfo2="srcLfo2", Velocity="srcVelocity",
           ModWheel="srcModWheel", Aftertouch="srcAftertouch", KeyTrack="srcKeyTrack", Random="srcRandom",
           Macro1="srcMacro1", Macro2="srcMacro2", Macro3="srcMacro3", Macro4="srcMacro4")
DST = dict(WtPos="dstWtPos", WtWarp="dstWtWarp", WtPitch="dstWtPitch", WtDetune="dstWtDetune", WtLevel="dstWtLevel",
           AnShape="dstAnShape", AnPw="dstAnPw", AnPitch="dstAnPitch", AnLevel="dstAnLevel",
           GrPos="dstGrPos", GrSize="dstGrSize", GrDensity="dstGrDensity", GrSpray="dstGrSpray", GrPitch="dstGrPitch",
           GrLevel="dstGrLevel", Cutoff="dstCutoff", Resonance="dstResonance", FltDrive="dstFltDrive",
           Amp="dstAmp", Pan="dstPan", PitchAll="dstPitchAll", Lfo1Rate="dstLfo1Rate", Lfo2Rate="dstLfo2Rate")

def add_macros(values, routes):
    """Every new preset gets two useful macros: 1 = brightness, 2 = character."""
    used = {r[0] for r in routes}
    if "Macro1" not in used:
        routes.append(("Macro1", "Cutoff", 0.35))
    if "Macro2" not in used:
        wt = values.get("wt_on", 1) != 0
        gr = values.get("gr_on", 0) != 0
        an = values.get("an_on", 0) != 0
        if wt:   routes.append(("Macro2", "WtPos", 0.4))
        elif gr: routes.append(("Macro2", "GrSpray", 0.5))
        elif an: routes.append(("Macro2", "AnShape", 0.4))
    return routes

def fmt(v):
    if float(v).is_integer(): return str(int(v))
    return repr(float(v)) + "f"

def main():
    levels = {}
    lp = os.path.join(HERE, "levels.json")
    if os.path.exists(lp):
        levels = json.load(open(lp))

    # Original "Signature" presets live as C++ in signature_presets.inc
    sig = open(os.path.join(HERE, "signature_presets.inc")).read()

    out = ["// GENERATED by tools/gen_presets.py - edit the specs there, then re-run it.",
           '#include "Presets.h"', "", "namespace ff", "{",
           "const std::vector<FactoryPreset>& factoryPresets()", "{",
           "    static const std::vector<FactoryPreset> presets = {"]
    out.append(sig.rstrip())
    for cat, name, values, routes in P:
        values = dict(values)
        if name in levels: values["master"] = levels[name]
        routes = add_macros(values, list(routes))
        vals = ", ".join('{ "%s", %s }' % (k, fmt(v)) for k, v in values.items())
        rts = ", ".join("{ %s, %s, %s }" % (SRC[s], DST[d], fmt(a)) for s, d, a in routes)
        out.append('        { "%s", "%s",\n          { %s },\n          { %s } },' % (name, cat, vals, rts))
    out += ["    };", "    return presets;", "}", "} // namespace ff", ""]
    open(os.path.join(ROOT, "Source", "Presets.cpp"), "w").write("\n".join(out))
    print("wrote", len(P), "new presets")

if __name__ == "__main__":
    main()
