# WB0074 – MIDI PC DEFER FIX2 RT SAFE

## Fehlerbild in B0072

MIDI Program Change im MULTI Performance Mode konnte einen ESP32-S3 Panic auslösen:

```text
Stack canary watchpoint triggered (MidiTask)
```

## Ursache

`handlePC()` lief als Callback im hochpriorisierten `MidiTask` mit 4096 Byte Stack.
Im MULTI-Modus rief der Callback synchron `safeLoadMultiSetup()` und anschließend
`loadMultiSetupFromSD()` auf. Der Multi-Loader besitzt große lokale Datei-/Part-/FX-
Strukturen und einen tiefen SD/FAT-Aufrufpfad. Dieser seltene Call-Tree kann den
MidiTask-Stack überschreiten. Außerdem gehört blockierende SD-Arbeit nicht in den
MIDI-Realtime-Parser.

## Fix

- `handlePC()` führt keinen Program-/Multi-Load mehr aus.
- Der Callback legt nur ein 2-Byte-Ereignis (`program`, `multiMode`) in eine eigene Queue.
- `ControlTask` verarbeitet maximal einen Program Change pro Service-Pass.
- Program-/Multi-Load läuft dadurch auf dem bewährten 8192-Byte-ControlTask-Stack.
- MidiTask bleibt bei 4096 Byte und frei von SD-/Multi-Load-Arbeit.
- Audio-, Filter-, Voice-, MULTI-DSP- und CDC-SMART-SAFE-Code bleiben unverändert.

## Test

1. B0074 flashen.
2. SINGLE: PC 0, 1, 9, 10, 11, 29, 30, 127 senden.
3. Auf MULTI umschalten.
4. PC 0..10 langsam einzeln senden.
5. Danach PC 0..10 zügig senden.
6. Währenddessen MIDI Clock F8 dauerhaft anliegen lassen; Start/Stop testen.
7. 7 Voices SINGLE und MULTI gegenprüfen.
8. Editor verbinden und Multi-PC erneut testen.

Erwartung: kein Stack-Canary, kein Reboot, MIDI Clock bleibt stabil, Multi wechselt korrekt.
