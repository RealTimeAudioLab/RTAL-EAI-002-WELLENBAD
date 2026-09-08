# FX0047 v1.0.0 Final — Release Test Checklist

Use this checklist on the final hardware pair.

- [ ] FX0047 compiles in the established AudioDSP toolchain.
- [ ] Boot reports Version 1.0.0 / Build FX0047.
- [ ] WELLENBAD v1.5.0 establishes FX LINK.
- [ ] I2S audio is clean with all FX OFF.
- [ ] Delay FREE mode works.
- [ ] Delay SYNC mode locks to steady MIDI Clock.
- [ ] Change MIDI tempo by >2 BPM: new delay timing is adopted cleanly after confirmation.
- [ ] No Doppler/flanger sweep during sync-time transition.
- [ ] Freeze remains transparent when engaged/released.
- [ ] Chorus works without dropout.
- [ ] Flanger works without dropout.
- [ ] Phaser works without dropout.
- [ ] Reverb works without dropout.
- [ ] Stereo Width behaves correctly.
- [ ] Combined Delay + Flanger + Reverb stress test is clean.
- [ ] No I2S errors/timeouts.
- [ ] No watchdog recovery.
- [ ] Preset/state control from WELLENBAD works.
