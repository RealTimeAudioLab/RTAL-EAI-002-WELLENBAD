# RTAL WELLENBAD v1.2

## Firmware **v1.2** introduces the completed **RTAL UI Engine 1.0**. The new interface provides direct access to every synthesis parameter while retaining the previous interface as a runtime fallback.

## Highlights of v1.2

- Six-voice polyphonic wavetable synthesis
- 44.1 kHz stereo I2S audio output
- 127 visible wavetable slots
- Built-in Flash wavetables plus SD-card wavetable support
- 64-wave and 128-wave table files
- Two oscillators per voice with wavetable offset, detune and mix
- Resonant filter with dedicated envelope
- Amp, filter and wavetable envelopes
- Main LFO with six selectable waveforms
- Dedicated wavetable LFO
- Velocity, aftertouch, key tracking, pitch bend and glide
- Poly, mono, unison and poly-glide play modes
- Chorus, drive, bitcrusher, sub oscillator and noise
- Arpeggiator with internal or MIDI clock
- 16-step modulation sequencer
- Complete sequencer data stored per preset
- Graphical live Sequencer Show page
- Morph A/B sound interpolation and musical Randomize
- Original/Edited Compare function
- Category-based preset browser
- Multi-stage user-preset STORE workflow
- Visible MIDI CC Learn workflow
- Live Wave Monitor following the latest active voice
- System, audio and storage diagnostic pages
- Automatic UI parameter audit at startup
- Backward-compatible loading of older preset formats

---

## Synthesis Architecture

```text
MIDI / Arpeggiator / Sequencer
              │
              ▼
       Voice Allocation
              │
      ┌───────┴────────┐
      │   Six Voices   │
      │                │
      │ OSC1 + OSC2    │
      │ Wavetable Scan │
      │ Sub + Noise    │
      │ Filter         │
      │ Amp Envelope   │
      └───────┬────────┘
              │
              ▼
        Stereo Effects
              │
              ▼
       Soft Limiter / I2S
```

### Oscillator section

Each voice provides two related wavetable oscillators. The second oscillator can use an offset position within the same table and can be detuned independently. `OSC MIX` blends between OSC1 and OSC2.

### Modulation sources

- Amp envelope
- Filter envelope
- Wave envelope
- Main LFO
- Dedicated Wave LFO
- Velocity
- Channel and polyphonic aftertouch
- Mod wheel
- Key tracking
- Modulation sequencer
- Pitch bend

### Main LFO waveforms

- Triangle
- Sine
- Saw Up
- Saw Down
- Square
- Random / Sample & Hold

---

## RTAL UI Engine 1.0

The 128 × 64 OLED interface follows three clearly separated visual levels.

### Performance pages

Performance pages show status information without parameter bars. They are intended for playing and monitoring.

### Edit pages

Edit pages show the parameter name, value and bar on one line. Shared columns and a common vertical grid keep all pages visually consistent.

### System pages

System pages use text lists without parameter bars. They provide MIDI, firmware, audio and storage information.

The fixed header displays:

```text
U070 HEAVEN CHOIR         CH01 V2
```

- preset slot and name,
- MIDI receive channel,
- currently active voices.

### Runtime UI fallback

Hold **SOUND + EDIT** simultaneously for approximately 1.2 seconds to switch between RTAL UI Engine 1.0 and the legacy interface.

---

## Front-Panel Controls

The physical buttons are labelled:

```text
SOUND  EDIT  MOD  FX  STORE  LOAD  PLAY  BACK
```

General operation:

- Short button press: open or cycle through the corresponding page group
- Long button press: jump to the primary page or alternate function of that group
- Encoder rotation: change the selected value
- Encoder short press: select the next parameter or workflow item
- Encoder long press: start MIDI CC Learn where supported
- BACK: leave dialogs or return to HOME

### Main page groups

**SOUND**

```text
OSC1 → OSC2 → MIXER → NOISE → SUB → OUTPUT → WAVE MON
```

**EDIT**

```text
FILTER → AMP ENV → FILTER ENV → WAVE ENV
```

**MOD**

```text
LFO → WAVE LFO → PERFORMANCE MOD → MORPH
```

**PLAY**

```text
HOME → PERFORMANCE → ARPEGGIATOR → SEQUENCER
→ SEQ EDIT → SEQ SHOW
```

---

## Wave Monitor

The Wave Monitor renders the current wavetable waveform on the OLED.

- `SCAN`: manually inspect the logical wave position
- `TABLE`: select the wavetable
- `LIVE`: follow the most recently triggered active voice

During playback, the monitor follows the voice's effective modulated wave position, including wavetable envelope, LFO, aftertouch and controller modulation.

---

## Arpeggiator

The arpeggiator supports:

- several playback modes,
- selectable rate,
- octave range,
- hold operation,
- internal clock,
- external MIDI clock,
- MIDI Start, Stop and Continue.

With internal clock, a newly started arpeggio triggers its first note immediately. With external MIDI clock, it remains aligned to the incoming clock grid.

---

## Modulation Sequencer

The 16-step modulation sequencer can target sound parameters such as wavetable position, filter cutoff, resonance and effects.

Editable parameters include:

- Mode
- Rate
- Number of active steps
- Modulation target
- Depth
- Table Mode: `ABS` or `REL`
- Step selection
- Value for each of the 16 steps

### Per-preset sequencer storage

Program format V4 stores the complete sequence with every preset:

```cpp
uint8_t seqValues[16];
uint8_t seqTableMode;
```

### SEQ SHOW

`PLAY / SEQ SHOW` visualizes the 16 step values as vertical bars and highlights the step currently being executed. It is a read-only performance display.

---

## Morph and Randomize

Morph is a temporary sound-design system that interpolates between two complete sound states.

```text
Morph Amount 0     = Snapshot A
Morph Amount 64    = approximately equal A/B blend
Morph Amount 127   = Snapshot B
```

The Morph page provides:

- Morph Amount
- Morph Source: Amount, Mod Wheel or Aftertouch
- Capture A
- Capture B
- Randomize

Randomize creates musically useful parameter variations. Save the result as a user preset to keep it.

Discrete values such as LFO Shape switch between the A and B state rather than producing undefined intermediate shapes.

---

## Presets

WELLENBAD provides 128 program slots:

- Factory presets: `F000–F029`
- User presets: `U030–U127`

### Preset Browser

The browser always opens in category `ALL` and initially selects the currently loaded program.

Available categories:

```text
ALL  PAD  LEAD  BASS  SEQ  FX  ORGAN  MISC  USER
```

### STORE workflow

The new STORE workflow uses three clear stages:

```text
SLOT → NAME → CONFIRM
```

It includes:

- user-slot selection,
- name editor,
- factory-slot protection,
- overwrite confirmation,
- cancellation through BACK.

### Preset compatibility

The current format is **Program V4**. Older V1, V2 and V3 presets are migrated automatically when loaded.

---

## MIDI

Supported MIDI messages include:

- Note On / Note Off
- Program Change
- Control Change
- Pitch Bend
- Channel Aftertouch
- Polyphonic Aftertouch
- MIDI Clock
- Start / Stop / Continue
- Song Position
- System Exclusive program transfer

### MIDI CC modes

- **BANKED**: fixed controller maps
- **LEARN**: user-defined assignments

### Visible CC Learn

Hold the encoder while a normal edit parameter is selected. The display shows:

```text
MIDI / CC LEARN
Parameter    Cutoff
Current CC   CC74
Status       MOVE CONTROL
```

After receiving a controller movement, the assignment is stored and confirmed on screen.

### MIDI thru behaviour

- MIDI Clock, Start and Stop are forwarded continuously.
- Control Change messages are forwarded when MIDI CC Bank 2 is selected.
- The MIDI library's automatic thru function remains disabled to avoid duplicate messages.

---

## SD-Card Storage

Default directories used by the firmware:

```text
/RTALWT          Wavetables
/RTALPRESETS     User presets
/RTALBACKUP      Bank backups
/RTALMETA        System metadata and configuration
```

### Wavetable files

Supported table sizes:

- 16,384 bytes: 64 waves
- 32,768 bytes: 128 waves

Supported project formats include signed `.WTB`, unsigned `.UWT` and legacy `.RAW` data.

SD wavetables replace the highest visible Flash slots from the back:

```text
SD table 0 → slot 126
SD table 1 → slot 125
SD table 2 → slot 124
...
```

The underlying Flash tables remain available when the SD files are removed.

---

## Build Environment

The validated project environment is:

- Arduino IDE 1.8.19
- ESP32-S3 target board with PSRAM
- Project-selected ESP32 Arduino core and board settings
- 44.1 kHz I2S audio

Required libraries and framework components:

- Arduino ESP32 core
- U8g2
- FortySevenEffects MIDI Library
- BfButton
- ESP32 Preferences
- ESP32 I2S driver
- ESP32 FS and SD libraries

### Build steps

1. Download or clone the repository.
2. Open `RTAL_WELLENBAD_v1.2/RTAL_WELLENBAD_v1.2.ino` in Arduino IDE.
3. Select the correct ESP32-S3 board.
4. Enable PSRAM using the setting appropriate for the board.
5. Install the required external libraries.
6. Verify the pin assignment against the hardware.
7. Compile and upload.
8. Open the serial monitor for boot diagnostics and the RTAL UI audit.

The source folder must retain its Arduino sketch name:

```text
RTAL_WELLENBAD_v1.2/
└── RTAL_WELLENBAD_v1.2.ino
```

---

## Startup Audit and Diagnostics

At startup, the firmware checks the new UI's parameter mapping and reports the result through the serial monitor:

```text
RTAL UI AUDIT v1.2
Parameters:       57
Mapped unique:    57
Missing:           0
Invalid refs:      0
Result:            OK
```

System pages provide live information for:

- firmware and UI version,
- voice use,
- voice steals,
- processing queue,
- SD-card state,
- wavetable count,
- load errors,
- free PSRAM.

The performance profiler is disabled by default in the release build and can be enabled in the source for development measurements.

---

## Repository Layout

```text
RTAL_WELLENBAD_v1.2/
├── RTAL_WELLENBAD_v1.2.ino   Main firmware and synthesis engine
├── audio.ino                 Audio rendering and I2S processing
├── midi.ino                  MIDI input, CC maps and SysEx
├── arp.ino                   Arpeggiator
├── sequencer.ino             Modulation sequencer
├── modulation.ino            Modulation helpers
├── efx.ino                   Internal effects
├── input.ino                 Buttons and encoder
├── ui.ino                    Legacy UI
├── rtal_ui_engine.ino        RTAL UI Engine 1.0
├── sdcard.ino                Presets, banks and wavetables
├── Wellenbad_Wavetables.h    Built-in wavetable data
├── RTALFooterFont.*          Compact footer font
├── RTALUILayout.h            Shared OLED layout grid
├── RTAL_Profiler.*           Optional performance profiler
├── CHANGELOG_v1.2.md
└── RELEASE_NOTES_v1.2.md
```

---

## Project Status

Firmware v1.2 is the first official release based on RTAL UI Engine 1.0. The legacy user interface remains available as a fallback while the new UI is validated in wider use.

Please report reproducible issues with:

- board and ESP32 core version,
- compiler output,
- connected hardware,
- exact operating steps,
- serial-monitor output,
- and photographs or video of display-related problems where useful.

---

## Author

**RealTimeAudioLab**  
Embedded Audio · MIDI · Wavetable Synthesis · Real-Time DSP

---

## Licence

Add the repository licence file and state the selected licence here before publication. Third-party libraries and included data remain subject to their respective licences.

------------------------------------------------------------------------
------------------------------------------------------------------------

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
