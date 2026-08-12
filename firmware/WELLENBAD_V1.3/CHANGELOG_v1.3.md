# Changelog - RTAL WELLENBAD v1.3

## Final release

v1.3 consolidates the v1.3 development series into the first stable 4-part multitimbral release.

### Major additions

- 4-part multitimbral architecture with dynamic shared voice allocation
- 8 voices official
- Per-part MIDI channels and same-channel layering
- Per-part sound programs and DSP parameters
- Per-part arpeggiator
- Per-part modulation sequencer with 16-step editor/show view
- Multi save/load
- Per-Multi INT/MIDI clock source and internal tempo
- Four-part SD wavetable cache
- Last-state restore for single presets and Multis
- MIDI Clock priority path with deferred CC processing
- Multi-aware preset browser and editor context
- Preset/ARP/SEQ save consistency fixes
- O3 + fast-math compiler optimization
- Filter-modulation control-rate optimization
- System diagnostics for audio, MIDI and storage

### Final configuration

```cpp
#define NUM_VOICES 8
#define RTAL_ENABLE_FAST_POLY 0
#define RTAL_DISABLE_CHORUS_HIGH_POLY 0
#define RTAL_INTERNAL_CHORUS 1
#define RTAL_FILTER_CONTROL_DIV 4
#define RTAL_PROFILER_ENABLED 0
```

### Known limitation

At high continuous polyphony, particularly 6-8 complex voices with internal chorus enabled, occasional audio deadline overruns can occur. This is accepted for v1.3.
