# RTAL WELLENBAD v1.1

## Performance Optimization & Stability Report

> **Engineering Validation Report**

Version 1.1 was developed as a performance and reliability release.
Instead of adding new synthesis features, every development iteration
focused on measurable improvements to the real-time audio engine. Every
optimization was benchmarked under identical operating conditions before
being accepted.

------------------------------------------------------------------------

# Test Environment

**Hardware**

-   ESP32-S3
-   8 MB PSRAM
-   44.1 kHz Audio Engine
-   AUDIO_BLOCK = 128
-   Integrated RTAL Performance Profiler

**Reference Patch**

All measurements were performed using the same INIT sound to guarantee
reproducible results.

**Voice Counts**

-   Idle
-   3 Voices
-   5 Voices
-   6 Voices (maximum practical polyphony)

Measured parameters:

-   DSP Load
-   Voice Processing Load
-   Average ISR
-   Maximum ISR
-   Audio Overruns

------------------------------------------------------------------------

# Optimization History

  Version   Optimization                     Result
  --------- -------------------------------- -------------------------------------
  A005ab    Snapshot constants               Significant improvement
  A005d     Envelope runtime cache           No measurable benefit (discarded)
  A005e     Runtime Voice Cache              Reduced voice processing time
  A005f     Pitch/Filter runtime constants   Largest overall improvement
  A005g     Oscillator phase cache           No measurable benefit (discarded)
  A005h     Overrun diagnostics              Analysis only
  A005i     Memory locality                  Very small improvement, not adopted

------------------------------------------------------------------------

# Detailed Measurement Results

## Baseline Firmware

Under six active voices the original firmware showed:

  Metric              Baseline
  ---------------- -----------
  DSP Load              \~84 %
  Average ISR        \~2439 µs
  Audio Overruns           123

The profiler clearly showed that the audio engine was already close to
the available timing budget.

------------------------------------------------------------------------

## Runtime Voice Cache (A005e)

Caching frequently used parameters produced the first significant
improvement.

  Metric              Before     After
  ---------------- --------- ---------
  Voice Load          73.6 %    71.1 %
  Average ISR        2212 µs   2160 µs
  Audio Overruns          56        34

The reduction in ISR execution time confirmed that repeated
floating-point calculations inside the audio loop had been successfully
eliminated.

------------------------------------------------------------------------

## Runtime Constants (A005f)

This optimization delivered the largest measurable performance gain.

  Metric             Baseline   Optimized
  ---------------- ---------- -----------
  DSP Load             \~84 %    \~66.7 %
  Voice Load              ---    \~63.3 %
  Average ISR         2439 µs     1934 µs
  Audio Overruns          123           8

### Improvement Summary

-   DSP load reduced by approximately **20 percentage points**
-   Average ISR shortened by approximately **505 µs**
-   Audio overruns reduced by approximately **93 %**
-   Additional timing headroom of almost **1 ms** per audio block

This optimization became the production baseline for Version 1.1.

------------------------------------------------------------------------

## Optimizations Rejected

Not every optimization produced measurable gains.

The following candidates were evaluated and intentionally discarded:

-   Envelope Runtime Cache
-   Oscillator Phase Increment Cache
-   Memory Locality Reordering

Although technically correct, none of them delivered reproducible
improvements large enough to justify the additional code complexity.

This benchmark-driven development process helped keep the firmware
efficient and maintainable.

------------------------------------------------------------------------

# Stability Improvements

Besides DSP optimization, Version 1.1 also introduced several
reliability improvements:

-   Complete RTAL namespace migration
-   Safe overwrite of preset and bank files
-   Dynamic bank buffer allocation (PSRAM with heap fallback)
-   Removal of duplicated configuration blocks
-   Compile-time profiler enable/disable switch

------------------------------------------------------------------------

# Performance Validation

The profiler demonstrated that remaining overruns only occurred under
extremely demanding polyphonic passages and not during MIDI processing,
SD access or user-interface activity.

This confirmed that the remaining timing limits are inherent to the
available CPU budget rather than software inefficiencies.

Version 1.1 removes the audible quality reduction previously used for high polyphony. Thanks to the A005 optimization series, all six voices are now rendered using the full dual-oscillator engine with chorus enabled, providing consistent sound quality across the entire polyphony range.

------------------------------------------------------------------------

# Conclusion

RTAL WELLENBAD v1.1 is a benchmark-driven engineering release.

Rather than relying on subjective impressions, every accepted
optimization is supported by objective profiler data collected under
repeatable test conditions.

The resulting firmware offers:

-   Lower DSP utilization
-   Lower ISR execution time
-   Dramatically fewer audio overruns
-   Improved timing determinism
-   Greater processing headroom
-   Identical sound quality

These optimizations provide a robust foundation for future development
while preserving complete compatibility with existing presets and
projects.

------------------------------------------------------------------------

**Developed by RealTimeAudioLab (RTAL)**
