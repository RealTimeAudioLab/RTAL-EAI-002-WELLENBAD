# RTAL WELLENBAD ↔ FX Protocol v1

Fixed frame, 11 bytes:

`52 46 | version | type | seq | param | value[4 little-endian] | CRC8`

CRC8 polynomial 0x07 covers `version..value`.

Types: HELLO, HELLO_ACK, PING, PONG, SET_PARAM, GET_STATE, STATE, CLOCK_STATE.
Float parameters use signed Q16.16. Tempo uses BPM x100.

The UART protocol is deliberately separate from I2S. Loss of the control link
must never stop the FX audio path.


## Build0039 modulation-core extension

Parameter IDs 64..73 are reserved for the shared RTAL modulation core: Enable, LFO Shape, Free Rate, Sync, Division, Depth, Stereo Phase, Smoothing, LFO Left diagnostic and LFO Right diagnostic. The last two are read-only STATE values. Protocol frame version remains 1.


## Build0041 Stereo Chorus parameters

| ID | Parameter | Encoding | Range |
|---:|---|---|---|
| 80 | Chorus Enable | int | 0/1 |
| 81 | Chorus Mix | Q16.16 | 0..1 |
| 82 | Chorus Base Delay | Q16.16 ms | 8..24 ms |
| 83 | Chorus Tone | Q16.16 Hz | 2000..18000 Hz |

Chorus modulation uses the shared ModCore parameters 64..73. The DSP chain in Build0041 is Delay -> Stereo Chorus -> Limiter.

## Build0042 additions – Stereo Flanger
Parameter IDs 96..101:
- 96 Enable (bool)
- 97 Mix (Q16.16 0..1)
- 98 Base Delay ms (Q16.16 0.20..12.0)
- 99 Feedback (Q16.16 -0.95..+0.95)
- 100 Feedback HPF Hz (Q16.16 20..2000)
- 101 Saturation (Q16.16 0..1)

Chorus and Flanger are mutually exclusive on the FX processor. Both use the shared RTAL ModCore (IDs 64..73).
