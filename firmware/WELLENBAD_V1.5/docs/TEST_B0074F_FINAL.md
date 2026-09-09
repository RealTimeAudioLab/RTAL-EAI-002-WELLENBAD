# B0074F final validation

Validated on target WELLENBAD hardware before public v1.5.0 freeze.

## Core release validation

- 7 voices in SINGLE: pass
- 7 voices in MULTI: pass
- MIDI Program Change in MULTI without MidiTask stack panic: pass
- repeated MULTI Program Change: pass
- multiple Parts with ARP HOLD active: pass
- sequential DISABLE of all active HOLD Parts: pass
- sequential ENABLE of all Parts: pass
- all Parts audible again after ENABLE: pass
- editor shows ENABLED state consistently: pass
- second sequential DISABLE after re-enable: pass
- editor state remains aligned with audible state: pass

## Relevant final fixes

1. MIDI Program Change is deferred out of MidiTask to ControlTask.
2. Lightweight pending mailbox replaces permanent FreeRTOS queue polling to preserve 7-voice MULTI headroom.
3. Performance Disable/Mute preserves ARP HOLD note pools.
4. Editor toggle writes return explicit success and use confirmation-based pending state without arbitrary timeout.
5. Stale Multi-state packets do not overwrite newer pending toggle state.
