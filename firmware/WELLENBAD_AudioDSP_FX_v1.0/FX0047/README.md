> **Build 0045:** Adds final Mid/Side Stereo Width (0–200%) after MOD FX and before limiter. See `docs/Test_Build0045.md`.

# RTAL Audio DSP Platform

## Release 0.1.0ah – Build 0034 Ducking Delay

Build 0034 adds input-controlled ducking to the wet delay output.

```text
CC22 = Duck Amount
CC23 = Duck Threshold
CC30 = Duck Release
```

Ranges:

```text
Amount    0.0 ... 1.0
Threshold -60 ... 0 dB
Release   50 ... 2000 ms
```

Defaults: Amount 0.0, Threshold -24 dB, Release 350 ms.

Serial:

```text
delay duckamount 0.75
delay duckthreshold -24
delay duckrelease 350
```

Aliases: `duck`, `duckthr`, `duckrel`.

The three parameters are integrated into RAM/NVS presets, Startup Preset,
Program Change, Smooth Transition, Compare, Revert and Commit.

NVS preset format is now version 7. Older version-6 records are rejected safely.
No O3 or fast-math pragmas are used.


## Build 0034a Hotfix

Build 0034a corrects two DSP regressions found during hardware testing.
Feedback Saturation now leaves the signal unchanged below its soft-knee
threshold. Duck Release no longer recalculates an exponential coefficient for
every audio sample; derived ducking values are updated once per audio block.


## Build 0034b – MIDI CC Safe Control

Continuous MIDI control was isolated from diagnostic I/O and bounded in the
Core-0 service loop. The live per-CC serial monitor is disabled by default,
only a bounded number of MIDI bytes is consumed per service call, repeated CC
values are coalesced, and duplicate values are ignored.

The five-second MIDI report remains available for diagnostics. MIDI realtime
bytes continue to be handled directly by the byte parser.

NVS format stays version 7 and all MIDI assignments remain unchanged.


## Build 0035 – Freeze Delay

CC78 controls Freeze. CC78 and CC115 use hysteresis: OFF 0..47, hold 48..79,
ON 80..127. Freeze smoothly crossfades new input out and effective feedback
toward unity without overwriting the stored Feedback parameter. Freeze is fully
preset-integrated. NVS format is version 8.


## Build 0039
Adds the shared RTAL Modulation Core infrastructure for future Chorus, Flanger and Phaser. Existing Delay DSP is unchanged; no modulation effect is audible yet. See `docs/Test_Build0041.md`.


## Build 0041
Adds the first real MOD FX: a high-quality stereo chorus using the shared RTAL Modulation Core and cubic Hermite fractional-delay interpolation.


## Build 0041a – Chorus Performance Fix

Optimizes the Build 0041 Stereo Chorus hot path without changing the intended chorus topology or Hermite interpolation. Per-sample critical sections are removed, statistics are block-aggregated, smoothing work is precomputed per block, and Chorus OFF now takes a low-cost warm-bypass path. See `docs/Test_Build0041a.md`.


## Build 0041b – Hybrid Chorus Optimization

Build 0041b keeps the Build 0041a real-time fixes and reduces the active chorus interpolation cost. Tap A remains cubic Hermite on both channels; Tap B uses linear interpolation. The two-tap stereo topology, RTAL ModCore, stereo phase offset, tone filter, smoothing, and warm bypass remain unchanged.


## Build 0043b performance update
Phaser coefficient generation now runs at a 4-sample control interval with linear interpolation; the allpass stages still run at full audio rate. Dedicated 2/4-stage render paths reduce hot-path overhead.


## Build 0046h
Delay coefficient optimization: sample-accurate parameter smoothing retained; feedback HPF/LPF coefficients update at most once per 16 samples and only while cutoff moves. Peak Forensics remains enabled for regression testing.
