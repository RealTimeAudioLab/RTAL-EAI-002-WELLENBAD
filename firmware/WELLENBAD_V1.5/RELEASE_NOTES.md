# RTAL WELLENBAD v1.5.0 FINAL

## Release identity

- Firmware: **v1.5.0**
- Build: **B0074F**
- Series: **A015**
- Status: **FINAL**
- Companion FX: **RTAL AudioDSP FX v1.0.0 / FX0047 FINAL**
- Final validation date: **09.09.2026**

## Final hardware-validation fixes

### 1. MULTI MIDI Program Change stack safety

Final testing found that loading a complete Multi directly from the high-priority MidiTask could exceed its 4096-byte stack. Program Change is now deferred: the MidiTask stores only a tiny pending request and the ControlTask performs Program/Multi loading. Rapid requests use a latest-request-wins policy.

### 2. Seven-voice MULTI real-time reserve

The initial deferred implementation used permanent FreeRTOS queue polling in ControlTask and reduced the already narrow seven-voice MULTI reserve. B0074F uses a lightweight pending mailbox instead, preserving the validated seven-voice configuration.

### 3. HOLD Arpeggiators with Part Enable/Mute

Performance Disable/Mute now silences a Part without erasing its HOLD note pool. Re-enabling can resume the held Arpeggiator. Structural operations still perform a full reset.

### 4. Editor ENABLE/DISABLE/MUTE synchronization

The WebSerial command path now returns explicit success/failure. Part toggle state remains pending until confirmed by WELLENBAD; there is no arbitrary timeout, and stale Multi-state packets cannot overwrite a newer pending toggle while SMART SAFE suppresses large state traffic.

## Validated scenarios

- SINGLE: seven voices
- MULTI: seven voices
- MULTI Program Change without MidiTask stack crash
- repeated Multi Program Changes
- several Parts with ARP HOLD active
- sequential DISABLE of all Parts
- sequential ENABLE of all Parts
- second sequential DISABLE after re-enable
- editor indication remains aligned with audible Part state
- companion AudioDSP FX v1.0.0 / FX0047 operation retained

## Release recommendation

B0074F supersedes B0072/B0073/B0073F/B0074 as the public WELLENBAD v1.5.0 release build.
