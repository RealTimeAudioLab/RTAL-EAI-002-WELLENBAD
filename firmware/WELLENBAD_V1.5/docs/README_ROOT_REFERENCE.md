# RTAL WELLENBAD  
**ESP32-S3 Wavetable Synthesizer with USB-CDC Editor, Graphical Sequencer and Advanced Multi Mode**

RTAL WELLENBAD is a custom ESP32-S3 based wavetable synthesizer developed by RealTimeAudioLab.  
It combines a compact standalone hardware instrument with a modern browser-based USB-CDC editor, deep wavetable sound design, flexible modulation, sequencing, multi-performance capabilities and a dedicated second ESP32-S3 running the RTAL AudioDSP FX Module v1.0.0.

WELLENBAD has evolved significantly beyond the earlier public v1.3 release.  
With **v1.5.0 Final**, the project reaches a much more mature state with:

- stable **7-voice operation**
- improved performance under demanding presets
- integrated **browser-based USB-CDC editor**
- **Graphical Wave Sequencer**
- extended **Multi Mode**
- better preset and multi handling
- improved real-time safety for editor communication

---

## v1.5 Highlights

### New in RTAL WELLENBAD v1.5.0 Final

- **7 voices stable** in Single and Multi mode
- **4-pole biquad lowpass filter**
- optimized DSP structure for higher voice count
- **USB-CDC browser editor**
- **Wave Monitor**
- **Graphical Wave Sequencer Editor**
- **Preset and FX preset management**
- **Multi mode improvements**
- **SMART SAFE editor communication**
- dedicated **RTAL AudioDSP FX Module v1.0.0 / FX0047 FINAL**
- Stereo Delay, Chorus, Flanger, Phaser, Reverb, Width and final Limiter on a second ESP32-S3
- better runtime safety under editor connection
- cleaner release candidate to final transition

---


### v1.5.0 final validation

The published v1.5.0 firmware is **B0074F A015 FINAL**. Final hardware tests after the earlier B0072 freeze corrected two MULTI performance edge cases: MIDI Program Change is now deferred out of the MIDI task to prevent stack overflow during Multi loads, and Part ENABLE/DISABLE/MUTE remains synchronized with the browser editor while several HOLD Arpeggiators are running. Seven-voice MULTI operation was revalidated after these fixes.

# Editor & Remote Control

One of the most important additions in WELLENBAD v1.5 is the **USB-CDC PC Editor**.

The editor transforms WELLENBAD from a hardware-only synth into a far more efficient sound-design and performance platform.  
It provides direct access to parameters, visualization tools, sequencing functions and system diagnostics — all inside a browser-based interface.

## Why the Editor Matters

The editor makes WELLENBAD faster and easier to use by providing:

- **faster parameter access**
- **better visual feedback**
- **waveform and wavetable activity monitoring**
- **graphical sequencing workflow**
- **remote hardware control**
- **preset and multi management**
- **live diagnostics and communication status**

## Editor Features

- Browser-based interface
- USB-CDC connection
- Parameter editing
- OLED mirror / remote control support
- Wave Monitor
- Graphical Wave Sequencer
- Performance-safe communication modes
- Link health / diagnostics
- Preset handling support
- Multi workflow support

---

## Editor Screenshots

> Replace the following placeholders with your final screenshots.

### Full Editor Overview
![WELLENBAD Editor Overview](docs/images/editor_overview.jpg)

### Wave Monitor
![WELLENBAD Wave Monitor](docs/images/editor_wave_monitor.jpg)

### Graphical Wave Sequencer
![WELLENBAD Graphical Wave Sequencer](docs/images/editor_wave_sequencer.jpg)

### Preset / Multi Management
![WELLENBAD Preset Multi Management](docs/images/editor_preset_multi.jpg)

### OLED Mirror / Hardware Interaction
![WELLENBAD OLED Mirror](docs/images/editor_oled_mirror.jpg)

### Diagnostics / Link Health
![WELLENBAD Diagnostics](docs/images/editor_diagnostics.jpg)

---

# Hardware

RTAL WELLENBAD is based on an **ESP32-S3** platform and was designed as a compact standalone wavetable synthesizer with direct hardware control.

## Core Hardware

- **ESP32-S3**
- **16 MB Flash**
- **8 MB PSRAM**
- **SSD1309 128x64 OLED**
- **MIDI IN / MIDI OUT**
- **I2S Audio**
- **second ESP32-S3 AudioDSP FX processor**
- **PCM5102A final stereo DAC on the FX module**
- **SD card support**
- dedicated buttons and encoder interface

## Typical System Architecture

- **Core 1**  
  Audio engine, highest priority

- **Core 0**  
  MIDI, UI, storage and communication tasks

This split is essential for keeping the audio path stable while still enabling features such as the USB-CDC editor and advanced UI functionality.

---

# Hardware Photos

> Keep your existing photos here for now.  
> You can later add or replace them with improved shots.

![WELLENBAD Hardware Photo 1](docs/images/wellenbad_photo_1.jpg)
![WELLENBAD Hardware Photo 2](docs/images/wellenbad_photo_2.jpg)
![WELLENBAD Hardware Photo 3](docs/images/wellenbad_photo_3.jpg)

---

# Sound Engine

WELLENBAD focuses on digital wavetable synthesis with expressive modulation and performance-oriented control.

## Main Synthesis Features

- wavetable-based oscillator architecture
- multiple waveform positions
- modulation-driven timbral movement
- dedicated envelopes and LFOs
- filter section with **4-pole biquad lowpass**
- resonance and drive behavior
- performance-oriented parameter access
- support for rich animated wavetable sounds

## Voices

- public stable target in v1.5: **7 voices**
- stable Single mode operation
- stable Multi mode operation
- optimized for practical real-world presets
- editor-safe communication refinements for live use

---


# Dedicated AudioDSP FX Module

WELLENBAD v1.5.0 is released together with a dedicated second ESP32-S3 effects processor:

```text
RTAL WELLENBAD
v1.5.0 B0074F A015 FINAL

        +

RTAL AudioDSP FX Module
v1.0.0 FX0047 FINAL
```

The FX release is based on the proven **0.1.0ai Build0046l StableSyncDelay** development state. FX0047 is a production release freeze: the DSP algorithms, routing, I2S format, block geometry and WELLENBAD FX-Link protocol are unchanged from that stable reference.

## Why a separate FX processor?

WELLENBAD already uses the main ESP32-S3 for seven voices of wavetable synthesis, filters, envelopes, modulation, ARPs, sequencers, MIDI, SD, OLED and editor communication.

A second ESP32-S3 gives the stereo effects chain its own real-time budget.

```text
WELLENBAD Main ESP32-S3
        |
        | 7-voice synth / 4-Part mix
        |
        | I2S 44.1 kHz stereo
        v
AudioDSP FX ESP32-S3
        |
        +--> Stereo Delay
        |
        +--> Chorus OR Flanger OR Phaser
        |
        +--> Stereo Reverb
        |
        +--> Stereo Width
        |
        +--> Output Limiter
        |
        | I2S
        v
PCM5102A
        |
        v
Stereo Line Out
```

The production processing order is:

```text
Delay -> MOD FX -> Reverb -> Width -> Limiter
```

## FX v1.0.0 feature set

### Stereo Delay

- independent Left / Right delay time
- up to approximately 1000 ms
- Feedback
- Level
- feedback HPF
- feedback LPF
- feedback saturation
- Crossfeed / Ping-Pong
- Ducking
- Freeze
- FREE timing
- MIDI-clock synchronized timing
- RAM presets
- NVS presets
- smooth preset transitions

### Stable Sync Delay

The final 0046l/FX0047 synchronization logic is designed to avoid audible flanging or Doppler-style pitch sweeps when a synchronized delay time changes.

- tempo changes must remain stable across multiple timing windows
- small MIDI Clock jitter is ignored
- meaningful sync-time changes use an old fixed tap and a new fixed tap simultaneously
- the two taps are crossfaded over about **24 ms**
- FREE/manual delay smoothing remains unchanged

### Chorus

- stereo modulation
- two taps per channel
- hybrid Hermite / linear interpolation
- tone control
- parameter smoothing
- shared RTAL modulation core

### Flanger

- stereo short-delay modulation
- feedback
- feedback HPF
- saturation
- shared modulation core

### Phaser

- optimized 2-stage / 4-stage allpass processing
- feedback
- feedback HPF
- saturation
- center frequency
- control-rate coefficient generation
- full audio-rate allpass processing

### Stereo Reverb

- FDN-style stereo reverb
- Mix
- Size
- Decay
- Damping
- Predelay
- real-time optimized memory placement

### Stereo Width

- Mid/Side image control
- approximately 0–200%
- located after Reverb and before the output Limiter

### Output Limiter

A lightweight final limiter protects the DAC/output path from excessive peaks produced by combined effects.

## Digital audio wiring

### WELLENBAD -> FX

| Signal | WELLENBAD | FX |
|---|---:|---:|
| BCLK | GPIO18 | GPIO18 |
| LRCK | GPIO16 | GPIO16 |
| DATA | GPIO17 | GPIO5 |
| GND | GND | GND |

WELLENBAD provides the input I2S clocks.

### FX -> PCM5102A

| Signal | FX | PCM5102A |
|---|---:|---|
| BCLK | GPIO18 | BCK |
| LRCK | GPIO16 | LRCK |
| DATA OUT | GPIO17 | DIN |
| GND | GND | GND |

The PCM5102A connected to the FX module is the **final audio output DAC**.

## Bidirectional FX control link

Audio and control are independent.

```text
WELLENBAD GPIO5 TX  ----> FX GPIO40 RX
WELLENBAD GPIO8 RX  <---- FX GPIO39 TX
GND                  ---- GND

115200 baud
8N1
RTAL WELLENBAD FX Protocol v1
CRC8-protected fixed frames
```

The link carries:

- HELLO / ACK
- PING / PONG
- state requests and state feedback
- effect parameters
- effective BPM
- INT / MIDI clock-source information
- diagnostics and connection state

## Editor integration

The browser editor connects only to WELLENBAD through USB CDC.

FX controls are forwarded internally:

```text
PC / Browser
     |
     | USB CDC
     v
WELLENBAD Main
     |
     | UART FX-Link
     v
AudioDSP FX
```

This keeps the normal user workflow simple: **one USB connection controls the complete synthesizer + effects system**.

## SINGLE / MULTI and FX

The FX chain is global and runs after the complete stereo synth mix.

In MULTI mode:

```text
Part 1 --\
Part 2 ---\
Part 3 ----> Shared Stereo Mix -> AudioDSP FX -> Stereo Out
Part 4 ---/
```

A Multi therefore stores **one shared FX scene** for the complete four-Part setup.

The system does not attempt to run four separate Reverb/Delay engines on the four Parts.

## Real-time design

The FX processor uses the same 44.1-kHz / 128-frame timing model as WELLENBAD.

```text
Block deadline ~= 2.902 ms
```

Production optimizations include:

- delay filter coefficient work moved out of the per-sample hot path
- coefficient refresh only when required
- 16-sample delay HPF/LPF coefficient update interval while parameters move
- optimized modulation-effect render paths
- optimized Reverb memory placement
- reduced production diagnostics
- retained deadline, I2S, watchdog and task-health monitoring

## Official release pair

| Synthesizer | Effects Processor | Status |
|---|---|---|
| **WELLENBAD v1.5.0 B0074F A015 FINAL** | **AudioDSP FX v1.0.0 FX0047 FINAL** | **Recommended** |

For the public WELLENBAD v1.5 release, these two firmware versions should be treated as one tested system generation.

---

# Multi Mode

v1.5 significantly improves the Multi workflow.

## Multi Capabilities

- layered / split style working methods
- part-based editing
- part routing improvements
- better stability in demanding voice-count situations
- improved real-time behavior under editor connection
- multi save / load functionality
- better usability for performance setups

Multi mode is an important part of WELLENBAD v1.5 because it turns the synth into a more flexible instrument for layered sounds and performance-oriented setups.

---

# Sequencer & Arpeggiator

WELLENBAD includes sequencing and arpeggiation tools that are deeply integrated into the sound engine.

## Included Functions

- onboard arpeggiator
- sequencer functions
- **Graphical Wave Sequencer**
- parameter-linked movement
- editor-based visual editing support
- performance integration with Single and Multi mode

The Graphical Wave Sequencer is one of the standout features of the newer WELLENBAD generations and benefits strongly from the browser editor.

---

# USB-CDC Communication Concept

The v1.5 generation introduced several iterations of the USB-CDC communication system in order to keep editor control compatible with real-time audio demands.

## Design Goals

- preserve audio stability
- avoid DSP overload while editing
- keep monitoring useful
- reduce unnecessary traffic under load
- prioritize sound generation over UI traffic when required

## Result in v1.5 Final

The final communication concept allows practical editor use while maintaining a stable 7-voice system.  
This was a major step forward compared to the earlier development state.

---

# From v1.3 to v1.5

Compared to the last public **v1.3** release, WELLENBAD v1.5 introduces major functional and technical advances.

## Major Progress Since v1.3

- voice architecture advanced to stable **7 voices**
- editor introduced and matured
- graphical editor functionality added
- wave visualization added
- sequencer editing expanded
- multi mode significantly improved
- DSP optimizations refined
- runtime safety improved
- communication strategy improved
- project maturity clearly increased

WELLENBAD v1.5 is therefore not just a small update, but a substantial step forward in usability, capability and overall completeness.

---

## Final validation fixes in B0074F

The public v1.5.0 release is based on **B0074F A015 MULTI TOGGLE SYNC FIX1**. It supersedes the earlier B0072 release candidate/freeze because final hardware validation exposed two MULTI-mode edge cases that have now been corrected and retested.

### Deferred MIDI Program Change in MULTI

A MIDI Program Change received while MULTI mode was active previously performed the complete Multi/SD load synchronously inside the high-priority `MidiTask`. The deeper SD/FAT and Multi-load call path could exceed the 4096-byte MIDI task stack and trigger:

```text
Stack canary watchpoint triggered (MidiTask)
```

B0074F retains the real-time-safe deferred Program Change architecture introduced during final validation:

```text
MIDI Program Change
        |
        v
MidiTask: store tiny pending request only
        |
        v
ControlTask: perform Program or Multi load
```

The normal MIDI path therefore remains short and deterministic. Rapid Program Changes are coalesced with a latest-request-wins policy instead of forcing a queue of expensive SD loads.

### MULTI HOLD + ENABLE/DISABLE/MUTE

Final live-performance testing also found a state-synchronization issue when several Parts were running Arpeggiators with **HOLD** enabled and Parts were toggled between ENABLED/DISABLED or MUTE/MUTED.

B0074F fixes both sides of the problem:

- Performance Disable/Mute silences the current Part without destroying its latched HOLD note pool.
- Structural operations such as Multi load, Program replacement, MIDI-channel changes or key-zone changes still perform a full Part reset.
- Re-enabling a Part can therefore resume its held Arpeggiator without requiring the chord to be played again.
- The browser editor now treats ENABLE/MUTE/SOLO changes as pending state until the device confirms the same state.
- Successful WebSerial writes return an explicit success value; a successful command is no longer rolled back as if transmission had failed.
- Pending state no longer expires after an arbitrary timeout, which is important while SMART SAFE intentionally suppresses large state packets during active audio.
- Stale Multi-state packets cannot overwrite a newer local toggle while confirmation is pending.

The validated workflow is now reliable with several simultaneous HOLD Arpeggiators: Parts can be disabled one after another, re-enabled one after another, and disabled again while the editor indication remains synchronized with the audible state.

### Seven-voice MULTI validation

The final deferred MIDI-PC implementation does not add a permanent FreeRTOS queue poll to the ControlTask. This preserves the real-time reserve required for the validated **seven-voice MULTI** configuration. SINGLE and MULTI therefore use the same official seven-voice release target.

# Firmware Status

## Current Public Release

**RTAL WELLENBAD v1.5.0 Final**  
**Recommended FX companion: RTAL AudioDSP FX v1.0.0 / FX0047 FINAL**

Status:
- stable
- release-ready
- recommended public version

## Key Characteristics of v1.5.0 Final

- stable 7-voice operation
- strong Single mode usability
- stable Multi mode
- integrated browser editor
- improved editor safety under load
- refined filter implementation
- matched **AudioDSP FX v1.0.0 FX0047 FINAL** companion release
- suitable as the current public GitHub release

---

# Repository Structure

Example structure:

```text
RTAL-EAI-002-WELLENBAD/
├── README.md
├── firmware/
│   ├── WELLENBAD_V1.3/
│   ├── WELLENBAD_V1.5/
│   └── FX_MODULE_V1.0/
├── docs/
│   ├── images/
│   ├── editor/
│   └── notes/
└── media/
```

---

# Installation / Usage

## Firmware

1. Open the firmware project in Arduino IDE  
2. Select the correct ESP32-S3 board configuration  
3. Verify pin mapping and local setup  
4. Compile and upload the firmware  
5. Insert SD card if required by your setup  
6. Connect MIDI and audio hardware  

## FX Module

1. Open `FX0047/FX0047.ino`
2. Flash the second ESP32-S3 with **RTAL AudioDSP FX v1.0.0 / FX0047 FINAL**
3. Connect the I2S link: `18 -> 18`, `16 -> 16`, `17 -> 5`
4. Connect the control link: WELLENBAD `TX5 -> FX RX40`, WELLENBAD `RX8 <- FX TX39`
5. Connect common GND
6. Connect FX `BCLK18 / LRCK16 / DATA17` to the PCM5102A
7. Take the final stereo audio output from the PCM5102A attached to the FX module

## Editor

1. Connect WELLENBAD via USB  
2. Open the editor HTML file in your browser  
3. Use **Connect WELLENBAD CDC**
4. Start editing and monitoring the synth in real time  

---

# Recommended Media Additions

To make this repository stronger on GitHub, the following media are especially recommended:

- one strong **hero hardware photo**
- one **complete editor overview**
- one **Wave Monitor screenshot**
- one **Graphical Wave Sequencer screenshot**
- one **preset / multi screenshot**
- one short **animated GIF** showing editor interaction
- one short **audio demo section**

---

# Project Status

RTAL WELLENBAD is an actively developed custom synthesizer platform and represents a mature ESP32-S3 wavetable instrument with a distinctive combination of:

- standalone hardware workflow
- browser-based editor workflow
- wavetable synthesis
- graphical sequencing
- advanced multi-performance features
- practical real-time engineering on embedded hardware

---

---

# Acknowledgements

## Historical Inspiration

WELLENBAD stands in the tradition of digital wavetable synthesis pioneered by instruments such as the **PPG Wave**.

Historical inspiration:

- **Wolfgang Palm**
- **PPG Wave**

## Open-Source Ecosystem

WELLENBAD has also benefited from the broader embedded-audio and open-source ecosystem.

Technical inspiration and community contributions include:

- **Marcel Licence**
- **Rolf Degen**
- **Mutable Instruments**
- **Craig Barnes**
- **Open Source Community**

WELLENBAD is an independent RealTimeAudioLab project. These acknowledgements recognize the people, projects and communities that have helped shape the wider field of digital synthesis and open embedded-audio development.

---

# License

**GNU General Public License v3**

WELLENBAD is released under the GNU General Public License version 3.

Please refer to the repository `LICENSE` file for the complete license terms.

---

# RealTimeAudioLab

**Engineering modern embedded audio systems through open development.**

Project: **RTAL WELLENBAD**  
Repository: **RTAL-EAI-002-WELLENBAD**  
Current Release: **WELLENBAD v1.5.0 — B0074F A015 FINAL**  
Companion FX Release: **AudioDSP FX v1.0.0 — FX0047 FINAL**

> Build real instruments, document the engineering honestly, and make the process useful to others.
