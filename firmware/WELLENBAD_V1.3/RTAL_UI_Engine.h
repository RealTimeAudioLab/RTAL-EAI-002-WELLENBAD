#pragma once
#include <Arduino.h>

#ifndef RTAL_UI_ENGINE_ENABLED
#define RTAL_UI_ENGINE_ENABLED 0
#endif
#ifndef RTAL_UI_ENGINE_DEFAULT_ACTIVE
#define RTAL_UI_ENGINE_DEFAULT_ACTIVE 0
#endif

#if RTAL_UI_ENGINE_ENABLED

enum class RtalScreenId : uint16_t {
  PlayHome   = 1,
  SoundOsc1  = 100,
  SoundOsc2  = 101,
  SoundMixer = 102,
  SoundNoise = 103,
  SoundSub   = 104,
  SoundOutput= 105,
  SoundWaveMon=106,

  EditFilter   = 200,
  EditAmpEnv   = 201,
  EditFilterEnv= 202,
  EditWaveEnv  = 203,

  ModLfo         = 300,
  ModWaveLfo     = 301,
  ModPerformance = 302,
  ModMorph       = 303,

  FxMain          = 400,

  PlayPerformance = 10,
  PlayArpeggiator = 11,
  PlaySequencer   = 12,
  PlaySeqEdit     = 13,
  PlaySeqShow     = 14,
  PlayMultiOverview = 15,
  PlayMultiPart     = 16,
  PlayMultiStorage  = 17,

  SystemMidi      = 700,
  SystemInfo      = 701,
  SystemAudio     = 702,
  SystemDiagnostics = 703,
  SystemStorage   = 704,
  SystemAbout     = 705
};

void rtalUiBegin();
bool rtalUiIsActive();
void rtalUiToggle();
void rtalUiDraw();
bool rtalUiHandleButton(uint8_t buttonIndex, bool longPress);
bool rtalUiHandleEncoder(int8_t delta);
bool rtalUiHandleEncoderPress(bool longPress);
void rtalUiShowMessage(const char* text, uint16_t durationMs = 900);
void rtalUiRunAudit();
uint8_t rtalUiSelectedMultiPart();

#else
inline void rtalUiBegin() {}
inline bool rtalUiIsActive() { return false; }
inline void rtalUiToggle() {}
inline void rtalUiDraw() {}
inline bool rtalUiHandleButton(uint8_t, bool) { return false; }
inline bool rtalUiHandleEncoder(int8_t) { return false; }
inline bool rtalUiHandleEncoderPress(bool) { return false; }
inline void rtalUiShowMessage(const char*, uint16_t = 900) {}
inline void rtalUiRunAudit() {}
inline uint8_t rtalUiSelectedMultiPart() { return 0; }
#endif
