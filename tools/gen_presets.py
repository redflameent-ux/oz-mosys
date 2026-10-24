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


# Resonator types: 0 String 1 Sitar 2 Tibetan Bowl 3 Crystal Bowl 4 Gong 5 Handpan 6 Tongue Drum 7 Tingsha/Bell
#   8 Music Box 9 Tabla 10 Hand Drum 11 Clay Pot 12 808 Cowbell 13 Scrap Metal 14 Anvil 15 Metal Pipe
#   16 Vibraphone 17 Marimba 18 E-Piano
# Scales: 0 Equal 1 Just 2 Bhairav 3 Rast 4 Bayati 5 Hijaz 6 Pythagorean.  Roots: 0=C 2=D 4=E 7=G 9=A
# Grain sources: 0 Choir 1 Monk Chant 2 Breath & Wind 3 Rain & Water 4 Your Sample
# Drive types: 0 Warm 1 Rat 2 Fuzz
NOISE = dict(an_on=1, an_tone=0, an_noise=1, an_drift=0)          # analog engine as a pure noise source
OFF = dict(wt_on=0)
def d(*parts, **kw):
    out = {}
    for p_ in parts: out.update(p_)
    out.update(kw); return out

# ============================== SACRED WORLD ==============================
C = "Sacred World"
p(C, "Sitar Raga", d(OFF, res_on=1, res_type=1, res_decay=3.5, res_bright=0.7, res_strike=0.8, res_buzz=0.5,
    scale=2, flt_on=0, amp_a=0.001, amp_s=1, amp_r=2.5, lfo2_rate=5.5, lfo2_fade=0.8, rev_mix=0.35, rev_size=0.8),
  [("Lfo2", "PitchAll", 0.003), ("ModWheel", "ResPitch", 0.04)])
p(C, "Tanpura Drone", d(OFF, res_on=1, res_type=1, res_decay=6, res_bright=0.55, res_strike=0.5, res_buzz=0.7,
    res_retrig=0.8, flt_on=0, amp_a=0.3, amp_s=1, amp_r=4, scale=1, cho_mix=0.2, rev_mix=0.5, rev_size=0.9))
p(C, "Koto Cascade", d(OFF, res_on=1, res_type=0, res_decay=2.2, res_bright=0.8, res_strike=0.9, res_buzz=0.1,
    flt_type=1, flt_cutoff=0.85, amp_a=0.001, amp_s=1, amp_r=1.5, dly_mix=0.3, dly_time=280, dly_fb=0.4, rev_mix=0.35),
  [("Velocity", "ResBright", 0.3)])
p(C, "Guzheng Glissando", d(OFF, res_on=1, res_type=0, res_decay=3.5, res_bright=0.75, res_strike=0.75,
    flt_type=1, flt_cutoff=0.9, amp_a=0.001, amp_s=1, amp_r=2.5, lfo2_rate=4.5, lfo2_fade=0.6, cho_mix=0.25,
    dly_mix=0.25, dly_time=330, rev_mix=0.45, rev_size=0.85),
  [("Lfo2", "PitchAll", 0.004)])
p(C, "Oud Dry Pluck", d(OFF, res_on=1, res_type=0, res_decay=0.9, res_bright=0.5, res_strike=0.65, res_buzz=0.15,
    scale=5, scale_root=2, flt_cutoff=0.6, amp_a=0.001, amp_s=1, amp_r=0.3, rev_mix=0.12))
p(C, "Saz Ostinato", d(OFF, res_on=1, res_type=1, res_decay=1.6, res_bright=0.75, res_strike=0.9, res_buzz=0.35,
    scale=5, scale_root=2, flt_type=2, flt_cutoff=0.65, flt_res=0.15, amp_a=0.001, amp_s=1, amp_r=0.6,
    cho_mix=0.45, tape_wow=0.4, rev_mix=0.25))
p(C, "Erhu Lament", d(OFF, res_on=1, res_type=0, res_decay=8, res_bright=0.6, res_strike=0.1, res_bow=0.75,
    flt_type=1, flt_cutoff=0.75, amp_a=0.12, amp_s=1, amp_r=0.6, glide=0.12, voice_mode=1,
    lfo2_rate=5.5, lfo2_fade=0.7, rev_mix=0.4, rev_size=0.85),
  [("Lfo2", "PitchAll", 0.008), ("ModWheel", "ResBow", 0.3)])
p(C, "Kemenche Grit", d(OFF, res_on=1, res_type=0, res_decay=8, res_bright=0.75, res_strike=0.1, res_bow=0.85,
    res_buzz=0.3, flt_type=2, flt_cutoff=0.68, flt_res=0.3, amp_a=0.08, amp_s=1, amp_r=0.4, voice_mode=1, glide=0.08,
    lfo2_rate=6.5, lfo2_fade=0.4, scale=5, scale_root=2, fx_drive=0.25, rev_mix=0.3),
  [("Lfo2", "PitchAll", 0.006)])
p(C, "Bansuri Breath", d(NOISE, wt_table=0, wt_pos=0.0, wt_level=0.7, an_level=0.25, flt_type=1, flt_cutoff=0.62,
    amp_a=0.06, amp_s=0.9, amp_r=0.35, voice_mode=1, glide=0.07, lfo2_rate=5.0, lfo2_fade=0.9,
    scale=2, rev_mix=0.45, rev_size=0.85),
  [("Lfo2", "PitchAll", 0.005), ("Env2", "PitchAll", -0.01), ("ModWheel", "AnLevel", 0.4)])
p(C, "Shakuhachi Drop", d(NOISE, wt_table=0, wt_pos=0.08, wt_level=0.65, an_level=0.35, flt_type=1, flt_cutoff=0.58,
    amp_a=0.05, amp_s=0.85, amp_r=0.6, e3_a=0, e3_d=0.6, e3_s=0, voice_mode=1, glide=0.15,
    lfo2_rate=4.0, lfo2_fade=1.2, rev_mix=0.5, rev_size=0.9),
  [("Env3", "PitchAll", 0.02), ("Env3", "AnLevel", 0.4), ("Lfo2", "PitchAll", 0.006)])
p(C, "Ney Mourning", d(NOISE, wt_table=2, wt_pos=0.85, wt_level=0.55, an_level=0.4, flt_type=1, flt_cutoff=0.55,
    amp_a=0.3, amp_s=1, amp_r=1.2, voice_mode=1, glide=0.1, scale=4, scale_root=2, lfo2_rate=4.5, lfo2_fade=1.5,
    rev_mix=0.55, rev_size=0.95),
  [("Lfo2", "PitchAll", 0.005)])
p(C, "Native Flute", d(NOISE, wt_table=0, wt_pos=0.15, wt_level=0.75, an_level=0.15, flt_type=1, flt_cutoff=0.6,
    amp_a=0.08, amp_s=0.9, amp_r=0.5, voice_mode=1, glide=0.05, lfo2_rate=5.5, lfo2_fade=1.0,
    rev_mix=0.45, rev_size=0.8),
  [("Lfo2", "PitchAll", 0.004)])

# ============================== RESONANCE & DRONES ==============================
C = "Resonance & Drones"
p(C, "Tibetan Bowl Strike", d(OFF, res_on=1, res_type=2, res_decay=9, res_bright=0.55, res_damp=0.4, res_strike=0.5,
    flt_on=0, amp_a=0.001, amp_s=1, amp_r=8, rev_mix=0.4, rev_size=0.9))
p(C, "Bowl Rubbed", d(OFF, res_on=1, res_type=2, res_decay=12, res_bright=0.45, res_strike=0.2, res_bow=0.6,
    flt_on=0, amp_a=1.5, amp_s=1, amp_r=8, lfo1_rate=0.12, rev_mix=0.5, rev_size=0.95),
  [("Lfo1", "ResBow", 0.2)])
p(C, "Crystal 528", d(OFF, res_on=1, res_type=3, res_decay=14, res_bright=0.4, res_strike=0.3, res_bow=0.35,
    tune_a4=444, flt_on=0, amp_a=0.6, amp_s=1, amp_r=10, rev_mix=0.55, rev_size=0.95))
p(C, "Crystal 432", d(OFF, res_on=1, res_type=3, res_decay=14, res_bright=0.4, res_strike=0.3, res_bow=0.35,
    tune_a4=432, flt_on=0, amp_a=0.6, amp_s=1, amp_r=10, rev_mix=0.55, rev_size=0.95))
p(C, "Temple Gong", d(OFF, res_on=1, res_type=4, res_oct=-1, res_decay=10, res_bright=0.5, res_damp=0.35, res_strike=0.35,
    flt_on=0, amp_a=0.001, amp_s=1, amp_r=9, rev_mix=0.5, rev_size=0.95))
p(C, "Handpan Kurd", d(OFF, res_on=1, res_type=5, res_decay=3.5, res_bright=0.6, res_strike=0.55,
    flt_on=0, amp_a=0.001, amp_s=1, amp_r=3, cho_mix=0.15, rev_mix=0.4, rev_size=0.8),
  [("Velocity", "ResBright", 0.3)])
p(C, "Steel Tongue", d(OFF, res_on=1, res_type=6, res_decay=4, res_bright=0.4, res_strike=0.35,
    flt_on=0, amp_a=0.001, amp_s=1, amp_r=3, rev_mix=0.4))
p(C, "Tingsha Mark", d(OFF, res_on=1, res_type=7, res_oct=2, res_decay=6, res_bright=0.8, res_strike=0.95,
    flt_type=3, flt_cutoff=0.45, amp_a=0.001, amp_s=1, amp_r=5, rev_mix=0.45, rev_size=0.9))
p(C, "Shruti Box", d(NOISE, wt_table=5, wt_pos=0.25, wt_unison=3, wt_detune=0.08, wt_level=0.7, an_level=0.12,
    flt_cutoff=0.5, amp_a=0.7, amp_s=1, amp_r=1.5, lfo1_rate=0.25, scale=1, rev_mix=0.35),
  [("Lfo1", "Amp", 0.15)])
p(C, "Rain Temple", d(OFF, gr_on=1, gr_source=3, gr_pos=0.5, gr_size=250, gr_density=30, gr_spray=0.9, gr_stereo=1,
    gr_level=0.6, res_on=1, res_type=3, res_decay=10, res_bow=0.25, res_strike=0.2, res_level=0.5,
    flt_type=1, flt_cutoff=0.7, amp_a=2, amp_s=1, amp_r=5, rev_mix=0.6, rev_size=0.95))

# ============================== MEMPHIS DRUMS ==============================
C = "Memphis Drums"
DRUM = dict(amp_a=0.001, amp_s=0, voice_mode=0)
p(C, "808 Cowbell", d(OFF, DRUM, res_on=1, res_type=12, res_decay=0.35, res_bright=0.8, res_strike=1, res_oct=1,
    flt_type=2, flt_cutoff=0.68, flt_res=0.15, amp_d=0.35, amp_r=0.2, crush_bits=12, rev_mix=0.05))
p(C, "Cowbell Gutter Tape", d(OFF, DRUM, res_on=1, res_type=12, res_decay=0.4, res_bright=0.7, res_strike=1, res_oct=1,
    flt_type=2, flt_cutoff=0.62, amp_d=0.4, amp_r=0.2, fx_drive=0.4, tape_wow=0.5, tape_age=0.6, tape_hiss=0.3, rev_mix=0))
p(C, "Blown 808 Kick", d(DRUM, wt_table=0, wt_pos=0, wt_oct=-2, e2_a=0, e2_d=0.06, e2_s=0, flt_cutoff=0.45,
    amp_d=1.6, amp_r=0.5, voice_mode=1, fx_drive=0.6, tape_age=0.7, tape_wow=0.2, rev_mix=0),
  [("Env2", "PitchAll", 0.5)])
p(C, "DR-660 Snare", d(OFF, DRUM, NOISE, an_level=0.7, res_on=1, res_type=10, res_oct=1, res_decay=0.12,
    res_strike=0.9, res_level=0.6, flt_type=3, flt_cutoff=0.38, amp_d=0.16, amp_r=0.1, crush_bits=12, crush_rate=2, rev_mix=0))
p(C, "Rimshot Tick", d(OFF, DRUM, NOISE, an_level=0.4, res_on=1, res_type=11, res_oct=2, res_decay=0.05, res_bright=0.9, res_strike=1, res_level=1,
    flt_type=3, flt_cutoff=0.45, amp_d=0.06, amp_r=0.04, crush_bits=12, rev_mix=0))
p(C, "Closed Hat Choke", d(OFF, DRUM, NOISE, an_level=0.6, res_on=1, res_type=13, res_oct=3, res_decay=0.05,
    res_bright=1, res_strike=1, res_level=0.5, flt_type=3, flt_cutoff=0.72, amp_d=0.05, amp_r=0.03, voice_mode=1,
    amp_vel=0.9, crush_bits=12, rev_mix=0),
  [("KeyTrack", "Cutoff", 0.2)])
p(C, "Open Hat Choke", d(OFF, DRUM, NOISE, an_level=0.55, res_on=1, res_type=13, res_oct=3, res_decay=0.4,
    res_bright=1, res_strike=1, res_level=0.5, flt_type=3, flt_cutoff=0.7, amp_d=0.45, amp_r=0.2, voice_mode=1,
    amp_vel=0.9, crush_bits=12, rev_mix=0))
p(C, "Lo-Fi Splash", d(OFF, DRUM, NOISE, an_level=0.5, res_on=1, res_type=13, res_oct=2, res_decay=1.4, res_bright=0.6,
    res_level=0.6, flt_type=1, flt_cutoff=0.55, amp_d=1.3, amp_r=0.4, crush_bits=10, crush_rate=3, tape_age=0.5, rev_mix=0.15))
p(C, "Reverse Crash", d(OFF, NOISE, an_level=0.5, res_on=1, res_type=13, res_oct=2, res_decay=3, res_bow=0.7,
    res_strike=0.1, res_level=0.6, flt_type=1, flt_cutoff=0.6, amp_a=1.6, amp_s=1, amp_r=0.03, crush_bits=12, tape_age=0.4, rev_mix=0.2))
p(C, "Snare Roll Machine", d(OFF, NOISE, an_level=0.7, res_on=1, res_type=10, res_oct=1, res_decay=0.08, res_strike=0.9,
    res_retrig=12, res_level=0.6, flt_type=3, flt_cutoff=0.4, amp_a=0.001, amp_s=1, amp_r=0.08, crush_bits=12, rev_mix=0),
  [("ModWheel", "Amp", 0.0)])
p(C, "Memphis Clap", d(OFF, DRUM, NOISE, an_level=0.9, flt_type=2, flt_cutoff=0.6, flt_res=0.2, amp_d=0.18, amp_r=0.12,
    lfo1_shape=3, lfo1_rate=90, crush_bits=12, rev_mix=0.12),
  [("Lfo1", "Amp", 0.6)])
p(C, "Tape Tom", d(DRUM, wt_table=0, wt_pos=0.05, e2_a=0, e2_d=0.12, e2_s=0, flt_cutoff=0.5, amp_d=0.6, amp_r=0.3,
    tape_age=0.6, tape_wow=0.3, fx_drive=0.3, rev_mix=0.1),
  [("Env2", "PitchAll", 0.25)])

# ============================== CHANTS & DARK KEYS ==============================
C = "Chants & Dark Keys"
p(C, "Gyuto Throat Chant", d(OFF, gr_on=1, gr_source=1, gr_pos=0.3, gr_size=350, gr_density=25, gr_spray=0.15,
    gr_stereo=0.7, gr_level=0.9, an_on=1, an_shape=0, an_oct=-1, an_sub=0.6, an_level=0.25, flt_type=1, flt_cutoff=0.55,
    amp_a=0.8, amp_s=1, amp_r=3, gr_scan=0.1, tape_wow=0.3, rev_mix=0.5, rev_size=0.9))
p(C, "Chopped Mantra", d(OFF, gr_on=1, gr_source=1, gr_pos=0.5, gr_size=120, gr_density=18, gr_level=0.9,
    flt_type=1, flt_cutoff=0.55, lfo1_shape=3, lfo1_rate=4, amp_a=0.005, amp_s=1, amp_r=0.2,
    tape_wow=0.5, tape_age=0.5, tape_hiss=0.4, rev_mix=0.2),
  [("Lfo1", "Amp", 1.0), ("Random", "Pan", 0.8)])
p(C, "Gothic Piano Hit", d(OFF, res_on=1, res_type=18, res_decay=3, res_bright=0.45, res_strike=0.8, res_oct=-1,
    an_on=1, an_shape=0, an_level=0.3, flt_cutoff=0.6, amp_a=0.001, amp_d=2.5, amp_s=0.2, amp_r=1.5, amp_vel=0.8,
    crush_bits=12, rev_mix=0.55, rev_size=0.9))
p(C, "Detuned Music Box", d(OFF, res_on=1, res_type=8, res_oct=1, res_decay=2.5, res_bright=0.7, res_strike=0.9,
    flt_on=0, amp_a=0.001, amp_s=1, amp_r=2, tape_wow=0.6, tape_age=0.4, tape_hiss=0.3, rev_mix=0.4))
p(C, "PCM Dread Strings", d(wt_table=0, wt_pos=0.66, wt_unison=5, wt_detune=0.2, flt_cutoff=0.45, amp_a=0.6, amp_s=1,
    amp_r=2, crush_bits=12, crush_rate=2, cho_mix=0.4, rev_mix=0.5))
p(C, "M1 Organ Gloom", d(wt_table=5, wt_pos=0.7, an_on=1, an_shape=2, an_level=0.3, flt_cutoff=0.55, amp_a=0.005,
    amp_s=1, amp_r=0.4, crush_bits=14, cho_mix=0.3, rev_mix=0.4))
p(C, "Harmonium Bed", d(NOISE, wt_table=5, wt_pos=0.45, wt_unison=2, wt_detune=0.1, an_level=0.1, flt_cutoff=0.55,
    amp_a=0.4, amp_s=1, amp_r=0.8, lfo1_rate=0.2, scale=1, rev_mix=0.3),
  [("Lfo1", "Amp", 0.1)])
p(C, "Dungchen Horn", d(wt_table=2, wt_pos=0.9, wt_oct=-2, wt_level=0.6, an_on=1, an_shape=1, an_oct=-2, an_sub=0.7,
    an_level=0.6, flt_cutoff=0.38, flt_res=0.3, flt_drive=0.4, amp_a=0.8, amp_s=1, amp_r=2, lfo2_rate=0.2,
    rev_mix=0.55, rev_size=0.95),
  [("Lfo2", "WtPos", 0.15)])

# ============================== TRAP LOW-END & FUSION PERCUSSION ==============================
C = "Trap & Fusion Percussion"
p(C, "808 Long Tail Glide", d(wt_table=0, wt_pos=0, wt_oct=-1, flt_on=0, e2_a=0, e2_d=0.06, e2_s=0, amp_a=0.001,
    amp_d=3, amp_s=0.7, amp_r=1, voice_mode=1, glide=0.15, fx_drive=0.5, tape_age=0.3, rev_mix=0),
  [("Env2", "PitchAll", 0.2)])
p(C, "Hard Digital Kick", d(NOISE, wt_table=0, wt_pos=0.05, wt_oct=-2, an_level=0.15, flt_cutoff=0.6, e2_a=0,
    e2_d=0.03, e2_s=0, amp_a=0.001, amp_d=0.35, amp_s=0, amp_r=0.1, voice_mode=1, fx_drive=0.4, rev_mix=0),
  [("Env2", "PitchAll", 0.7), ("Env2", "AnLevel", 1.0)])
p(C, "Rolling Hats", d(OFF, NOISE, an_level=0.6, res_on=1, res_type=13, res_oct=3, res_decay=0.04, res_bright=1,
    res_strike=1, res_retrig=8, res_level=0.5, flt_type=3, flt_cutoff=0.75, amp_a=0.001, amp_s=1, amp_r=0.05,
    amp_vel=1, rev_mix=0),
  [("Random", "Pan", 0.5), ("ModWheel", "ResPitch", 0.1)])
p(C, "Woodblock Clap", d(OFF, DRUM, NOISE, an_level=0.6, res_on=1, res_type=17, res_oct=2, res_decay=0.08, res_strike=1,
    flt_type=2, flt_cutoff=0.62, amp_d=0.15, amp_r=0.1, rev_mix=0.12))
p(C, "Tabla Dayan", d(OFF, DRUM, res_on=1, res_type=9, res_decay=0.8, res_bright=0.6, res_strike=0.9, flt_on=0,
    amp_d=1, amp_r=0.5, rev_mix=0.15),
  [("Velocity", "ResBright", 0.3)])
p(C, "Tabla Bayan Bend", d(OFF, DRUM, res_on=1, res_type=10, res_oct=-1, res_decay=0.7, res_bright=0.3, res_strike=0.5,
    e2_a=0.02, e2_d=0.35, e2_s=0, flt_on=0, amp_d=1, amp_r=0.5, rev_mix=0.1),
  [("Env2", "ResPitch", 0.15)])
p(C, "Darbuka Doum Tek", d(OFF, DRUM, res_on=1, res_type=10, res_decay=0.3, res_bright=0.75, res_strike=0.9, flt_on=0,
    amp_d=0.4, amp_r=0.2, rev_mix=0.1),
  [("KeyTrack", "ResBright", 0.4)])
p(C, "Udu Bloop", d(OFF, DRUM, res_on=1, res_type=11, res_oct=-1, res_decay=0.5, res_bright=0.3, res_strike=0.3,
    e2_a=0.01, e2_d=0.15, e2_s=0, flt_on=0, amp_d=0.6, amp_r=0.3, rev_mix=0.1),
  [("Env2", "ResPitch", -0.12)])
p(C, "Shaker Rain", d(OFF, DRUM, NOISE, an_level=0.6, flt_type=3, flt_cutoff=0.78, amp_a=0.01, amp_d=0.12, amp_r=0.08,
    rev_mix=0.1))
p(C, "Rainstick", d(OFF, NOISE, an_level=0.3, gr_on=1, gr_source=3, gr_size=60, gr_density=90, gr_spray=1, gr_stereo=1,
    gr_level=0.8, flt_type=3, flt_cutoff=0.55, amp_a=0.6, amp_s=1, amp_r=1.5, rev_mix=0.35))

# ============================== INDUSTRIAL ==============================
C = "Industrial"
p(C, "Scrap Iron Hit", d(OFF, DRUM, res_on=1, res_type=13, res_decay=0.8, res_bright=0.75, res_strike=1, flt_on=0,
    amp_d=0.9, amp_r=0.4, fx_drive=0.5, drive_type=1, rev_mix=0.2))
p(C, "Anvil Strike", d(OFF, DRUM, res_on=1, res_type=14, res_oct=1, res_decay=1.2, res_bright=0.9, res_strike=1, flt_on=0,
    amp_d=1.2, amp_r=0.5, rev_mix=0.25))
p(C, "Pipe Clang", d(OFF, DRUM, res_on=1, res_type=15, res_decay=3, res_bright=0.7, res_strike=0.9, flt_on=0,
    amp_d=3, amp_r=1, dly_mix=0.25, dly_time=90, dly_fb=0.5, rev_mix=0.3))
p(C, "Metal Cowbell Hybrid", d(OFF, DRUM, res_on=1, res_type=12, res_oct=1, res_decay=0.4, res_strike=1, an_on=0,
    flt_type=2, flt_cutoff=0.7, amp_d=0.5, amp_r=0.3, fx_drive=0.6, drive_type=1, ring_freq=1200, ring_mix=0.3, rev_mix=0.1))
p(C, "Gated Drop-D Chug", d(OFF, res_on=1, res_type=0, res_oct=-1, res_decay=0.35, res_bright=0.35, res_strike=1,
    flt_cutoff=0.6, amp_a=0.001, amp_d=0.12, amp_s=0, amp_r=0.03, fx_drive=0.85, drive_type=1, rev_mix=0))
p(C, "Feedback Scream", d(OFF, res_on=1, res_type=0, res_oct=1, res_decay=15, res_bright=0.9, res_bow=0.6, res_strike=0.2,
    flt_type=2, flt_cutoff=0.75, flt_res=0.6, amp_a=1.2, amp_s=1, amp_r=1, voice_mode=1, lfo2_rate=6, lfo2_fade=2,
    fx_drive=0.9, drive_type=2, rev_mix=0.3),
  [("Lfo2", "PitchAll", 0.004)])
p(C, "Steam Pipe Flute", d(NOISE, wt_table=0, wt_pos=0.1, an_level=0.7, flt_type=1, flt_cutoff=0.7, amp_a=0.05, amp_s=0.9,
    amp_r=0.4, voice_mode=1, ring_freq=720, ring_mix=0.45, crush_bits=8, dly_mix=0.35, dly_time=40, dly_fb=0.7, rev_mix=0.2))
p(C, "Demonic Bowl", d(OFF, res_on=1, res_type=2, res_oct=-1, res_decay=12, res_bow=0.7, res_strike=0.2, flt_cutoff=0.6,
    amp_a=1.5, amp_s=1, amp_r=5, fx_drive=0.8, drive_type=0, tape_age=0.3, rev_mix=0.45))
p(C, "Closer Noise Snare", d(OFF, DRUM, NOISE, an_level=0.9, flt_type=2, flt_cutoff=0.6, flt_res=0.2, amp_d=0.22,
    amp_r=0.15, fx_drive=0.6, drive_type=2, crush_bits=10, rev_mix=0.08))
p(C, "Prophet Saw Teeth", d(wt_table=0, wt_pos=0.66, wt_unison=4, wt_detune=0.35, an_on=1, an_shape=1, an_oct=-1,
    an_level=0.6, flt_cutoff=0.4, flt_res=0.4, flt_env=0.3, e2_d=0.25, e2_s=0.2, amp_s=1, amp_r=0.1, glide=0.04,
    fx_drive=0.5, drive_type=1, rev_mix=0))
p(C, "Glitch Grains", d(OFF, gr_on=1, gr_source=0, gr_size=18, gr_density=60, gr_spray=1, gr_pitchrand=12,
    gr_reverse=0.5, gr_stereo=1, gr_level=0.9, flt_on=0, amp_s=1, amp_r=0.5, crush_bits=8, crush_rate=4, rev_mix=0.1),
  [("Lfo1", "GrPos", 0.5)])

# ============================== JAPANESE ACID JAZZ ==============================
C = "Japanese Acid Jazz"
p(C, "Tokyo Rhodes", d(OFF, res_on=1, res_type=18, res_decay=3, res_bright=0.55, res_strike=0.7, res_buzz=0.15,
    flt_on=0, amp_a=0.001, amp_d=3, amp_s=0.3, amp_r=0.5, amp_vel=0.8, lfo1_rate=4.5, cho_mix=0.35, rev_mix=0.2),
  [("Lfo1", "Pan", 0.4), ("Velocity", "ResBuzz", 0.3)])
p(C, "Vibes Tremolo", d(OFF, res_on=1, res_type=16, res_decay=4, res_bright=0.55, res_strike=0.6, flt_on=0,
    amp_a=0.001, amp_s=1, amp_r=2, lfo1_rate=5, lfo1_retrig=0, rev_mix=0.3),
  [("Lfo1", "Amp", 0.3)])
p(C, "Shibuya Clav", d(OFF, an_on=1, an_shape=2, an_pw=0.15, an_level=0.8, flt_cutoff=0.4, flt_res=0.45, flt_env=0.4,
    e2_d=0.15, e2_s=0, amp_a=0.001, amp_d=0.3, amp_s=0.2, amp_r=0.08, amp_vel=0.9, fx_drive=0.2, cho_mix=0.2, rev_mix=0.1),
  [("Velocity", "Cutoff", 0.3)])
p(C, "Upright Walk", d(OFF, res_on=1, res_type=0, res_oct=-1, res_decay=1.5, res_bright=0.35, res_strike=0.5,
    flt_cutoff=0.5, amp_a=0.001, amp_s=1, amp_r=0.25, amp_vel=0.6, rev_mix=0.08))
p(C, "Flute Loop Jazz", d(NOISE, wt_table=0, wt_pos=0.1, an_level=0.18, flt_type=1, flt_cutoff=0.65, amp_a=0.04,
    amp_s=0.9, amp_r=0.3, lfo2_rate=5, lfo2_fade=0.6, tape_wow=0.3, tape_age=0.3, dly_mix=0.2, rev_mix=0.3),
  [("Lfo2", "PitchAll", 0.004)])
p(C, "Smoky Organ", d(wt_table=5, wt_pos=0.6, flt_cutoff=0.7, amp_a=0.005, amp_s=1, amp_r=0.15, lfo1_rate=6.5,
    lfo1_retrig=0, fx_drive=0.25, rev_mix=0.15),
  [("Lfo1", "Amp", 0.12), ("Lfo1", "PitchAll", 0.002)])

# ============================== 60s PSYCH ==============================
C = "60s Psych"
p(C, "Fuzz Lead 66", d(OFF, res_on=1, res_type=0, res_decay=6, res_bright=0.6, res_strike=0.9, flt_cutoff=0.7,
    amp_a=0.001, amp_s=1, amp_r=0.4, voice_mode=1, glide=0.04, lfo2_rate=5.5, lfo2_fade=0.8, fx_drive=0.75, drive_type=2,
    rev_mix=0.3),
  [("Lfo2", "PitchAll", 0.004)])
p(C, "Farfisa Swirl", d(wt_table=4, wt_pos=0.1, an_on=1, an_shape=2, an_level=0.3, flt_cutoff=0.75, amp_a=0.003,
    amp_s=1, amp_r=0.1, lfo2_rate=6, cho_mix=0.6, cho_rate=0.8, rev_mix=0.25),
  [("Lfo2", "PitchAll", 0.003)])
p(C, "Mellotron Flutes", d(NOISE, wt_table=0, wt_pos=0.05, wt_unison=2, wt_detune=0.08, an_level=0.12,
    flt_type=1, flt_cutoff=0.6, amp_a=0.12, amp_s=0.9, amp_r=0.6, tape_wow=0.6, tape_age=0.5, tape_hiss=0.25, rev_mix=0.3))
p(C, "Electric Sitar", d(OFF, res_on=1, res_type=1, res_decay=2.5, res_bright=0.8, res_strike=0.9, res_buzz=0.6,
    flt_on=0, amp_a=0.001, amp_s=1, amp_r=1, cho_mix=0.3, rev_mix=0.3))
p(C, "Backwards Guitar", d(OFF, res_on=1, res_type=0, res_decay=4, res_bright=0.6, res_bow=0.4, res_strike=0.3,
    flt_cutoff=0.65, amp_a=0.9, amp_s=1, amp_r=0.05, fx_drive=0.3, drive_type=0, tape_wow=0.4, dly_mix=0.3, rev_mix=0.4))
p(C, "Phase Out Pad", d(wt_table=1, wt_pos=0.3, wt_unison=3, wt_detune=0.15, flt_type=4, flt_cutoff=0.55, flt_res=0.6,
    amp_a=0.8, amp_s=1, amp_r=2, lfo1_shape=1, lfo1_rate=0.25, cho_mix=0.4, rev_mix=0.45),
  [("Lfo1", "Cutoff", 0.3)])

# ============================== DUNGEON SYNTH ==============================
C = "Dungeon Synth"
p(C, "Crypt Choir", d(wt_table=2, wt_pos=0.9, wt_unison=4, wt_detune=0.15, gr_on=1, gr_source=0, gr_level=0.5,
    flt_type=1, flt_cutoff=0.5, amp_a=1.2, amp_s=1, amp_r=3, crush_bits=14, rev_mix=0.6, rev_size=0.95))
p(C, "Castle Strings", d(wt_table=0, wt_pos=0.66, wt_unison=4, wt_detune=0.15, flt_cutoff=0.4, amp_a=0.8, amp_s=1,
    amp_r=2.5, tape_age=0.4, tape_wow=0.2, rev_mix=0.55, rev_size=0.9))
p(C, "Lute of the Keep", d(OFF, res_on=1, res_type=0, res_decay=2.2, res_bright=0.45, res_strike=0.6, flt_cutoff=0.65,
    amp_a=0.001, amp_s=1, amp_r=1.2, cho_mix=0.15, rev_mix=0.45))
p(C, "Tomb Organ", d(wt_table=5, wt_pos=0.85, wt_oct=-1, an_on=1, an_shape=2, an_oct=-1, an_level=0.3, flt_cutoff=0.5,
    amp_a=0.3, amp_s=1, amp_r=2, rev_mix=0.65, rev_size=0.95))
p(C, "Fantasy Horn", d(wt_table=0, wt_pos=0.6, wt_unison=2, wt_detune=0.1, flt_cutoff=0.32, flt_env=0.25, e2_a=0.15,
    e2_d=0.8, e2_s=0.6, amp_a=0.15, amp_s=1, amp_r=0.8, rev_mix=0.5))
p(C, "Torchlit Bells", d(OFF, res_on=1, res_type=7, res_decay=4, res_bright=0.45, res_strike=0.5, flt_on=0, amp_a=0.001,
    amp_s=1, amp_r=3, crush_bits=12, rev_mix=0.55, rev_size=0.95))

# ============================== 8-BIT LO-FI ==============================
C = "8-Bit Lo-Fi"
CHIP = dict(crush_bits=6, crush_rate=4)
p(C, "Square Lead 50", d(OFF, an_on=1, an_shape=2, an_pw=0.5, an_drift=0, an_level=0.7, flt_on=0, amp_a=0.001, amp_s=0.9,
    amp_r=0.05, voice_mode=1, lfo2_rate=6, lfo2_fade=0.3, crush_bits=8, rev_mix=0.05),
  [("Lfo2", "PitchAll", 0.006)])
p(C, "Pulse 12 Arp", d(OFF, CHIP, an_on=1, an_shape=2, an_pw=0.12, an_drift=0, an_level=0.7, flt_on=0, amp_a=0.001,
    amp_d=0.15, amp_s=0.3, amp_r=0.05, dly_mix=0.2, dly_time=150, rev_mix=0))
p(C, "Triangle Bass", d(OFF, an_on=1, an_shape=0, an_oct=-1, an_drift=0, an_level=0.9, flt_on=0, amp_a=0.001, amp_s=1,
    amp_r=0.05, voice_mode=1, crush_bits=5, rev_mix=0))
p(C, "Noise Drum Chip", d(OFF, DRUM, NOISE, an_level=0.8, flt_on=0, amp_d=0.12, amp_r=0.05, crush_bits=4, crush_rate=8, rev_mix=0),
  [("KeyTrack", "Amp", 0.0)])
p(C, "Game Over Pad", d(CHIP, wt_table=4, wt_pos=0.3, flt_cutoff=0.6, amp_a=0.3, amp_s=1, amp_r=1.5, tape_wow=0.4,
    tape_age=0.4, rev_mix=0.35))
p(C, "Lo-Fi Chip Keys", d(OFF, res_on=1, res_type=18, res_decay=1.5, res_strike=0.7, flt_cutoff=0.6, amp_a=0.001,
    amp_s=1, amp_r=0.5, crush_bits=8, crush_rate=3, tape_wow=0.5, tape_age=0.5, tape_hiss=0.4, rev_mix=0.2))

# ============================== KRAUTROCK ==============================
C = "Krautrock"
p(C, "Motorik Bass", d(OFF, an_on=1, an_shape=1, an_oct=-1, an_sub=0.4, an_level=0.9, flt_cutoff=0.35, flt_res=0.3,
    flt_env=0.3, e2_d=0.15, e2_s=0, amp_a=0.001, amp_d=0.25, amp_s=0.5, amp_r=0.08, rev_mix=0))
p(C, "Kosmische Sequence", d(wt_table=4, wt_pos=0.2, flt_cutoff=0.35, flt_res=0.55, flt_env=0.4, e2_d=0.12, e2_s=0,
    amp_a=0.001, amp_d=0.25, amp_s=0, amp_r=0.15, lfo1_rate=0.05, dly_mix=0.4, dly_time=375, dly_fb=0.55, rev_mix=0.3),
  [("Lfo1", "Cutoff", 0.25)])
p(C, "Organ Drone Kraut", d(wt_table=5, wt_pos=0.5, an_on=1, an_shape=1, an_level=0.3, flt_cutoff=0.55, amp_a=0.2,
    amp_s=1, amp_r=1, lfo1_shape=1, lfo1_rate=0.1, fx_drive=0.3, rev_mix=0.35),
  [("Lfo1", "Cutoff", 0.15)])
p(C, "Tape Echo Guitar", d(OFF, res_on=1, res_type=0, res_decay=2, res_bright=0.65, res_strike=0.8, flt_cutoff=0.7,
    amp_a=0.001, amp_s=1, amp_r=0.5, fx_drive=0.35, dly_mix=0.45, dly_time=420, dly_fb=0.6, dly_tone=0.4, tape_wow=0.3, rev_mix=0.2))
p(C, "Cluster Synth", d(wt_table=1, wt_pos=0.5, wt_unison=3, wt_detune=0.4, flt_type=2, flt_cutoff=0.6, flt_res=0.5,
    amp_a=0.4, amp_s=1, amp_r=2, lfo1_shape=5, lfo1_rate=0.3, ring_freq=180, ring_mix=0.2, rev_mix=0.45),
  [("Lfo1", "Cutoff", 0.3), ("Lfo1", "WtPos", 0.3)])
p(C, "Neu! Hammer Pulse", d(wt_table=0, wt_pos=0.66, flt_cutoff=0.5, lfo1_shape=3, lfo1_rate=4, lfo1_retrig=0, amp_s=1,
    amp_r=0.2, rev_mix=0.2),
  [("Lfo1", "Amp", 0.6)])

# ============================== ZAMROCK ==============================
C = "Zamrock"
p(C, "Lusaka Fuzz", d(OFF, res_on=1, res_type=0, res_decay=3, res_bright=0.6, res_strike=0.9, flt_cutoff=0.65,
    amp_a=0.001, amp_s=1, amp_r=0.3, fx_drive=0.85, drive_type=2, tape_age=0.4, rev_mix=0.15))
p(C, "Garage Organ", d(wt_table=5, wt_pos=0.8, flt_cutoff=0.7, amp_a=0.003, amp_s=1, amp_r=0.1, fx_drive=0.45,
    drive_type=1, tape_age=0.4, rev_mix=0.2))
p(C, "Copperbelt Bass", d(OFF, res_on=1, res_type=0, res_oct=-1, res_decay=1, res_bright=0.4, res_strike=0.7,
    flt_cutoff=0.45, amp_a=0.001, amp_s=1, amp_r=0.1, fx_drive=0.35, tape_age=0.5, rev_mix=0))
p(C, "Wah Rhythm", d(OFF, res_on=1, res_type=0, res_decay=0.6, res_bright=0.7, res_strike=1, flt_type=2, flt_cutoff=0.55,
    flt_res=0.6, amp_a=0.001, amp_d=0.2, amp_s=0.3, amp_r=0.1, fx_drive=0.6, drive_type=2, rev_mix=0.1),
  [("ModWheel", "Cutoff", 0.35), ("Env2", "Cutoff", 0.25)])
p(C, "Afro Psych Lead", d(wt_table=0, wt_pos=0.8, an_on=1, an_shape=2, an_pw=0.3, an_level=0.4, flt_cutoff=0.6,
    amp_a=0.01, amp_s=1, amp_r=0.3, voice_mode=1, glide=0.05, fx_drive=0.6, drive_type=2, tape_age=0.5, tape_wow=0.3,
    rev_mix=0.2))

# ============================== JAZZ NOIR ==============================
C = "Jazz Noir"
p(C, "Muted Trumpet", d(wt_table=2, wt_pos=0.45, wt_level=0.8, flt_type=2, flt_cutoff=0.6, flt_res=0.35, amp_a=0.04,
    amp_s=0.9, amp_r=0.25, voice_mode=1, glide=0.04, lfo2_rate=5, lfo2_fade=0.6, rev_mix=0.35),
  [("Lfo2", "PitchAll", 0.004), ("Velocity", "Cutoff", 0.2)])
p(C, "Midnight Vibes", d(OFF, res_on=1, res_type=16, res_decay=5, res_bright=0.4, res_strike=0.5, flt_on=0, amp_a=0.001,
    amp_s=1, amp_r=3, lfo1_rate=3.5, lfo1_retrig=0, rev_mix=0.45, rev_size=0.85),
  [("Lfo1", "Amp", 0.2)])
p(C, "Smoke Rhodes", d(OFF, res_on=1, res_type=18, res_decay=4, res_bright=0.35, res_strike=0.5, flt_cutoff=0.6,
    amp_a=0.002, amp_d=3, amp_s=0.3, amp_r=0.8, cho_mix=0.3, tape_age=0.3, rev_mix=0.35))
p(C, "Noir Upright", d(OFF, res_on=1, res_type=0, res_oct=-1, res_decay=1.8, res_bright=0.3, res_strike=0.45,
    flt_cutoff=0.45, amp_a=0.001, amp_s=1, amp_r=0.35, amp_vel=0.6, rev_mix=0.15))
p(C, "Brushed Snare", d(OFF, NOISE, an_level=0.5, flt_type=2, flt_cutoff=0.68, amp_a=0.06, amp_d=0.4, amp_s=0,
    amp_r=0.2, rev_mix=0.2))
p(C, "Rain-Slick Pad", d(wt_table=1, wt_pos=0.2, wt_unison=3, wt_detune=0.12, gr_on=1, gr_source=3, gr_level=0.3,
    gr_spray=0.8, flt_type=1, flt_cutoff=0.5, amp_a=1.5, amp_s=1, amp_r=3, tape_hiss=0.3, tape_age=0.3, rev_mix=0.55))

# ============================== FM CLASSICS (FM8 / DX spirit) ==============================
C = "FM Classics"
FM = dict(wt_on=0, fm_on=1, fm_m21=0)          # FM engine only; routings set per preset
def op(i, ratio=1, level=0, a=0.002, d=0.8, s_=0.4, r=0.4, vel=0.5, fine=0, wave=0):
    k = "fm%d_" % i
    return {k+"ratio": ratio, k+"level": level, k+"a": a, k+"d": d, k+"s": s_, k+"r": r, k+"vel": vel, k+"fine": fine, k+"wave": wave}
p(C, "Tine E-Piano", d(FM, op(1, 1, 1, d=3.5, s_=0, r=0.7), op(2, 1, 0, d=1.4, s_=0.08, vel=0.85),
    op(3, 1, 0.45, d=2.2, s_=0, r=0.5), op(4, 14, 0, d=0.22, s_=0, vel=0.9),
    fm_m21=0.42, fm_m43=0.33, flt_cutoff=0.9, amp_a=0.001, amp_s=1, amp_r=0.7, cho_mix=0.35, cho_rate=0.4, rev_mix=0.2),
  [("Velocity", "FmIndex", 0.3)])
p(C, "Tubular Bell", d(FM, op(1, 1, 1, d=7, s_=0, r=4), op(2, 3.5, 0, d=5, s_=0), op(3, 1, 0.5, d=5, s_=0, r=3, fine=4),
    op(4, 5.19, 0, d=3, s_=0), fm_m21=0.5, fm_m43=0.4, flt_on=0, amp_a=0.001, amp_s=1, amp_r=4, rev_mix=0.4, rev_size=0.85))
p(C, "DX Bass Pluck", d(FM, op(1, 1, 1, d=1.2, s_=0.6, r=0.15), op(2, 1, 0, d=0.35, s_=0.1, vel=0.8),
    fm_m21=0.55, fm_m22=0.45, fm_oct=-1, flt_cutoff=0.75, amp_a=0.001, amp_s=1, amp_r=0.15, voice_mode=1, glide=0.02, rev_mix=0))
p(C, "Growl Bass", d(FM, op(1, 1, 1, d=3, s_=1, r=0.2), op(2, 2, 0, d=3, s_=1), op(3, 0.5, 0.5, d=3, s_=1),
    fm_m21=0.5, fm_m22=0.35, fm_oct=-1, lfo1_shape=0, lfo1_rate=3, flt_cutoff=0.55, flt_res=0.3, fx_drive=0.35,
    amp_a=0.002, amp_s=1, amp_r=0.2, voice_mode=1, glide=0.04, rev_mix=0),
  [("Lfo1", "FmIndex", 0.35)])
p(C, "Glass Pad", d(FM, op(1, 1, 0.8, a=1.5, d=3, s_=1, r=3, fine=-7), op(2, 3, 0, a=2, d=4, s_=0.6, r=3),
    op(3, 2, 0.6, a=1.8, d=3, s_=1, r=3, fine=7), op(4, 7, 0, a=2.5, d=5, s_=0.3, r=3),
    fm_m21=0.25, fm_m43=0.2, flt_type=1, flt_cutoff=0.7, amp_a=1.2, amp_s=1, amp_r=3, lfo1_rate=0.15, cho_mix=0.5,
    rev_mix=0.55, rev_size=0.9),
  [("Lfo1", "FmIndex", 0.2)])
p(C, "Brass Stab", d(FM, op(1, 1, 1, a=0.03, d=0.6, s_=0.8, r=0.25), op(2, 1, 0, a=0.09, d=0.6, s_=0.7, r=0.25),
    op(3, 1, 0, a=0.07, d=0.5, s_=0.5, fine=5), op(4, 2, 0, a=0.1, d=0.4, s_=0.4),
    fm_m21=0.42, fm_m31=0.3, fm_m41=0.25, flt_cutoff=0.8, amp_a=0.02, amp_s=1, amp_r=0.25, cho_mix=0.25, rev_mix=0.2),
  [("Velocity", "FmIndex", 0.35)])
p(C, "Digital Clav", d(FM, op(1, 1, 1, d=0.9, s_=0, r=0.12), op(2, 3, 0, d=0.3, s_=0, vel=0.9),
    op(3, 1, 0.5, d=0.7, s_=0, r=0.1), op(4, 7, 0, d=0.12, s_=0),
    fm_m21=0.5, fm_m43=0.45, flt_cutoff=0.85, amp_a=0.001, amp_s=1, amp_r=0.12, rev_mix=0.12))
p(C, "FM Marimba", d(FM, op(1, 1, 1, d=0.9, s_=0, r=0.4), op(2, 3.5, 0, d=0.07, s_=0, vel=0.9),
    op(3, 4, 0.25, d=0.25, s_=0, r=0.2), fm_m21=0.55, flt_on=0, amp_a=0.001, amp_s=1, amp_r=0.5, rev_mix=0.25))
p(C, "Six-Bar Organ", d(FM, op(1, 0.5, 0.8, a=0.005, d=1, s_=1, r=0.08), op(2, 1, 0.7, a=0.005, d=1, s_=1, r=0.08),
    op(3, 2, 0.5, a=0.005, d=1, s_=1, r=0.08), op(4, 3, 0.4, a=0.005, d=1, s_=1, r=0.08),
    op(5, 4, 0.3, a=0.005, d=1, s_=1, r=0.08), op(6, 6, 0.25, a=0.005, d=1, s_=1, r=0.08),
    flt_cutoff=0.8, amp_a=0.004, amp_s=1, amp_r=0.08, lfo2_rate=6.5, cho_mix=0.45, cho_rate=1.2, rev_mix=0.2),
  [("Lfo2", "Amp", 0.08)])
p(C, "Metal Sweep", d(FM, op(1, 1, 1, a=0.01, d=4, s_=0.6, r=2), op(2, 1.41, 0, d=4, s_=0.7), op(3, 2.7, 0, d=4, s_=0.7),
    op(4, 3.3, 0, d=4, s_=0.7), op(5, 5.1, 0, d=4, s_=0.7), op(6, 7.9, 0, d=4, s_=0.7),
    fm_m65=0.35, fm_m54=0.35, fm_m43=0.35, fm_m32=0.35, fm_m21=0.4, fm_index=0.4,
    e3_a=4, e3_d=1, e3_s=1, e3_r=2, flt_cutoff=0.7, amp_a=0.3, amp_s=1, amp_r=2, rev_mix=0.45, dly_mix=0.25),
  [("Env3", "FmIndex", 0.7)])
p(C, "FM Sitar", d(FM, op(1, 1, 1, d=3, s_=0.1, r=0.6), op(2, 1, 0, d=2.5, s_=0.3), op(3, 3, 0, d=0.5, s_=0, vel=0.8),
    fm_m21=0.45, fm_m22=0.55, fm_m31=0.3, flt_cutoff=0.8, amp_a=0.001, amp_s=1, amp_r=0.6, rev_mix=0.3,
    scale=2), [])
p(C, "Space Bell Pad", d(FM, op(1, 1, 1, a=1.2, d=6, s_=0.5, r=5), op(2, 3.5, 0, a=1.5, d=5, s_=0.3), op(3, 2, 0.6, a=1.4, d=6, s_=0.5, r=5, fine=5),
    op(4, 5.19, 0, a=2, d=5, s_=0.2), op(5, 1.5, 0, a=2.5, d=8, s_=0.4), op(6, 0.5, 0.5, a=2, d=6, s_=1, r=5),
    fm_m21=0.4, fm_m43=0.35, fm_m54=0.2, flt_type=1, flt_cutoff=0.75, amp_a=1.5, amp_s=1, amp_r=5,
    cho_mix=0.4, dly_mix=0.25, dly_time=480, dly_fb=0.5, rev_mix=0.6, rev_size=0.95),
  [("Lfo1", "FmIndex", 0.15)])

# ----------------------------------------------------------------------------
SRC = dict(Env2="srcEnv2", Env3="srcEnv3", Lfo1="srcLfo1", Lfo2="srcLfo2", Velocity="srcVelocity",
           ModWheel="srcModWheel", Aftertouch="srcAftertouch", KeyTrack="srcKeyTrack", Random="srcRandom",
           Macro1="srcMacro1", Macro2="srcMacro2", Macro3="srcMacro3", Macro4="srcMacro4")
DST = dict(WtPos="dstWtPos", WtWarp="dstWtWarp", WtPitch="dstWtPitch", WtDetune="dstWtDetune", WtLevel="dstWtLevel",
           AnShape="dstAnShape", AnPw="dstAnPw", AnPitch="dstAnPitch", AnLevel="dstAnLevel",
           GrPos="dstGrPos", GrSize="dstGrSize", GrDensity="dstGrDensity", GrSpray="dstGrSpray", GrPitch="dstGrPitch",
           GrLevel="dstGrLevel", Cutoff="dstCutoff", Resonance="dstResonance", FltDrive="dstFltDrive",
           Amp="dstAmp", Pan="dstPan", PitchAll="dstPitchAll", Lfo1Rate="dstLfo1Rate", Lfo2Rate="dstLfo2Rate",
           ResPitch="dstResPitch", ResBright="dstResBright", ResDecay="dstResDecay", ResBow="dstResBow",
           ResLevel="dstResLevel", ResBuzz="dstResBuzz", FmPitch="dstFmPitch", FmIndex="dstFmIndex", FmLevel="dstFmLevel")

def add_macros(values, routes):
    """Every new preset gets two useful macros: 1 = brightness, 2 = character."""
    used = {r[0] for r in routes}
    if "Macro1" not in used:
        if values.get("flt_on", 1) != 0: routes.append(("Macro1", "Cutoff", 0.35))
        elif values.get("res_on", 0):    routes.append(("Macro1", "ResDecay", 0.5))
    if "Macro2" not in used:
        wt = values.get("wt_on", 1) != 0
        gr = values.get("gr_on", 0) != 0
        an = values.get("an_on", 0) != 0
        rs = values.get("res_on", 0) != 0
        fm = values.get("fm_on", 0) != 0
        if fm:   routes.append(("Macro2", "FmIndex", 0.4))
        elif rs:   routes.append(("Macro2", "ResBright", 0.4))
        elif wt: routes.append(("Macro2", "WtPos", 0.4))
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
           '#include "Presets.h"', "#include <map>", "#include <array>", "", "namespace ff", "{",
           "const std::vector<FactoryPreset>& factoryPresets()", "{",
           "    static const std::vector<FactoryPreset> presets = [] {",
           "    std::vector<FactoryPreset> v = {"]
    out.append(sig.rstrip())
    for cat, name, values, routes in P:
        values = dict(values)
        if name in levels: values["master"] = levels[name]
        routes = add_macros(values, [r for r in routes if r[2] != 0])
        vals = ", ".join('{ "%s", %s }' % (k, fmt(v)) for k, v in values.items())
        rts = ", ".join("{ %s, %s, %s }" % (SRC[s], DST[d], fmt(a)) for s, d, a in routes)
        out.append('        { "%s", "%s",\n          { %s },\n          { %s } },' % (name, cat, vals, rts))
    # sound-map positions measured by the render tool (tools/features.json)
    feats = {}
    fp = os.path.join(HERE, "features.json")
    if os.path.exists(fp):
        feats = json.load(open(fp))
    def pct(vals, x):
        return sum(1 for v in vals if v < x) / max(1, len(vals))
    names = list(feats.keys())
    bs = [feats[n][0] for n in names]; ag = [feats[n][1] for n in names]; mv = [feats[n][2] for n in names]
    out += ["    };",
            "    static const std::map<juce::String, std::array<float, 6>> map = {"]
    for n in names:
        b, a, m = feats[n]
        out.append('        { "%s", { %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff } },' % (n.replace('"','\\"'), pct(bs,b), pct(ag,a), pct(mv,m), b, a, m))
    out += ["    };",
            "    for (auto& p : v)",
            "        if (auto it = map.find (p.name); it != map.end())",
            "        {",
            "            p.mapX = it->second[0]; p.mapY = it->second[1]; p.movement = it->second[2];",
            "            p.rawBright = it->second[3]; p.rawAggro = it->second[4]; p.rawMove = it->second[5];",
            "        }",
            "    return v; }();",
            "    return presets;", "}", "} // namespace ff", ""]
    open(os.path.join(ROOT, "Source", "Presets.cpp"), "w").write("\n".join(out))
    print("wrote", len(P), "new presets")

if __name__ == "__main__":
    main()
