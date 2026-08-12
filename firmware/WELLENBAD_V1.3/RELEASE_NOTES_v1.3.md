# RTAL WELLENBAD v1.3 - Release Notes

RTAL WELLENBAD v1.3 is the stable release of the new four-part architecture developed throughout the v1.3 series.

## Highlights

### Four-part multitimbral synthesis

Four independent Parts share an eight-voice pool. Each Part can use its own preset, MIDI channel, volume, transpose, mute/enable state and voice reserve. Assigning the same MIDI channel to multiple Parts creates layers.

### Per-part ARP and modulation sequencer

Each Part has its own arpeggiator and 16-step modulation sequencer while sharing one master clock. ARP and SEQ can run from the internal clock or external MIDI clock.

### Multi storage

Multi setups save all four Part programs and Part settings. The Multi format also stores clock source and internal tempo. Existing earlier Multi files remain compatible through the versioned loader.

### SD wavetable support

Each Part has its own SD wavetable cache, allowing multiple different SD wavetables to be used simultaneously without SD access from the AudioTask.

### MIDI realtime stability

External MIDI clock handling was prioritized and continuous CC processing was decoupled from the high-priority MIDI realtime path. This significantly improves external sync under dense controller traffic.

### Performance configuration

The release uses:

- 8 voices
- GCC `O3` and `fast-math`
- internal chorus enabled
- Fast Poly disabled
- no automatic chorus disabling at high polyphony
- filter modulation updated at 8 kHz while the actual filter remains sample-accurate at 32 kHz

## Diagnostics

The lightweight System diagnostics remain available and include audio DSP/budget, DSP Max, overruns, MIDI queue/clock information and storage/cache status. The heavy development profiler is disabled in the release build.

## Known realtime limitation

Hardware testing showed that very CPU-intensive sounds can exceed the 128-sample audio-block deadline when 6-8 voices are continuously active. With eight voices and internal chorus enabled, occasional overruns may therefore be audible on the most demanding patches. This behavior is accepted for v1.3.

## Final hardware regression status

The release candidate with internal chorus enabled passed the final functional hardware regression performed before v1.3 release.
