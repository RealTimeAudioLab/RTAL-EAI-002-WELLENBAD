# RTAL WELLENBAD v1.5.0 Final — Release Notes

## Final Release

Build B0072 ist die Final-Freigabe der v1.5.0-Linie und basiert funktional unverändert auf WB0071 RC1.

## Bestätigter stabiler Basisstand

- 7 Voices in SINGLE ohne hörbare Verzerrung.
- 7 Voices in MULTI ohne hörbare Verzerrung.
- 7 Voices mit verbundenem Editor im SMART-SAFE-Betrieb.
- Die zuvor beobachteten hörbaren >10-ms-CDC-Stalls wurden durch die SMART-SAFE-Architektur beseitigt.
- Der Editor blendet die unerwünschte V0-V7-Anzeige aus.
- 4-Pol FAST32 Biquad bleibt unverändert.
- Fast Poly bleibt deaktiviert.

## Änderungen von RC1 zu Final

- Firmwarekennung: `v1.5.0 B0072 A015 FINAL`.
- Sketch-Ordner/Hauptdatei: `WB0072/WB0072.ino`.
- Offizieller Editor auf B0072 aktualisiert.
- RC-spezifische Hinweise und Checklisten aus dem eigentlichen Final-Paket entfernt.
- Final README, Release Notes und SHA256-Manifest ergänzt.

## Keine funktionalen DSP-Änderungen

Zwischen WB0071 RC1 und B0072 Final wurden keine Audio-, Voice-, Filter-, MULTI- oder CDC-Algorithmen verändert.
