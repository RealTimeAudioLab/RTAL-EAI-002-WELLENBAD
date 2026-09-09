#include "RTAL_FX_Link.h"
#include <string.h>

using namespace RTALFxProtocol;

static HardwareSerial gFxSerial(2);
static bool gReady = false;
static bool gConnected = false;
static uint8_t gSeq = 0;
static uint32_t gTxFrames = 0, gRxFrames = 0, gCrcErrors = 0, gVersionErrors = 0, gTimeouts = 0;
static uint32_t gLastRxMs = 0, gLastHelloMs = 0, gLastPingMs = 0, gLastClockMs = 0;
static uint8_t gLastClockSource = 0xFF;
static uint32_t gLastBpmX100 = 0;
static int32_t gRemote[128] = {};
static bool gRemoteValid[128] = {};
static int32_t gDesired[128] = {};
static bool gDesiredValid[128] = {};
static uint8_t gRxBuf[sizeof(Frame)] = {};
static size_t gRxPos = 0;


// Build0054 / A013FXPRE1 -------------------------------------------------
// Persistent FX preset schema. Delay Freeze and read-only LFO diagnostics are
// intentionally excluded. Clock/link metadata are global runtime state, not FX
// preset content.
static const uint32_t FX_PRESET_MAGIC = 0x31584652UL; // 'RFX1'
static const uint8_t FX_PRESET_VERSION = 1;
static const uint8_t gFxPresetParam[RTAL_FX_PRESET_PARAM_COUNT] = {
  P_DELAY_ENABLE, P_DELAY_LEFT_MS, P_DELAY_RIGHT_MS, P_DELAY_FEEDBACK,
  P_DELAY_LEVEL, P_DELAY_LOWPASS_HZ, P_DELAY_HIGHPASS_HZ,
  P_DELAY_SATURATION, P_DELAY_CROSSFEED, P_DELAY_DUCK_AMOUNT,
  P_DELAY_DUCK_THRESHOLD_DB, P_DELAY_DUCK_RELEASE_MS, P_DELAY_MODE,
  P_DELAY_LEFT_DIV, P_DELAY_RIGHT_DIV,
  P_MOD_ENABLE, P_MOD_LFO_SHAPE, P_MOD_RATE_HZ, P_MOD_SYNC, P_MOD_DIVISION,
  P_MOD_DEPTH, P_MOD_STEREO_PHASE_DEG, P_MOD_SMOOTHING_MS, P_MOD_EFFECT_SELECT,
  P_CHORUS_ENABLE, P_CHORUS_MIX, P_CHORUS_DELAY_MS, P_CHORUS_TONE_HZ,
  P_FLANGER_ENABLE, P_FLANGER_MIX, P_FLANGER_DELAY_MS, P_FLANGER_FEEDBACK,
  P_FLANGER_HPF_HZ, P_FLANGER_SATURATION,
  P_PHASER_ENABLE, P_PHASER_MIX, P_PHASER_STAGES, P_PHASER_FEEDBACK,
  P_PHASER_HPF_HZ, P_PHASER_SATURATION, P_PHASER_CENTER_HZ,
  P_WIDTH_ENABLE, P_WIDTH_AMOUNT,
  P_REVERB_ENABLE, P_REVERB_MIX, P_REVERB_SIZE, P_REVERB_DECAY,
  P_REVERB_DAMPING_HZ, P_REVERB_PREDELAY_MS
};

static int32_t fxQ16(float v) {
  return (int32_t)(v * 65536.0f + (v >= 0.0f ? 0.5f : -0.5f));
}

static int32_t fxPresetDefaultRaw(uint8_t p) {
  switch (p) {
    case P_DELAY_ENABLE: return 0;
    case P_DELAY_LEFT_MS: return fxQ16(250.0f);
    case P_DELAY_RIGHT_MS: return fxQ16(375.0f);
    case P_DELAY_FEEDBACK: return fxQ16(0.35f);
    case P_DELAY_LEVEL: return fxQ16(0.25f);
    case P_DELAY_LOWPASS_HZ: return fxQ16(6000.0f);
    case P_DELAY_HIGHPASS_HZ: return fxQ16(20.0f);
    case P_DELAY_SATURATION: return 0;
    case P_DELAY_CROSSFEED: return 0;
    case P_DELAY_DUCK_AMOUNT: return 0;
    case P_DELAY_DUCK_THRESHOLD_DB: return fxQ16(-24.0f);
    case P_DELAY_DUCK_RELEASE_MS: return fxQ16(350.0f);
    case P_DELAY_MODE: return 0;
    case P_DELAY_LEFT_DIV: return 7;
    case P_DELAY_RIGHT_DIV: return 2;
    case P_MOD_ENABLE: return 0;
    case P_MOD_LFO_SHAPE: return 0;
    case P_MOD_RATE_HZ: return fxQ16(0.25f);
    case P_MOD_SYNC: return 0;
    case P_MOD_DIVISION: return 3;
    case P_MOD_DEPTH: return fxQ16(0.50f);
    case P_MOD_STEREO_PHASE_DEG: return fxQ16(90.0f);
    case P_MOD_SMOOTHING_MS: return fxQ16(50.0f);
    case P_MOD_EFFECT_SELECT: return 0;
    case P_CHORUS_ENABLE: return 0;
    case P_CHORUS_MIX: return fxQ16(0.35f);
    case P_CHORUS_DELAY_MS: return fxQ16(16.0f);
    case P_CHORUS_TONE_HZ: return fxQ16(12000.0f);
    case P_FLANGER_ENABLE: return 0;
    case P_FLANGER_MIX: return fxQ16(0.35f);
    case P_FLANGER_DELAY_MS: return fxQ16(2.5f);
    case P_FLANGER_FEEDBACK: return fxQ16(0.35f);
    case P_FLANGER_HPF_HZ: return fxQ16(80.0f);
    case P_FLANGER_SATURATION: return fxQ16(0.12f);
    case P_PHASER_ENABLE: return 0;
    case P_PHASER_MIX: return fxQ16(0.35f);
    case P_PHASER_STAGES: return 4;
    case P_PHASER_FEEDBACK: return fxQ16(0.30f);
    case P_PHASER_HPF_HZ: return fxQ16(80.0f);
    case P_PHASER_SATURATION: return fxQ16(0.08f);
    case P_PHASER_CENTER_HZ: return fxQ16(900.0f);
    case P_WIDTH_ENABLE: return 1;
    case P_WIDTH_AMOUNT: return fxQ16(1.0f);
    case P_REVERB_ENABLE: return 0;
    case P_REVERB_MIX: return fxQ16(0.28f);
    case P_REVERB_SIZE: return fxQ16(0.62f);
    case P_REVERB_DECAY: return fxQ16(0.58f);
    case P_REVERB_DAMPING_HZ: return fxQ16(7200.0f);
    case P_REVERB_PREDELAY_MS: return fxQ16(18.0f);
    default: return 0;
  }
}

static int fxPresetIndexOf(uint8_t p) {
  for (uint8_t i = 0; i < RTAL_FX_PRESET_PARAM_COUNT; ++i)
    if (gFxPresetParam[i] == p) return (int)i;
  return -1;
}

static bool fxPresetPersistentParam(uint8_t p) { return fxPresetIndexOf(p) >= 0; }

static void sendFrame(uint8_t type, uint8_t param, int32_t value);

static void initDesiredFxDefaults() {
  memset(gDesiredValid, 0, sizeof(gDesiredValid));
  for (uint8_t i = 0; i < RTAL_FX_PRESET_PARAM_COUNT; ++i) {
    const uint8_t p = gFxPresetParam[i];
    gDesired[p] = fxPresetDefaultRaw(p);
    gDesiredValid[p] = true;
  }
}

static void replayDesiredFxState() {
  // Freeze is transient and is never replayed after a link reconnect.
  sendFrame(SET_PARAM, P_DELAY_FREEZE, 0);
  uint8_t selected = 0;
  for (uint8_t i = 0; i < RTAL_FX_PRESET_PARAM_COUNT; ++i) {
    const uint8_t p = gFxPresetParam[i];
    const int32_t v = gDesiredValid[p] ? gDesired[p] : fxPresetDefaultRaw(p);
    if (p == P_MOD_ENABLE || p == P_CHORUS_ENABLE || p == P_FLANGER_ENABLE ||
        p == P_PHASER_ENABLE || p == P_MOD_EFFECT_SELECT) {
      if (p == P_MOD_EFFECT_SELECT && v >= 0 && v <= 3) selected = (uint8_t)v;
      continue;
    }
    sendFrame(SET_PARAM, p, v);
  }
  // Normalize the shared Mod-FX engine after reconnect as well.
  sendFrame(SET_PARAM, P_MOD_ENABLE, selected != 0 ? 1 : 0);
  sendFrame(SET_PARAM, P_CHORUS_ENABLE, selected == 1 ? 1 : 0);
  sendFrame(SET_PARAM, P_FLANGER_ENABLE, selected == 2 ? 1 : 0);
  sendFrame(SET_PARAM, P_PHASER_ENABLE, selected == 3 ? 1 : 0);
  sendFrame(SET_PARAM, P_MOD_EFFECT_SELECT, selected);
}

static uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i=0;i<len;++i) {
    crc ^= data[i];
    for (uint8_t b=0;b<8;++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

static void sendFrame(uint8_t type, uint8_t param, int32_t value) {
  Frame f{SOF1,SOF2,VERSION,type,++gSeq,param,value,0};
  const uint8_t *raw = reinterpret_cast<const uint8_t*>(&f);
  f.crc = crc8(raw + 2, sizeof(Frame) - 3);
  gFxSerial.write(reinterpret_cast<const uint8_t*>(&f), sizeof(f));
  ++gTxFrames;
}

// Build0045a: deterministic reset handshake. WELLENBAD is the control master.
static void pushSafeBootFxState() {
  sendFrame(SET_PARAM, P_DELAY_ENABLE, 0);
  sendFrame(SET_PARAM, P_DELAY_FREEZE, 0);
  sendFrame(SET_PARAM, P_MOD_ENABLE, 0);
  sendFrame(SET_PARAM, P_MOD_EFFECT_SELECT, 0);
  sendFrame(SET_PARAM, P_CHORUS_ENABLE, 0);
  sendFrame(SET_PARAM, P_FLANGER_ENABLE, 0);
  sendFrame(SET_PARAM, P_PHASER_ENABLE, 0);
  sendFrame(SET_PARAM, P_REVERB_ENABLE, 0);
  sendFrame(SET_PARAM, P_WIDTH_ENABLE, 1);
  sendFrame(SET_PARAM, P_WIDTH_AMOUNT, 65536); // Q16.16 = 1.0 / 100%
}

static void acceptFrame(const Frame &f) {
  if (f.version != VERSION) { ++gVersionErrors; return; }
  const uint8_t *raw = reinterpret_cast<const uint8_t*>(&f);
  if (crc8(raw + 2, sizeof(Frame) - 3) != f.crc) { ++gCrcErrors; return; }
  ++gRxFrames; gLastRxMs = millis();
  if (f.type == HELLO_ACK || f.type == PONG) gConnected = true;
  if (f.type == HELLO_ACK) {
    gConnected = true;
    pushSafeBootFxState();
    replayDesiredFxState();
    sendFrame(GET_STATE, 0, 0);
    Serial.println(F("FX LINK: connected, safe state + desired preset state pushed, state requested"));
  } else if (f.type == STATE && f.param < 128) {
    gRemote[f.param] = f.value;
    gRemoteValid[f.param] = true;
  }
}

static void parseByte(uint8_t b) {
  if (gRxPos == 0 && b != SOF1) return;
  if (gRxPos == 1 && b != SOF2) { gRxPos = (b == SOF1) ? 1 : 0; if (gRxPos) gRxBuf[0]=SOF1; return; }
  gRxBuf[gRxPos++] = b;
  if (gRxPos == sizeof(Frame)) {
    Frame f; memcpy(&f, gRxBuf, sizeof(f)); gRxPos = 0; acceptFrame(f);
  }
}

void RTALFxLink::begin() {
  memset(gRemoteValid, 0, sizeof(gRemoteValid));
  initDesiredFxDefaults();
  gFxSerial.begin(BAUD, SERIAL_8N1, SYNTH_RX_PIN, SYNTH_TX_PIN);
  gReady = true;
  Serial.print(F("FX LINK: UART2 RX=")); Serial.print(SYNTH_RX_PIN);
  Serial.print(F(" TX=")); Serial.print(SYNTH_TX_PIN);
  Serial.print(F(" baud=")); Serial.println(BAUD);
}

void RTALFxLink::service(uint8_t clockSource, uint16_t internalBpm, uint32_t midiBpmX100) {
  if (!gReady) return;
  uint8_t budget = 32;
  while (budget-- && gFxSerial.available()) { int c=gFxSerial.read(); if(c>=0) parseByte((uint8_t)c); }
  const uint32_t now=millis();
  if (gConnected && (now-gLastRxMs)>3000UL) { gConnected=false; ++gTimeouts; Serial.println(F("FX LINK: timeout")); }
  if (!gConnected && (now-gLastHelloMs)>=500UL) { gLastHelloMs=now; sendFrame(HELLO,0,0x00014041); }
  if (gConnected && (now-gLastPingMs)>=1000UL) { gLastPingMs=now; sendFrame(PING,0,(int32_t)now); }
  uint32_t effectiveBpmX100 = clockSource ? midiBpmX100 : ((uint32_t)internalBpm * 100UL);
  if (gConnected && (clockSource != gLastClockSource || effectiveBpmX100 != gLastBpmX100 || (now-gLastClockMs)>=250UL)) {
    gLastClockMs=now; gLastClockSource=clockSource; gLastBpmX100=effectiveBpmX100;
    sendFrame(CLOCK_STATE, P_CLOCK_SOURCE, clockSource ? 1 : 0);
    sendFrame(CLOCK_STATE, P_CLOCK_BPM_X100, (int32_t)effectiveBpmX100);
  }
}

bool RTALFxLink::connected(){ return gConnected; }
RTALFxLinkStats RTALFxLink::stats(){ return {gConnected,gTxFrames,gRxFrames,gCrcErrors,gVersionErrors,gTimeouts,gLastRxMs?millis()-gLastRxMs:0}; }
void RTALFxLink::requestState(){ if(gReady) sendFrame(GET_STATE,0,0); }
void RTALFxLink::setBool(uint8_t p,bool v){
  const int32_t raw = v ? 1 : 0;
  if (p < 128) { gRemote[p] = raw; gRemoteValid[p] = true; }
  if (p < 128 && fxPresetPersistentParam(p)) { gDesired[p] = raw; gDesiredValid[p] = true; }
  if(gReady) sendFrame(SET_PARAM,p,raw);
}
void RTALFxLink::setInt(uint8_t p,int32_t v){
  if (p < 128) { gRemote[p] = v; gRemoteValid[p] = true; }
  if (p < 128 && fxPresetPersistentParam(p)) { gDesired[p] = v; gDesiredValid[p] = true; }
  if(gReady) sendFrame(SET_PARAM,p,v);
}
void RTALFxLink::setFloat(uint8_t p,float v){
  const int32_t raw = floatToQ16(v);
  if (p < 128) { gRemote[p] = raw; gRemoteValid[p] = true; }
  if (p < 128 && fxPresetPersistentParam(p)) { gDesired[p] = raw; gDesiredValid[p] = true; }
  if(gReady) sendFrame(SET_PARAM,p,raw);
}

// A011_FIX1: enforce the shared Mod-FX core invariant in one place.
// Exactly one of Chorus/Flanger/Phaser may be enabled, or all are OFF.
void RTALFxLink::selectModEffect(uint8_t effect){
  if (effect > 3) effect = 0;
  // A014A_FIX1: the shared modulation engine follows Effect Select.
  // OFF => engine OFF. CHORUS/FLANGER/PHASER => engine ON automatically.
  setBool(P_MOD_ENABLE, effect != 0);
  setBool(P_CHORUS_ENABLE, effect == 1);
  setBool(P_FLANGER_ENABLE, effect == 2);
  setBool(P_PHASER_ENABLE, effect == 3);
  setInt(P_MOD_EFFECT_SELECT, effect);
}

void RTALFxLink::setModEffectEnabled(uint8_t effect, bool enabled){
  if (effect < 1 || effect > 3) return;
  if (enabled) { selectModEffect(effect); return; }
  const uint8_t enableParam = (effect == 1) ? P_CHORUS_ENABLE : (effect == 2) ? P_FLANGER_ENABLE : P_PHASER_ENABLE;
  setBool(enableParam, false);
  int32_t selected = 0;
  if (getRemoteValue(P_MOD_EFFECT_SELECT, selected) && selected == effect) selectModEffect(0);
}
bool RTALFxLink::getRemoteValue(uint8_t p,int32_t &v){ if(p>=128||!gRemoteValid[p])return false; v=gRemote[p]; return true; }

void RTALFxLink::capturePresetState(RTALFxPresetState &state){
  memset(&state, 0, sizeof(state));
  state.magic = FX_PRESET_MAGIC;
  state.version = FX_PRESET_VERSION;
  state.count = RTAL_FX_PRESET_PARAM_COUNT;
  for (uint8_t i = 0; i < RTAL_FX_PRESET_PARAM_COUNT; ++i) {
    const uint8_t p = gFxPresetParam[i];
    int32_t v = 0;
    if (gDesiredValid[p]) state.value[i] = gDesired[p];
    else state.value[i] = getRemoteValue(p, v) ? v : fxPresetDefaultRaw(p);
  }
  // Freeze is deliberately not stored. The delay buffer itself is not preset data.
}

bool RTALFxLink::presetStateValid(const RTALFxPresetState &state){
  return state.magic == FX_PRESET_MAGIC && state.version == FX_PRESET_VERSION &&
         state.count == RTAL_FX_PRESET_PARAM_COUNT;
}

void RTALFxLink::applyPresetState(const RTALFxPresetState &state){
  if (!presetStateValid(state)) { applyPresetDefaults(); return; }

  // A preset never restores a stale frozen audio buffer.
  setBool(P_DELAY_FREEZE, false);

  // Apply all persistent parameters except the three mutually-exclusive enable
  // flags and Effect Select. Those four are normalized in one final step below.
  for (uint8_t i = 0; i < RTAL_FX_PRESET_PARAM_COUNT; ++i) {
    const uint8_t p = gFxPresetParam[i];
    if (p == P_MOD_ENABLE || p == P_CHORUS_ENABLE || p == P_FLANGER_ENABLE ||
        p == P_PHASER_ENABLE || p == P_MOD_EFFECT_SELECT) continue;
    setInt(p, state.value[i]);
  }

  int32_t selected = 0;
  const int selIndex = fxPresetIndexOf(P_MOD_EFFECT_SELECT);
  if (selIndex >= 0) selected = state.value[selIndex];
  if (selected < 0 || selected > 3) selected = 0;
  selectModEffect((uint8_t)selected);
  requestState();
}

void RTALFxLink::makePresetDefaults(RTALFxPresetState &state){
  memset(&state, 0, sizeof(state));
  state.magic = FX_PRESET_MAGIC;
  state.version = FX_PRESET_VERSION;
  state.count = RTAL_FX_PRESET_PARAM_COUNT;
  for (uint8_t i = 0; i < RTAL_FX_PRESET_PARAM_COUNT; ++i)
    state.value[i] = fxPresetDefaultRaw(gFxPresetParam[i]);
}

void RTALFxLink::applyPresetDefaults(){
  RTALFxPresetState state;
  makePresetDefaults(state);
  applyPresetState(state);
}

float RTALFxLink::q16ToFloat(int32_t v){ return (float)v/65536.0f; }
int32_t RTALFxLink::floatToQ16(float v){ return (int32_t)(v*65536.0f + (v>=0?0.5f:-0.5f)); }
