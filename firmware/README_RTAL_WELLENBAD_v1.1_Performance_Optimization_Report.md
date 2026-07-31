# RTAL WELLENBAD v1.1

## Performance Optimization & Stability Update

Version **1.1** represents the first major optimization release of the
RTAL WELLENBAD wavetable synthesizer. Rather than introducing new
synthesis features, this release focused entirely on improving the
real-time performance, execution determinism, memory management, and
long-term reliability of the audio engine.

> **Increase DSP efficiency while preserving identical sound quality and
> functionality.**

Every optimization was implemented individually, benchmarked under
identical conditions, and only accepted if measurable improvements could
be verified.

------------------------------------------------------------------------

# Optimization Methodology

Every optimization was developed as an independent firmware version and
benchmarked under identical conditions using the integrated RTAL
Performance Profiler.

Benchmark scenarios included:

-   Idle
-   3 Voices
-   5 Voices
-   6 Voices (maximum practical polyphony)

Measured metrics:

-   DSP load
-   Voice processing load
-   Average / Peak ISR execution time
-   Audio overruns
-   Missed audio blocks

Only optimizations that produced measurable improvements became part of
Version 1.1.

------------------------------------------------------------------------

# Runtime Voice Cache

Frequently used synthesis parameters are now cached instead of being
recalculated every audio block.

Cached values include:

-   Pitch increment
-   Velocity gain
-   Pan gains
-   Filter base frequency
-   Key tracking
-   Velocity tracking

This significantly reduces floating-point calculations inside the
real-time audio loop.

------------------------------------------------------------------------

# Snapshot-Based Parameter Evaluation

Global parameters are evaluated once at the beginning of each audio
block.

Examples:

-   Pitch Bend
-   Global Aftertouch
-   Filter Modulation
-   Master Volume
-   MIDI Controller values

All voices share these cached values during block processing.

------------------------------------------------------------------------

# Reduced Audio ISR Load

Several expensive calculations were moved outside the per-sample
processing loop.

This reduced average interrupt execution time, timing jitter and the
number of audio overruns.

------------------------------------------------------------------------

# Memory Locality Improvements

Frequently accessed voice parameters were reorganized to improve cache
locality.

Although the performance gain on the ESP32-S3 is modest, the code base
is cleaner and more scalable.

------------------------------------------------------------------------

# RTAL Performance Profiler

Version 1.1 introduced a dedicated real-time profiler measuring:

-   DSP load
-   Voice processing
-   MIDI
-   User Interface
-   SD Card access
-   Effects
-   Average / Peak ISR
-   Audio overruns

The profiler can now be completely disabled:

``` cpp
#define RTAL_PROFILER_ENABLED 0
```

When disabled, the compiler removes the profiling code, resulting in
virtually zero runtime overhead.

------------------------------------------------------------------------

# Audio Engine Improvements

Version 1.1 provides:

-   Lower DSP load
-   Reduced ISR execution time
-   Fewer audio overruns
-   Improved timing determinism
-   Additional CPU headroom

while preserving identical sound quality.

------------------------------------------------------------------------

# SD Card Reliability

## Safe File Replacement

Preset and Bank files are now deleted before being rewritten, preventing
file corruption caused by append mode.

## Improved Bank Loading

Large temporary bank buffers are allocated dynamically from PSRAM (with
heap fallback) instead of the task stack, preventing stack overflow.

## RTAL Storage Layout

The SD card structure has been fully migrated to the RTAL namespace.

    /RTALWT
    /RTALPRESETS
    /RTALBACKUP
    /RTALMETA

Preset files now use the `.RTAL` extension.

------------------------------------------------------------------------

# Internal Cleanup

Legacy configuration blocks and duplicate definitions were removed.

All storage format versions and SD directory definitions are now
maintained from a single configuration location.

------------------------------------------------------------------------

# Measured Results

Compared with previous releases, Version 1.1 achieves:

-   Significantly lower average ISR execution time
-   Noticeably reduced DSP utilization
-   Dramatically fewer audio overruns
-   Higher processing headroom
-   Identical synthesis behaviour and sound quality

------------------------------------------------------------------------

# Conclusion

RTAL WELLENBAD Version 1.1 is primarily an engineering release.

The firmware architecture has been substantially optimized and provides
a stable, deterministic and maintainable foundation for future
development.

------------------------------------------------------------------------

## Developed by

**RealTimeAudioLab (RTAL)**

Designed and optimized for deterministic real-time audio processing on
the ESP32-S3 platform.
