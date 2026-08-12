# RTAL WELLENBAD v1.3

**ESP32-S3 Wavetable Synthesizer · 8 Voices · 4-Part Multitimbral · ARP · Modulation Sequencer · SD Wavetables · MIDI · OLED**

> **RTAL — Real Time Audio Lab**  
> A compact digital synthesizer built around an ESP32-S3, with the aim of creating a capable, musical wavetable instrument using a small number of inexpensive components.

<!-- Replace with your own project photo -->
<!-- ![RTAL WELLENBAD](images/wellenbad_v13.jpg) -->

---

## About WELLENBAD

**WELLENBAD** is an embedded wavetable synthesizer developed as part of **RTAL — Real Time Audio Lab**.

The project explores how far a modern microcontroller can be pushed as a complete real-time musical instrument: wavetable oscillators, filters, envelopes, modulation, MIDI, arpeggiators, sequencing, preset management, SD-card storage and a dedicated hardware user interface all run on a single **ESP32-S3**.

The design philosophy is deliberately practical:

- high musical value with modest hardware
- deterministic real-time audio processing
- direct hardware control instead of a computer-dependent workflow
- open, understandable firmware architecture
- gradual evolution through real hardware testing

Version **1.3** is a major milestone. WELLENBAD has evolved from a polyphonic wavetable synthesizer into a **four-part multitimbral instrument** with a dynamically shared eight-voice engine.

---

## v1.3 Highlights

- **8-voice polyphony**
- **4-part multitimbral architecture**
- dynamic shared voice allocation across all four Parts
- independent preset per Part
- independent MIDI channel per Part
- same-channel Part layering
- Part volume, transpose, mute, enable and voice reserve
- **four independent arpeggiators**
- **four independent 16-step modulation sequencers**
- shared internal or external MIDI master clock
- Multi setup save/load
- per-Multi clock source and tempo storage
- per-Part SD wavetable cache
- internal stereo chorus
- preset Morph / Compare / Randomize functions
- MIDI CC control and CC Learn
- last-state restore
- lightweight audio, MIDI and storage diagnostics

---

## Architecture

WELLENBAD separates time-critical audio processing from MIDI and user-interface work across the ESP32-S3 cores.

```text
                         ┌──────────────────────────────┐
 MIDI IN ───────────────►│ MIDI / Control              │
 Buttons / Encoder ─────►│ UI / Presets / SD           │
                         │          Core 0              │
                         └──────────────┬───────────────┘
                                        │
                                        ▼
                         ┌──────────────────────────────┐
                         │ 8-Voice Synth Engine         │
                         │ OSC / WT / ENV / FILTER      │
                         │ MOD / PAN / MIX / CHORUS     │
                         │          Core 1              │
                         └──────────────┬───────────────┘
                                        │ I2S
                                        ▼
                                  Stereo DAC
```

The four Parts share one pool of eight voices. This allows the available polyphony to be used where it is actually needed instead of reserving a fixed number of voices for each Part.

```text
Part 1 ─┐
Part 2 ─┼──► Dynamic 8-Voice Pool ─► Mix ─► Chorus ─► I2S
Part 3 ─┤
Part 4 ─┘
```

---

## Four-Part Multitimbral Mode

Each Part can be configured independently with:

- Preset
- MIDI channel
- Volume
- Transpose
- Mute
- Enable
- Voice Reserve
- Hold
- Arpeggiator Gate

Multiple Parts may listen to the **same MIDI channel**, making layered sounds possible without any special layer mode.

A Multi setup stores the complete four-Part configuration together with its clock source and internal tempo.

WELLENBAD v1.3 provides **32 Multi slots** (`M00`–`M31`).

---

## Wavetable Synthesis

The synthesis engine is based on 256-sample wavetables and provides a large internal wavetable collection together with optional SD-card wavetables.

Main synthesis sections include:

- dual wavetable oscillator architecture
- wavetable position / morphing
- oscillator mixing
- pitch and detune control
- amplitude envelope
- filter envelope
- wave envelope
- LFO modulation
- velocity and aftertouch modulation
- stereo pan / spread
- resonant filter processing
- internal stereo chorus

The firmware exposes **127 selectable wavetable slots**. SD wavetables can replace slots from the upper end of this range without performing SD-card access inside the real-time AudioTask.

---

## SD Wavetable Cache

v1.3 introduces a dedicated wavetable cache for each of the four Parts.

```text
SD Card
  │
  ├──► Part 1 WT Cache
  ├──► Part 2 WT Cache
  ├──► Part 3 WT Cache
  └──► Part 4 WT Cache
          │
          ▼
      Audio Engine
```

This makes it possible for different Parts to use different SD-based wavetables simultaneously while keeping blocking SD I/O out of the audio path.

Up to **16 SD wavetable files** are indexed by the current firmware.

---

## Arpeggiators

Every Part has its own arpeggiator.

Available parameters include:

- Mode
- musical Rate division
- Octave range
- Hold
- Gate

All four arpeggiators share the global WELLENBAD master clock and can therefore remain synchronized while running independently.

Clock source:

- **INT** — internal tempo
- **MIDI** — external MIDI Clock, 24 PPQN

External MIDI Clock handling has a dedicated high-priority path. Continuous controller traffic is deferred so that dense MIDI CC streams do not unnecessarily disturb realtime clock processing.

---

## Modulation Sequencers

Each Part also contains an independent **16-step modulation sequencer**.

Per-Part sequencer parameters include:

- Mode
- Rate
- number of Steps
- modulation Target
- Depth
- Table Mode
- 16 editable step values

The `SEQ SHOW` view provides live visual feedback of the currently running step.

ARP and SEQ share the same master timing system, allowing rhythmic modulation and note generation to remain locked together.

---

## Presets and Performance Workflow

WELLENBAD supports factory and user programs together with SD-card storage.

The v1.3 preset system includes:

- Factory presets
- User presets
- SD preset storage
- Multi setups
- Morph A/B
- Randomize
- Compare
- MIDI CC Learn
- last-state restore

The firmware keeps Single Performance and Multi operation clearly separated: loading a normal Factory/User preset from the Performance browser returns WELLENBAD to Single mode, while loading a preset from within the Multi Part editor changes only the selected Part.

An optional **U100–U119 ARP/SEQ showcase bank** is included with the release package under:

```text
Extras/Presets/
```

---

## MIDI

WELLENBAD provides hardware MIDI IN and OUT.

Current v1.3 functionality includes:

- Note On / Note Off
- per-Part MIDI channels
- same-channel layering
- Pitch Bend
- Mod Wheel
- Volume / Expression
- Sustain
- Channel Aftertouch
- Program Change
- MIDI CC control
- BANKED CC access
- CC Learn
- MIDI Clock
- Start / Stop / Continue
- All Notes Off / All Sound Off handling

The master tempo display automatically follows the measured external MIDI Clock when MIDI sync is selected.

---

## User Interface

The synthesizer is operated from a compact **128 × 64 SSD1309 OLED**, eight hardware buttons and one rotary encoder with push switch.

The UI provides dedicated pages for:

- Oscillators
- Filter
- Amp Envelope
- Filter Envelope
- Wave Envelope
- LFO
- Performance
- FX
- ARP
- SEQ
- Morph
- MIDI
- Wave Monitor
- Program management
- Multi / Part editing
- System diagnostics

The live Wave Monitor follows the active sound and provides visual feedback directly on the hardware.

---

## Hardware

### Main platform

- **ESP32-S3**
- 16 MB Flash
- 8 MB PSRAM
- SSD1309 128 × 64 SPI OLED
- I2S stereo DAC
- hardware MIDI IN / OUT
- SD card
- 8 push buttons
- rotary encoder + push switch

### Firmware audio format

| Parameter | v1.3 |
|---|---:|
| Sample rate | **44.1 kHz** |
| Audio block | **128 frames** |
| Polyphony | **8 voices** |
| Multitimbral Parts | **4** |
| Wavetable size | **256 samples** |
| Visible wavetable slots | **127** |
| Multi slots | **32** |

---

## Pin Assignment

The v1.3 firmware uses the following pin assignment:

| Function | GPIO |
|---|---:|
| MIDI RX | 40 |
| MIDI TX | 39 |
| I2S BCLK | 18 |
| I2S LRCK | 16 |
| I2S DATA OUT | 17 |
| OLED SCK | 12 |
| OLED MOSI | 11 |
| OLED CS | 10 |
| OLED DC | 6 |
| OLED RESET | 7 |
| Button 1 | 21 |
| Button 2 | 47 |
| Button 3 | 45 |
| Button 4 | 38 |
| Button 5 | 4 |
| Button 6 | 15 |
| Button 7 | 3 |
| Button 8 | 14 |
| Encoder A | 1 |
| Encoder B | 2 |
| Encoder switch | 42 |
| SD MISO | 13 |
| SD CS | 9 |

OLED and SD share the SPI clock/data lines used by the hardware design.

---

## SD Card Structure

The firmware uses the following directories:

```text
/RTALWT        SD wavetables
/RTALPRESETS   presets
/RTALBACKUP    backup data
/RTALMETA      metadata
/RTALMULTI     Multi setups
```

Multi files use a versioned file format. The v1.3 loader retains compatibility with the earlier Multi format while new saves use the current format.

---

## Build Environment

The v1.3 release was developed and hardware-tested with:

- **Arduino IDE 1.8.19**
- **ESP32 Arduino Core 2.0.16**
- ESP32-S3 target
- PSRAM enabled

Open:

```text
RTAL_WELLENBAD_v1.3.ino
```

in Arduino IDE. Keep all supplied `.ino`, `.h` and `.cpp` files together in the same sketch directory.

### Release performance configuration

```cpp
#define NUM_VOICES 8
#define RTAL_ENABLE_FAST_POLY 0
#define RTAL_DISABLE_CHORUS_HIGH_POLY 0
#define RTAL_INTERNAL_CHORUS 1
#define RTAL_FILTER_CONTROL_DIV 4
#define RTAL_PROFILER_ENABLED 0
```

The release is compiled with GCC `O3` and `fast-math` optimizations.

Filter cutoff modulation is updated at one quarter of the 44.1 kHz audio rate (**11.025 kHz**), while the actual filter processing remains at full audio sample rate.

---

## Diagnostics

The production firmware retains lightweight diagnostic pages for hardware testing and troubleshooting.

Available information includes:

- active voices
- DSP time / block budget
- maximum DSP time
- audio overruns
- MIDI queue statistics
- MIDI Clock jitter information
- ARP / SEQ activity
- SD status
- per-Part wavetable cache status
- wavetable load/failure counters
- minimum heap / PSRAM information

The heavy development profiler is disabled in the v1.3 release.

---

## Real-Time Performance Note

WELLENBAD v1.3 officially supports eight voices.

Hardware testing showed that very demanding patches with **6–8 continuously active voices**, especially with the internal chorus enabled, can occasionally exceed the 128-frame audio-block deadline. Five continuously active voices were clean in the final production benchmark; at higher sustained polyphony, occasional overruns may occur depending on the patch.

This behavior is a known and accepted limitation of v1.3 rather than a hidden release issue. Musical passages with fewer simultaneous voices — including typical arpeggiator operation — can remain comfortably within the real-time budget.

---

## Development History

WELLENBAD is developed iteratively on real hardware. The v1.3 series concentrated on moving from a single-performance synthesizer to a stable multitimbral architecture while preserving the sound engine and hardware workflow established in v1.2.

Major v1.3 development work included:

- four-Part program architecture
- shared voice ownership and allocation
- robust same-channel layering
- Part-aware MIDI controllers
- independent ARP engines
- independent modulation sequencers
- Multi V2 storage
- per-Part SD wavetable caching
- preset / ARP / SEQ context consistency
- external MIDI Clock stability improvements
- DSP profiling and production optimization
- eight-voice release validation

---

## What Comes Next — v1.4

The next development stage is planned around a **dual-ESP32-S3 architecture**.

WELLENBAD will remain the synthesizer and musical master, while a second ESP32-S3 will operate as a dedicated stereo FX processor. A working delay engine already exists on the FX platform.

Planned direction:

```text
WELLENBAD ESP32-S3
Synth / MIDI / UI / ARP / SEQ
          │
          │ digital audio + control
          ▼
Dedicated FX ESP32-S3
Delay / Chorus / Reverb / further FX
          │
          ▼
       Stereo DAC
```

This allows the main synthesizer to keep its available CPU time focused on voice generation while the FX processor can grow independently.

---

## Project Status

**RTAL WELLENBAD v1.3 — Stable Release**

The final v1.3 release candidate with internal chorus enabled passed the functional hardware regression before release.

---

## Gallery

<p align="center">
<img src="images/Wellenbad_Prototype_3.jpeg" width="900">

---

## RTAL — Real Time Audio Lab

WELLENBAD is part of **Real Time Audio Lab**, a collection of hardware and firmware projects exploring synthesizers, samplers, MIDI systems, digital audio processing and embedded real-time sound generation.

The project combines modern ESP32-S3 processing with the hands-on workflow and directness of dedicated electronic musical instruments.

---

*RTAL WELLENBAD v1.3 — August 2026*
