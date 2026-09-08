# RTAL WELLENBAD v1.5.0

**ESP32-S3 Wavetable Synthesizer · 7 Hardware-Validated Voices · 4-Part Multitimbral · Performance Mode · 4-Pole Filter · ARP · Modulation Sequencer · SD Wavetables · MIDI · USB-CDC Editor · OLED**

> **RTAL — Real Time Audio Lab**  
> A compact digital synthesizer built around the ESP32-S3, developed with a strong focus on musical usefulness, deterministic real-time audio and continuous validation on real hardware.

---

## About WELLENBAD

**RTAL WELLENBAD** is an embedded wavetable synthesizer developed as part of **RTAL — Real Time Audio Lab**.

The project explores how far a modern microcontroller can be pushed as a complete real-time musical instrument. Wavetable oscillators, filters, envelopes, modulation, MIDI, arpeggiators, sequencing, preset management, SD-card storage, multitimbral operation, diagnostics and a dedicated hardware user interface all run around a single ESP32-S3 synth engine.

The design philosophy remains deliberately practical:

- high musical value with modest hardware
- deterministic real-time audio processing
- direct hardware control without requiring a computer
- open and understandable firmware architecture
- careful separation of real-time and non-real-time work
- incremental development through real hardware testing
- no hidden “paper specifications”: release limits are based on what the hardware actually sustains

Version **1.5.0** is the largest WELLENBAD update since the published v1.3 release. It consolidates the four-Part multitimbral architecture, introduces a new 4-pole low-pass filter, expands Multi memory, adds a true SINGLE/MULTI performance workflow, extends per-Part routing and adds a USB-CDC remote editor designed not to compromise audio real-time performance.

Firmware identity:

```text
v1.5.0 B0072 A015 FINAL
```

---

# What Changed Since v1.3

The last public release was **WELLENBAD v1.3**. Version 1.5 keeps the core musical concept, but several important subsystems have been redesigned or significantly extended.

| Area | v1.3 | v1.5.0 |
|---|---|---|
| Official polyphony | 8 nominal voices, demanding patches could overrun | **7 hardware-validated voices** |
| Multitimbral Parts | 4 | **4** |
| Voice allocation | Shared dynamic pool | **Shared dynamic 7-voice pool** |
| Filter | Previous resonant filter implementation | **New 4-pole FAST32 biquad LPF** |
| Filter topology | Previous design | **Two cascaded 2-pole stages** |
| Filter cutoff range | parameter mapped | **20 Hz – 18 kHz, 128-point log coefficient table** |
| Filter character | resonance / drive | **Drive before filter + global resonance around the 4-pole structure** |
| Multi slots | 32 (`M00–M31`) | **128 (`M000–M127`)** |
| Multi Part zones | basic Part routing | **Key Low / Key High per Part** |
| Keytrack reference | global/default | **Root Note per Part** |
| Part positioning | existing stereo voice spread | **additional per-Part Pan** |
| Performance workflow | Single/Multi existed in development form | **explicit SINGLE / MULTI Performance Mode** |
| Program Change | program oriented | **mode-aware: Programs in SINGLE, Multis in MULTI** |
| User preset FX | separate / previous behavior | **persistent shared FX state in current user preset format** |
| Multi FX | not part of early Multi format | **one shared persistent FX scene per Multi** |
| Remote editor | not part of published v1.3 | **native USB-CDC editor** |
| Remote real-time protection | — | **CDC SMART SAFE** |
| OLED mirror | hardware only | **browser mirror when real-time reserve permits** |
| Release philosophy | 8 voices with documented edge cases | **7 voices chosen for reliable real-hardware operation** |

The change from 8 nominal voices in v1.3 to **7 official voices in v1.5 is intentional**.

v1.3 already documented that very demanding patches at high sustained polyphony could exceed the 128-frame audio deadline. During the v1.5 development cycle the audio engine, filter, Multi path and editor interaction were extensively tested. The final release therefore publishes the polyphony level that proved reliable in both SINGLE and MULTI operation on the target hardware.

---

# v1.5.0 Highlights

- **7 hardware-validated voices**
- **44.1 kHz** internal audio
- **128-frame** audio blocks
- **4-Part multitimbral** architecture
- dynamic voice sharing across all four Parts
- explicit **SINGLE / MULTI Performance Mode**
- **128 Multi locations** (`M000–M127`)
- protected Factory Programs `F000–F029`
- User Programs `U030–U127`
- new **4-pole FAST32 low-pass filter**
- two cascaded 2-pole biquad stages
- 20 Hz–18 kHz logarithmic cutoff coefficient table
- filter Drive
- global resonance
- independent Program per Multi Part
- independent MIDI channel per Part
- same-channel Part layering
- per-Part volume
- per-Part transpose
- per-Part mute / enable
- per-Part voice reserve
- per-Part ARP gate
- per-Part key zone
- per-Part filter-keytrack root
- per-Part pan
- four independent arpeggiators
- four independent 16-step modulation sequencers
- shared internal / external MIDI master clock
- Multi save / load
- persistent shared FX scene per Multi
- mode-aware MIDI Program Change
- USB-CDC Remote Editor
- dedicated **RTAL AudioDSP FX Module v1.0.0 / FX0047 FINAL** companion processor
- **CDC SMART SAFE** real-time protection
- SD wavetable support
- Factory/User preset management
- MIDI CC control and CC Learn
- lightweight real-time diagnostics
- Fast Poly remains disabled in the production release

---

# Architecture

WELLENBAD keeps time-critical synthesis work isolated from user-interface, MIDI, SD and editor work as far as possible.

```text
                       ┌──────────────────────────────────┐
MIDI IN ──────────────►│ MIDI / Control / ARP / SEQ       │
Buttons / Encoder ────►│ UI / Presets / SD / USB Remote  │
USB-CDC Editor ───────►│              Core 0             │
                       └───────────────┬──────────────────┘
                                       │
                                       ▼
                       ┌──────────────────────────────────┐
                       │ Shared 7-Voice Synth Engine      │
                       │                                  │
                       │ WT OSC → ENV → 4-Pole LPF        │
                       │        → PAN / MIX / Audio       │
                       │              Core 1              │
                       └───────────────┬──────────────────┘
                                       │ I2S
                                       ▼
                                  Stereo DAC
```

In MULTI mode:

```text
Part 1 ─┐
Part 2 ─┼──► Dynamic 7-Voice Pool ─► Stereo Mix ─► Output
Part 3 ─┤
Part 4 ─┘
```

The Parts do **not** own fixed groups of voices. All Parts share the same voice pool, allowing polyphony to be used where it is musically needed.

---

# Audio Engine

## Core audio format

| Parameter | v1.5.0 |
|---|---:|
| Sample rate | **44.1 kHz** |
| Audio block | **128 frames** |
| Block deadline | approx. **2.902 ms** |
| Official polyphony | **7 voices** |
| Multitimbral Parts | **4** |
| Wavetable size | **256 samples** |
| Visible wavetable slots | **127** |
| Multi slots | **128** |
| Filter control divider | **4** |
| Filter modulation update rate | **11.025 kHz** |
| Filter audio processing | **44.1 kHz** |

The audio task runs on the dedicated real-time core at high priority.

The heavy development profiler is disabled in the production build.

---

# New 4-Pole FAST32 Low-Pass Filter

One of the most important changes in v1.5 is the replacement of the previous WELLENBAD filter path with a new resource-conscious **4-pole biquad low-pass filter**.

## Topology

The filter consists of two cascaded 2-pole low-pass stages:

```text
Input
  │
  ▼
Drive
  │
  ▼
2-Pole LPF Stage 1
  │
  ▼
2-Pole LPF Stage 2
  │
  ▼
Output
  ▲
  │
Global Resonance Feedback
```

The two stages use different Q values:

```text
Stage 1 Q ≈ 0.541196
Stage 2 Q ≈ 1.306563
```

Together they form the 4-pole response.

## Coefficient strategy

The filter is designed specifically for the ESP32-S3 real-time workload:

- coefficients are precomputed
- **128 logarithmically spaced cutoff positions**
- cutoff range approximately **20 Hz to 18 kHz**
- float32 processing uses the ESP32-S3 FPU
- no `sin()`, `cos()`, `tan()`, `exp()`, `pow()` or `tanh()` calls in the audio render path
- cutoff / envelope control work is updated every 4 audio samples
- actual filtering still runs at the full 44.1 kHz sample rate
- numerator symmetry is used to reduce operations in the biquad implementation
- zero / inactive modulation paths are skipped where possible

The result is a considerably more substantial 4-pole filter character while retaining enough real-time headroom for the final seven-voice configuration.

## Drive and resonance

Drive is applied before the filter stages.

Resonance acts around the complete filter structure rather than as two unrelated resonance controls. This makes the two cascaded stages behave as one musical 4-pole filter rather than simply two independent low-pass filters placed in series.

---

# Wavetable Synthesis

The fundamental WELLENBAD synthesis architecture remains wavetable-based.

Main synthesis functions include:

- dual wavetable oscillator architecture
- wavetable selection
- wavetable position / morphing
- oscillator mix
- oscillator detune
- oscillator B offset
- pitch bend
- sub oscillator
- noise
- amplitude envelope
- filter envelope
- wave envelope
- main LFO
- wave LFO
- velocity modulation
- channel-aftertouch modulation
- filter key tracking
- stereo pan / spread
- Drive
- Bitcrush
- monophonic / polyphonic performance parameters
- glide
- unison detune

WELLENBAD exposes 127 wavetable positions and supports SD-based wavetable content.

Blocking SD access is kept out of the sample render path.

---

# SINGLE and MULTI Performance Modes

v1.5 introduces an explicit Performance Mode.

```text
SINGLE
MULTI
```

The currently selected mode determines how WELLENBAD interprets performance actions such as Program Change and preset selection.

## Hardware shortcuts

| Control | Action |
|---|---|
| T7 / PLAY short | PLAY |
| T7 / PLAY long | **toggle SINGLE / MULTI** |
| T8 / BACK short | BACK |
| T8 / BACK long | **HOME** |

Changing the selected Multi Part does **not** silently change Performance Mode.

---

# SINGLE Mode

SINGLE mode behaves as the normal one-program WELLENBAD performance mode.

A Program controls the complete seven-voice synthesizer engine.

MIDI Program Change selects Programs:

```text
PC 0–29   → Factory Programs F000–F029
PC 30–127 → User Programs U030–U127
```

Factory Programs are protected.

User Programs are stored on SD.

---

# Four-Part MULTI Mode

MULTI mode provides four independent Parts sharing the seven-voice engine.

Each Part contains a complete Program and can be configured with:

- Program
- MIDI channel
- Volume
- Transpose
- Voice Reserve
- ARP Gate
- Key Low
- Key High
- Root Note
- Pan
- Enable
- Mute

## Key zones

v1.5 adds real per-Part MIDI note zones:

```text
KEY LOW
KEY HIGH
```

This makes keyboard splits and restricted layers possible without external MIDI processing.

Example:

```text
Part 1: C1–B2   Bass
Part 2: C3–B4   Pad
Part 3: C5–C8   Lead
Part 4: C1–C8   Layer / texture
```

## Root Note

Each Part also stores a `Root Note`.

This is the zero-reference for filter key tracking, allowing the keytracking response to be musically centered differently for individual Parts.

## Per-Part Pan

A Multi Part can add its own pan offset in addition to the existing voice stereo spread.

This allows a four-Part setup to be spatially arranged as part of the saved performance.

## Same-channel layering

Multiple enabled Parts may listen to the same MIDI channel.

This allows layered performances without a separate layer mode:

```text
Part 1 → MIDI CH 1
Part 2 → MIDI CH 1
Part 3 → MIDI CH 3
Part 4 → MIDI CH 4
```

Parts 1 and 2 will sound together when channel 1 is played.

---

# Multi Memory

v1.3 provided 32 Multi slots.

v1.5 expands this to:

```text
M000 – M127
```

for a total of **128 Multi setups**.

A current Multi stores:

- Multi name
- all four embedded Programs
- Part Program references
- Part MIDI channels
- Part volume
- Part transpose
- Part voice reserve
- Part ARP gate
- Part key zones
- Part Root Note
- Part Pan
- Part enable / mute state
- master clock source
- internal tempo
- one shared persistent FX scene

Embedding the Program data means a saved Multi can restore the sound that belonged to the performance even if an individual Program location is changed later.

## Multi file compatibility

The Multi storage format evolved through several versions.

The v1.5 loader retains support for earlier Multi formats. Older formats receive neutral defaults for fields that did not yet exist, such as later key-zone / Root / Pan or FX data.

Current saves use the latest Multi format.

Legacy v1.3 Multi data therefore remains useful rather than being discarded by the new firmware generation.

---

# Preset Manager

v1.5 formalizes the Program memory into two areas.

## Factory Programs

```text
F000 – F029
```

30 Factory / showcase locations.

Factory Programs are protected against normal User save operations.

## User Programs

```text
U030 – U127
```

98 User locations stored on SD.

User Programs can contain the complete current Program data and, in the current file format, persistent FX state.

The Program browser separates Factory categories and the User area.

Current categories include:

- ALL
- PAD
- LEAD
- BASS
- SEQ
- FX
- ORGAN
- MISC
- USER

---


# RTAL AudioDSP FX Module v1.0.0

WELLENBAD v1.5.0 is designed to operate together with a dedicated second ESP32-S3 running the **RTAL AudioDSP FX Module v1.0.0 / FX0047 FINAL**.

This is the officially recommended effects processor for the v1.5 release.

```text
RTAL WELLENBAD
v1.5.0 B0072 A015 FINAL

        +

RTAL AudioDSP FX Module
v1.0.0 FX0047 FINAL
```

FX0047 is a release freeze of the proven:

```text
RTAL Audio DSP Platform
0.1.0ai Build0046l StableSyncDelay
```

No effect algorithm, routing topology, I2S format, block size or WELLENBAD FX-Link protocol was changed when the stable 0046l development state was promoted to FX v1.0.0.

## Why a second ESP32-S3?

The WELLENBAD main processor already performs:

- seven polyphonic synth voices
- two wavetable oscillators per voice
- envelopes and modulation
- the 4-pole FAST32 filter
- MIDI
- four arpeggiators
- four modulation sequencers
- Multi voice allocation
- OLED / UI
- SD storage
- USB-CDC editor communication

Instead of spending the remaining real-time budget on a large stereo effects chain, WELLENBAD sends its completed stereo synthesizer signal to a second ESP32-S3.

The FX processor can therefore dedicate its own full real-time budget to delay, modulation effects, reverb and final stereo processing.

This architecture is a central part of the current WELLENBAD platform rather than an optional future concept.

## Complete two-processor audio architecture

```text
                         RTAL WELLENBAD v1.5.0
                         ESP32-S3 MAIN
                               |
             7-Voice Wavetable Synthesizer
                               |
                         Stereo Synth Mix
                               |
                               | I2S
                               | 44.1 kHz / stereo
                               | BCLK  GPIO18
                               | LRCK  GPIO16
                               | DATA  GPIO17
                               v
                   RTAL AudioDSP FX v1.0.0
                         ESP32-S3 FX
                               |
                               | DATA IN GPIO5
                               v
                         Stereo Delay
                               |
                               v
                MOD FX: one active processor
                  Chorus / Flanger / Phaser
                               |
                               v
                         Stereo Reverb
                               |
                               v
                         Stereo Width
                               |
                               v
                         Output Limiter
                               |
                               | I2S
                               | BCLK GPIO18
                               | LRCK GPIO16
                               | DATA OUT GPIO17
                               v
                           PCM5102A
                               |
                               v
                        Stereo Line Out
```

The final FX processing order in FX0047 is:

```text
Delay -> MOD FX -> Reverb -> Width -> Limiter
```

Only one processor from the shared modulation-effects group is active at a time:

```text
CHORUS
FLANGER
PHASER
```

This keeps the processing model predictable and preserves real-time headroom.

## FX audio format

| Parameter | FX v1.0.0 |
|---|---:|
| Processor | ESP32-S3 |
| Sample rate | **44.1 kHz** |
| Block size | **128 frames** |
| Channels | **Stereo** |
| Input I2S port | Port 0 |
| Output I2S port | Port 1 |
| DMA buffers | 8 |
| DMA frames | 128 |
| Final DAC | PCM5102A |

The FX processor follows the same fundamental real-time block budget as WELLENBAD:

```text
128 / 44100 ~= 2.902 ms
```

Every complete FX block must finish inside this time window.

## I2S audio connection: WELLENBAD -> FX

WELLENBAD is the I2S clock master for the digital audio stream entering the FX processor.

| Signal | WELLENBAD Main | FX Module |
|---|---:|---:|
| BCLK | GPIO18 | GPIO18 |
| LRCK / WS | GPIO16 | GPIO16 |
| Stereo DATA | GPIO17 | GPIO5 |
| Ground | GND | GND |

```text
WELLENBAD GPIO18 BCLK  ----> FX GPIO18 BCLK
WELLENBAD GPIO16 LRCK  ----> FX GPIO16 LRCK
WELLENBAD GPIO17 DATA  ----> FX GPIO5 DATA IN
WELLENBAD GND          ----> FX GND
```

## I2S audio connection: FX -> PCM5102A

The FX processor creates the final stereo output stream for the DAC.

| Signal | FX Module | PCM5102A |
|---|---:|---|
| BCLK | GPIO18 | BCK |
| LRCK / WS | GPIO16 | LRCK |
| DATA OUT | GPIO17 | DIN |
| Ground | GND | GND |

```text
FX GPIO18 BCLK      ----> PCM5102A BCK
FX GPIO16 LRCK      ----> PCM5102A LRCK
FX GPIO17 DATA OUT  ----> PCM5102A DIN
FX GND              ----> PCM5102A GND
```

The **final analog audio output is therefore taken from the PCM5102A connected to the FX processor**, not directly from the WELLENBAD main ESP32-S3.

## Bidirectional WELLENBAD FX-Link

Audio and control use separate connections.

The FX-Link is a dedicated bidirectional UART connection between the two ESP32-S3 processors.

| Direction | WELLENBAD Main | FX Module |
|---|---:|---:|
| WELLENBAD -> FX | GPIO5 TX | GPIO40 RX |
| FX -> WELLENBAD | GPIO8 RX | GPIO39 TX |
| Ground | GND | GND |

Protocol:

```text
115200 baud
8N1
RTAL WELLENBAD FX Protocol v1
fixed 11-byte frames
CRC8 protected
```

The control link supports:

- HELLO / ACK connection qualification
- PING / PONG supervision
- GET_STATE / STATE synchronization
- parameter changes
- effect enable / disable state
- preset state
- effective tempo transfer
- INT / MIDI clock-source metadata
- connection diagnostics

The audio stream remains independent from the UART control link.

## Stereo Delay

The FX module contains a full stereo delay engine with:

- independent Left and Right delay times
- up to approximately 1000 ms
- feedback
- wet level
- feedback low-pass filter
- feedback high-pass filter
- feedback saturation
- crossfeed / ping-pong behavior
- ducking
- Freeze
- FREE timing mode
- clock-synchronized timing
- RAM presets
- NVS presets
- smooth preset transitions
- Stable Sync Delay

### Stable Sync Delay

The final 0046l/FX0047 delay synchronization was developed specifically to avoid the pitch sweep / flanging effect that can occur when a clock-synchronized delay continuously moves one read pointer while tempo changes.

FX0047 uses a more robust strategy:

1. incoming effective MIDI tempo is observed over multiple timing windows
2. a new tempo must remain within the confirmation tolerance before it is accepted
3. small MIDI-clock jitter therefore does not continuously retune the delay
4. when a meaningful synchronized delay-time change is required, old and new fixed delay taps exist simultaneously
5. the processor crossfades between them over approximately **24 ms**
6. the old tap is then discarded

Conceptually:

```text
OLD synchronized tap ----\
                          >---- 24 ms crossfade ----> output
NEW synchronized tap ----/
```

This avoids the Doppler-like pitch sweep produced by continuously sliding one delay tap.

FREE/manual delay time retains its normal smoothing behavior.

## Chorus

The stereo Chorus uses the shared RTAL modulation core.

Important characteristics include:

- stereo modulation
- two taps per channel
- hybrid Hermite / linear interpolation
- delay-depth modulation
- tone control
- parameter smoothing
- stereo phase relationship
- real-time optimized render path

## Flanger

The stereo Flanger includes:

- short variable delay
- stereo modulation
- feedback
- feedback high-pass filter
- saturation
- shared modulation core
- optimized real-time processing

The feedback HPF reduces excessive low-frequency build-up while the saturation stage controls high-feedback behavior.

## Phaser

The stereo Phaser uses optimized **2-stage and 4-stage** processing modes.

Features include:

- stereo allpass structure
- feedback
- feedback high-pass filter
- saturation
- center frequency
- shared modulation core
- control-rate coefficient generation
- full audio-rate allpass processing

Higher experimental stage counts were removed after real-hardware performance testing. The production implementation favors the 2/4-stage configurations that fit the real-time budget reliably.

## Stereo Reverb

The FX module contains a stereo FDN-style reverb with:

- Mix
- Size
- Decay
- Damping
- Predelay
- stereo output
- optimized memory placement

Hot reverb structures are placed where they produce the best real-time behavior, while larger delay-style storage can use PSRAM where appropriate.

## Stereo Width

The final stereo image can be adjusted using Mid/Side processing.

Range:

```text
0%   = mono
100% = normal stereo
200% = widened stereo
```

Stereo Width runs near the end of the FX chain, after Reverb and before the final Limiter.

## Output Limiter

A lightweight output limiter protects the final stereo stream from excessive peaks generated when multiple effects combine constructively.

The limiter is intended as output protection, not as a creative mastering compressor.

## FX presets and state

The FX platform contains preset/state infrastructure for:

- 8 RAM Delay presets
- 8 NVS Delay presets
- preset names and metadata
- Startup Preset
- smooth Program Change
- Compare
- Revert
- Commit
- remote state query from WELLENBAD

Transient runtime audio is intentionally not treated as preset data.

For example, the actual samples currently stored inside a Delay Freeze buffer are not serialized as part of an FX preset.

## WELLENBAD Program and Multi interaction

The WELLENBAD v1.5 architecture treats FX as a **shared stereo scene after the Part mix**.

This has important consequences:

### SINGLE

A current User Program can contain persistent shared FX state.

### MULTI

A Multi stores **one shared FX scene** for the complete four-Part performance.

The four Parts do not each run their own Delay / Reverb processor.

Instead:

```text
Part 1 --\
Part 2 ---\
Part 3 ----> shared stereo mix -> FX Module -> stereo output
Part 4 ---/
```

Loading a Program into one selected Multi Part does not unexpectedly replace the complete Multi FX scene.

This keeps the performance architecture deterministic.

## Editor integration

The WELLENBAD v1.5 USB-CDC Editor also acts as the user interface for the companion FX module.

The PC talks only to the WELLENBAD main processor through native USB CDC.

WELLENBAD then forwards relevant FX control information over the dedicated UART FX-Link.

```text
Browser Editor
      |
      | USB CDC
      v
WELLENBAD Main ESP32-S3
      |
      | FX-Link UART
      v
AudioDSP FX ESP32-S3
```

This means the FX module does not require a second PC editor connection during normal operation.

The WELLENBAD editor provides access to the current FX scene, including the Delay and modulation/reverb-related controls supported by the current protocol.

FX-Link health information is also returned to WELLENBAD and can be displayed by the editor.

## FX real-time optimization

The AudioDSP development line went through extensive timing analysis before the v1.0 release freeze.

Important production optimizations include:

- 44.1 kHz / 128-frame deterministic block processing
- single-pass input/output conversion architecture
- delay HPF/LPF derived coefficients removed from the per-sample hot path
- delay filter coefficients refreshed at most every 16 samples and only while their cutoff is changing
- reduced diagnostic overhead in the production profile
- optimized Chorus / Flanger / Phaser render paths
- control-rate Phaser coefficient generation
- optimized Reverb memory placement
- reduced diagnostic sampling
- block deadline counters retained
- I2S read/write and clock diagnostics retained
- watchdog and task-liveness monitoring retained

The goal follows the same RTAL rule used by the synthesizer:

> sound generation and audio processing always have priority over diagnostics and user-interface traffic.

## FX release identity

```text
Product : RTAL AudioDSP FX Module
Version : 1.0.0
Build   : FX0047
Status  : FINAL
Basis   : 0.1.0ai Build0046l StableSyncDelay
```

## Official compatibility

| WELLENBAD | AudioDSP FX | Status |
|---|---|---|
| **v1.5.0 B0072 A015 FINAL** | **v1.0.0 FX0047 FINAL** | **Recommended release pair** |
| v1.4 development lineage | Build0046l / FX0047 lineage | Development compatible |
| older WELLENBAD versions | not release-tested | Verify before use |

For the public v1.5 release, the intended configuration is therefore:

```text
WELLENBAD v1.5.0 B0072
+
AudioDSP FX v1.0.0 FX0047
```

---

# Arpeggiators

Every Multi Part has its own arpeggiator.

Available musical parameters include:

- Mode
- Rate division
- Octave range
- Hold
- Gate

All four arpeggiators share the WELLENBAD master clock while maintaining independent note operation.

Clock sources:

```text
INT  — internal tempo
MIDI — external MIDI Clock, 24 PPQN
```

External MIDI Clock may remain continuously present. Transport is controlled with:

```text
FA — Start
FB — Continue
FC — Stop
```

The firmware keeps timing-sensitive MIDI Clock handling separated from lower-priority controller work.

---

# 16-Step Modulation Sequencers

Each Part has an independent modulation-sequencer runtime.

Per-Part sequencer functions include:

- Mode
- Rate
- number of Steps
- modulation Target
- Depth
- Table Mode
- 16 editable step values
- live current-step indication

ARP and SEQ use the same musical timing infrastructure, allowing generated notes and parameter modulation to stay synchronized.

The Remote Editor includes a graphical sequencer interface.

---

# MIDI

WELLENBAD provides hardware MIDI IN and OUT.

Current v1.5 functionality includes:

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
- Start
- Stop
- Continue
- All Notes Off
- All Sound Off handling

## Mode-aware Program Change

A major v1.5 workflow improvement is that Program Change follows Performance Mode.

### SINGLE mode

```text
PC 0–127 → Program F/U 000–127
```

### MULTI mode

```text
PC 0–127 → Multi M000–M127
```

Multi changes use a protected transition so voices are silenced safely during the setup change.

---

# USB-CDC Remote Editor

v1.5 introduces an extensive browser-based Remote Editor using the ESP32-S3 native USB-CDC interface.

Official editor:

```text
editor/RTAL_WELLENBAD_Editor_v1.5.0.html
```

The editor provides remote access to major WELLENBAD functions, including:

- oscillator parameters
- filter
- envelopes
- modulation
- performance parameters
- ARP
- graphical SEQ
- clock / system functions
- preset manager
- Multi selection and Part editing
- FX controls
- hardware-button / encoder remote control
- OLED mirror
- link health
- DSP / overrun diagnostics

No network connection is required for the normal USB editor workflow.

---

# CDC SMART SAFE

A conventional “always stream everything” editor caused an important real-time problem during v1.5 development.

Large or frequent USB-CDC transfers could occasionally interfere with the very small 2.902 ms audio-block budget, especially at maximum polyphony.

v1.5 therefore introduces **CDC SMART SAFE**.

The principle is simple:

> Audio timing has priority over editor refresh rate.

When the synthesizer is idle and sufficient real-time reserve exists, the editor receives normal state updates and OLED frames.

While audio is active:

- large OLED mirror transfers are suppressed
- full parameter/state dumps are suppressed
- large Multi / FX / sequencer snapshots are suppressed
- incoming Editor → WELLENBAD control remains active
- only a very small live telemetry packet may be transmitted when enough DSP reserve remains
- if the DSP load becomes too high, even that small telemetry transfer is skipped

After a period of audio inactivity, the full editor state is synchronized again.

This allows the editor to remain useful without allowing screen refresh traffic to take priority over sound generation.

## OLED mirror behavior

Because full OLED frames are deliberately paused during critical real-time operation, the mirrored display may temporarily show the last safe frame while notes are sounding.

The editor intentionally does **not** add a synthetic `V0…V7` voice overlay. This keeps the mirror visually faithful and avoids confusing local graphics that are not part of the physical OLED.

The physical WELLENBAD OLED continues to operate normally.

---

# User Interface

The hardware interface uses:

- SSD1309 128 × 64 OLED
- eight push buttons
- rotary encoder
- encoder push switch

Main pages include:

- OSC
- FILTER
- AMP ENV
- FILTER ENV
- WAVE ENV
- LFO
- PERFORM
- FX
- ARP
- SEQ
- MORPH
- MIDI
- WAVE MON
- PROGRAM
- MULTI / Part editing
- diagnostics / system functions

The firmware also contains the RTAL widget-based UI engine with legacy fallback support.

---

# Hardware

## Main platform

- ESP32-S3
- 16 MB Flash
- 8 MB PSRAM
- SSD1309 128 × 64 SPI OLED
- I2S stereo DAC
- hardware MIDI IN / OUT
- SD card
- 8 push buttons
- rotary encoder + push switch
- native USB connection for the v1.5 Remote Editor
- second ESP32-S3 running RTAL AudioDSP FX v1.0.0
- PCM5102A stereo DAC on the FX module as the final audio output

---

# Pin Assignment

The core WELLENBAD hardware assignment remains compatible with the established platform.

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
| Button 7 / PLAY | 3 |
| Button 8 / BACK | 14 |
| Encoder A | 1 |
| Encoder B | 2 |
| Encoder switch | 42 |
| SD MISO | 13 |
| SD CS | 9 |

OLED and SD share the SPI clock / data lines used by the hardware design.

## FX control link

The current firmware also contains the RTAL control link for the companion FX processor:

| Function | GPIO |
|---|---:|
| FX Link RX | 8 |
| FX Link TX | 5 |
| Baud rate | 115200 |

This UART carries control, clock/tempo metadata and diagnostics. Audio is transported separately.

---

# SD Card Structure

The firmware uses the established RTAL directories:

```text
/RTALWT        SD wavetables
/RTALPRESETS   User Programs
/RTALBACKUP    backup data
/RTALMETA      metadata
/RTALMULTI     Multi setups
```

Typical current files include:

```text
/RTALPRESETS/U030.RTAL
...
/RTALPRESETS/U127.RTAL
```

and Multi setup files for the `M000–M127` range.

SD-card access and wavetable caching are designed so blocking file operations do not occur in the per-sample audio render path.

---

# Build Environment

The v1.5 release is based on the established WELLENBAD toolchain:

- **Arduino IDE 1.8.19**
- **ESP32 Arduino Core 2.0.16**
- ESP32-S3 target
- PSRAM enabled
- 16 MB Flash target configuration

Open:

```text
WB0072/WB0072.ino
```

in Arduino IDE.

Keep all supplied `.ino`, `.h` and `.cpp` files together in the `WB0072` sketch directory.

---

# Production Configuration

Important production settings:

```cpp
#define SAMPLE_RATE 44100
#define AUDIO_BLOCK 128
#define NUM_VOICES 7

#define RTAL_ENABLE_FAST_POLY 0
#define RTAL_DISABLE_CHORUS_HIGH_POLY 0
#define RTAL_INTERNAL_CHORUS 1
#define RTAL_FILTER_CONTROL_DIV 4
#define RTAL_PROFILER_ENABLED 0
```

The release uses compiler optimization directives:

```cpp
#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")
```

## Fast Poly

`RTAL_ENABLE_FAST_POLY` remains deliberately disabled.

The v1.5 performance target was reached through real DSP-path optimization rather than by re-enabling the retired Fast Poly mode.

---

# Diagnostics

The production firmware retains lightweight diagnostics that are useful during hardware testing and troubleshooting.

Available information includes, depending on the active diagnostic view:

- active voice information
- current DSP time
- maximum DSP time
- audio-block deadline
- overrun count
- MIDI queue activity
- MIDI Clock status
- ARP / SEQ state
- SD status
- wavetable loading status
- heap / PSRAM information
- Remote link health
- FX link state

The detailed development profiler is disabled in the final production build to avoid unnecessary timing overhead.

---

# Real-Time Performance

At 44.1 kHz with 128-frame blocks, WELLENBAD has approximately:

```text
128 / 44100 ≈ 2.902 ms
```

to complete one audio block.

v1.5 was developed around keeping the worst-case musical workload inside this deadline.

## Why seven voices?

The previous v1.3 release advertised eight voices but also documented that demanding patches at 6–8 continuously active voices could occasionally exceed the block deadline.

For v1.5, the release criterion was changed:

> The official polyphony should describe the configuration that is actually reliable on the target hardware.

The final v1.5 configuration therefore uses:

```text
NUM_VOICES 7
```

Seven voices were validated during development in:

- SINGLE mode
- MULTI mode
- demanding 4-pole-filter patches
- operation with the USB Remote Editor using SMART SAFE

This is deliberately more conservative than a nominal “maximum possible” number and better reflects the real-time objective of the project.

---

# Compatibility Notes

## Programs

- Factory Programs remain in the `F000–F029` area.
- User Programs occupy `U030–U127`.
- Current User Program files can include persistent FX data.
- Older User Program formats without FX data load with deterministic safe FX defaults rather than accidentally inheriting an unrelated previous scene.

## Multis

- v1.5 provides `M000–M127`.
- earlier Multi file versions remain readable
- new fields that did not exist in older formats receive neutral defaults
- current saves use the latest Multi format

## Performance Mode

Loading or selecting content is deliberately context-aware:

- normal Program work belongs to SINGLE
- a Part Program can be edited inside MULTI without silently forcing a mode change
- Multi Program Change selects complete Multi setups
- invalid / unavailable Multi loads fail safely instead of leaving a partially changed performance

---

# Development Philosophy

WELLENBAD is not developed as a synthetic benchmark.

Every major generation has been tested as an actual musical instrument, and several v1.5 decisions came directly from real hardware behavior:

- replacing the old filter with a higher-quality 4-pole design
- optimizing the filter until seven voices became practical
- identifying separate SINGLE and MULTI performance costs
- finding USB-CDC traffic as a source of rare audible real-time stalls
- redesigning editor communication around SMART SAFE
- deliberately publishing seven reliable voices rather than retaining an unreliable nominal eight

This iterative process is central to RTAL:

> measure, listen, reproduce, optimize, test again.

---

# Version History

## v1.3

The first published four-Part multitimbral WELLENBAD release:

- nominal 8-voice shared engine
- 4 Parts
- independent ARP / SEQ
- 32 Multis
- per-Part SD wavetable cache
- MIDI / CC Learn
- documented real-time limits at high sustained polyphony

## v1.5.0

Current release:

- 7 hardware-validated voices
- new 4-pole FAST32 biquad low-pass filter
- Drive + global resonance
- optimized seven-voice real-time DSP path
- 128 Multi slots
- explicit SINGLE / MULTI Performance Mode
- mode-aware Program Change
- per-Part Key Low / Key High
- per-Part Root Note
- per-Part Pan
- current User Program format with persistent FX state
- shared persistent FX state in Multi
- native USB-CDC Remote Editor
- graphical sequencer editing
- editor Preset / Multi / FX management
- CDC SMART SAFE real-time protection
- release-oriented cleanup and validation

---

# Release Identity

```text
Product : RTAL WELLENBAD
Version : 1.5.0
Build   : B0072
Series  : A015
Status  : FINAL
FX      : RTAL AudioDSP FX v1.0.0 / FX0047 FINAL
```

---

# Repository

RTAL WELLENBAD is part of:

**RTAL — Real Time Audio Lab**

Repository:

`RTAL-EAI-002-WELLENBAD`

The firmware is intended both as a musical instrument and as an open reference for practical real-time audio development on the ESP32-S3.

---

# Notes for Developers

When modifying v1.5, keep the following real-time rules in mind:

1. Do not add SD or filesystem work to the audio path.
2. Avoid expensive transcendental functions inside per-sample rendering.
3. Do not add apparently “small” branches to the hottest sample/voice loops without measuring their effect at seven voices.
4. Keep MIDI Clock handling higher priority than bulk controller work.
5. Keep large USB-CDC state transfers away from active high-polyphony audio.
6. Preserve `RTAL_ENABLE_FAST_POLY = 0` unless a future branch is explicitly created to revisit it.
7. Treat the 2.902 ms block deadline as a hard real-time budget, not an average target.
8. Test SINGLE and MULTI separately: they do not have identical overhead.
9. Validate changes with the Remote Editor both disconnected and connected.
10. Prefer block-rate or control-rate preprocessing when a result does not need to be recalculated per sample.

---

## Final Note

WELLENBAD v1.5.0 is a consolidation release.

It does not try to win by quoting the largest possible feature numbers. Instead, it brings the synthesis engine, Multi architecture, filter, preset system and computer editor together into a configuration that has been tuned around the actual limitations of the ESP32-S3.

The result is a compact seven-voice, four-Part wavetable synthesizer with a surprisingly broad performance architecture — built around one inexpensive microcontroller and developed as a real instrument rather than only as a DSP experiment.

**RTAL WELLENBAD v1.5.0 — Real Time Audio Lab**
