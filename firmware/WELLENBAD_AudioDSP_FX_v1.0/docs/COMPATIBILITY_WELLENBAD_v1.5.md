# WELLENBAD v1.5.0 + AudioDSP FX v1.0.0 Compatibility

## Recommended pair

- Synth: `RTAL WELLENBAD v1.5.0 B0072 A015 FINAL`
- FX: `RTAL AudioDSP FX Module v1.0.0 FX0047 FINAL`
- FX DSP basis: `0.1.0ai Build0046l StableSyncDelay`

## Audio link

```text
WELLENBAD GPIO18 BCLK  -> FX GPIO18 BCLK
WELLENBAD GPIO16 LRCK  -> FX GPIO16 LRCK
WELLENBAD GPIO17 DATA  -> FX GPIO5 DATA IN
GND                    -> GND
```

FX output:

```text
FX GPIO18 BCLK      -> PCM5102A BCK
FX GPIO16 LRCK      -> PCM5102A LRCK
FX GPIO17 DATA OUT  -> PCM5102A DIN
GND                 -> GND
```

## Control link

```text
WELLENBAD GPIO5 TX  -> FX GPIO40 RX
WELLENBAD GPIO8 RX  <- FX GPIO39 TX
GND                  -> GND
```

UART: `115200, 8N1`, RTAL WELLENBAD FX protocol v1.

## Clock behavior

WELLENBAD forwards the effective INT or MIDI tempo/clock-source metadata to the FX processor. Build0046l / FX0047 stabilizes synchronized Delay timing by confirming meaningful tempo changes before applying them and crossfading between fixed old/new delay taps.
