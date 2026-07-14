#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <MIDI.h>
#include <Preferences.h>
#include <BfButton.h>
#include <driver/i2s.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <FS.h>
#include <SD.h>
#include "Wellenbad_Wavetables.h"

// ================================================================
// Firmware release identity
// ================================================================
#define FW_NAME       "WELLENBAD"
#define FW_VERSION    "v1.0"
#define FW_BUILD_DATE __DATE__
#define FW_BUILD_TIME __TIME__
#define FW_BANK_KEY "bankInit9162"

// NVS diagnostics
uint32_t bootCounter = 0;
volatile uint32_t presetCrcErrors = 0;

struct Voice;
struct SmoothParam;
struct StereoSample;

static inline void smoothUpdate(SmoothParam &p, uint8_t target, uint8_t speedShift);
static inline void updateSmoothParams();
static inline void updateGlide(Voice &v);
static inline int32_t processAmpEnv(Voice &v);
static inline int32_t processFilterEnv(Voice &v);
static inline int32_t processWaveEnv(Voice &v);
static inline int16_t fastNoise(Voice &v);
static inline int16_t oscRead(Voice &v);
static inline int16_t ppgFilter(Voice &v, int16_t input);
static inline int16_t renderVoice(Voice &v);
static inline StereoSample renderVoiceStereo(Voice &v);
void envNoteOn(Voice &v);
void envNoteOff(Voice &v);
void handleSPP(unsigned int position);
void midiChannelBootMenu();
void drawMidiChannelBootScreen(uint8_t ch);

// ================================================================
// Pins
// ================================================================
#define MIDI_RX_PIN 40
#define MIDI_TX_PIN 39
#define I2S_BCLK    18
#define I2S_LRCK    16
#define I2S_DOUT    17
#define OLED_SCK    12
#define OLED_MOSI   11
#define OLED_CS     10
#define OLED_DC     6
#define OLED_RST    7
#define BTN_1_PIN   21
#define BTN_2_PIN   47
#define BTN_3_PIN   45
#define BTN_4_PIN   38
#define BTN_5_PIN   4
#define BTN_6_PIN   15
#define BTN_7_PIN   3
#define BTN_8_PIN   14
#define ENC_A_PIN   1
#define ENC_B_PIN   2
#define ENC_SW_PIN  42
#define SD_MISO_PIN 13
#define SD_CS_PIN   9

// ================================================================
// System constants
// ================================================================
#define SAMPLE_RATE 44100
#define AUDIO_BLOCK 128
#define NUM_VOICES 6
#define NUM_PROGRAMS 128
#define PROGRAM_NAME_LEN 16
#define NUM_WAVES 64
#define WT_SIZE 256
#define WT_MASK 255
#define CHORUS_SIZE 1024
#define CHORUS_MASK 1023
#define ARP_MAX_NOTES 16
#define PB_RANGES 13
#define SD_WT_MAX_TABLES 16
#define SD_WT_DIR "/PPGWT"

// Visible wavetable slots:
// 0..126 are the 127 selectable table slots.
// SD wavetables override these slots from the back:
// SD0 -> slot 126, SD1 -> slot 125, SD2 -> slot 124, ...
#define WT_VISIBLE_SLOTS 127
#define WT_LAST_SLOT     (WT_VISIBLE_SLOTS - 1)
#define WT_FLASH_SLOTS   WT_VISIBLE_SLOTS

#define SD_PRESET_DIR "/PPGPRESETS"
#define SD_BACKUP_DIR "/PPGBACKUP"
#define SD_META_DIR   "/PPGMETA"
#define SD_PRESET_MAGIC 0x31504750UL  // 'PPG1'
#define SD_BANK_MAGIC   0x314B4250UL  // 'PBK1'
#define SD_PRESET_VERSION 2
#define SD_BANK_VERSION   2
#define SD_USER_PROGRAM_MAGIC 0x31555050UL  // 'PPU1'
#define SD_USER_PROGRAM_VERSION 2
#define SD_WT_FILE_SIZE_64  (64UL * PPG_WT_SAMPLES)
#define SD_WT_FILE_SIZE_128 (128UL * PPG_WT_SAMPLES)
#define SD_WT_FILE_SIZE     SD_WT_FILE_SIZE_64

// ================================================================
/// Polyphony/CPU optimization switches
// ================================================================
#define PPG_POLY_FAST_THRESHOLD 5
#define PPG_ENABLE_FAST_POLY 1
#define PPG_DYNAMIC_HEADROOM 1
#define PPG_DISABLE_CHORUS_HIGH_POLY 1

#ifndef EXT_RAM_ATTR
#define EXT_RAM_ATTR
#endif

// ================================================================
// MIDI / OLED / Storage
// ================================================================
HardwareSerial SerialMIDI(1);
MIDI_CREATE_INSTANCE(HardwareSerial, SerialMIDI, MIDI);

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
Preferences prefs;

void setMidiCcModeGlobal(uint8_t v, bool store);
void setMidiCcBankGlobal(uint8_t v, bool store);
void setLfoShapeGlobal(uint8_t v, bool store);
void setSeqTableModeGlobal(uint8_t v, bool store);
void setClockSourceGlobal(uint8_t v, bool store);
bool saveSystemConfigToSD();
bool loadSystemConfigFromSD();

// ================================================================
// Parameters
// ================================================================
enum ParamId {
  P_WAVETABLE,
  P_WAVE_POS,
  P_WAVE_MOD,
  P_OSC_MIX,
  P_OSC_DETUNE,
  P_OSC_B_OFFSET,
  P_CUTOFF,
  P_RESONANCE,
  P_FILTER_ENV,
  P_ATTACK,
  P_DECAY,
  P_SUSTAIN,
  P_RELEASE,
  P_F_ATTACK,
  P_F_DECAY,
  P_F_SUSTAIN,
  P_F_RELEASE,
  P_WAVE_ENV,
  P_WAVE_ENV_ATTACK,
  P_WAVE_ENV_DECAY,
  P_WAVE_ENV_SUSTAIN,
  P_WAVE_ENV_RELEASE,
  P_LFO_RATE,
  P_LFO_AMOUNT,
  P_LFO_TARGET,
  P_WAVE_LFO_RATE,
  P_WAVE_LFO_AMOUNT,
  P_AFTERTOUCH_WAVE,
  P_AFTERTOUCH_FILTER,
  P_VEL_AMP,
  P_VEL_FILTER,
  P_KEYTRACK,
  P_PAN_SPREAD,
  P_DRIVE,
  P_BITCRUSH,
  P_VOLUME,
  P_PLAY_MODE,
  P_GLIDE,
  P_BEND_RANGE,
  P_UNISON_DETUNE,
  P_SUB_LEVEL,
  P_NOISE_LEVEL,
  P_CHORUS,
  P_ARP_MODE,
  P_ARP_RATE,
  P_ARP_OCTAVES,
  P_ARP_HOLD,
  P_CLOCK_SOURCE,
  P_TAP_TEMPO,
  P_SEQ_MODE,
  P_SEQ_RATE,
  P_SEQ_STEPS,
  P_SEQ_TARGET,
  P_SEQ_DEPTH,
  P_MORPH_AMOUNT,
  P_RANDOMIZE,
  PARAM_COUNT
};

#define UI_PARAM_NONE    255
#define UI_PARAM_CC_MODE 254
#define UI_PARAM_SEQ_STEP 253
#define UI_PARAM_SEQ_VALUE 252
#define UI_PARAM_SEQ_TABLE_MODE 251
#define UI_PARAM_LFO_SHAPE 250
#define UI_PARAMS_PER_PAGE 8

// ================================================================
// SD global diagnostics and forward declarations
// Must appear before resetDiagnostics() and handleCC() for Arduino IDE 1.8.x.
// ================================================================
#define SYS_CONFIG_FILE "/PPGMETA/CONFIG.TXT"
#ifndef SD_PRESET_DIR
#define SD_PRESET_DIR "/PPGPRESETS"
#endif
#ifndef SD_BACKUP_DIR
#define SD_BACKUP_DIR "/PPGBACKUP"
#endif
#ifndef SD_META_DIR
#define SD_META_DIR   "/PPGMETA"
#endif
#ifndef SD_PRESET_MAGIC
#define SD_PRESET_MAGIC 0x31504750UL
#endif
#ifndef SD_BANK_MAGIC
#define SD_BANK_MAGIC   0x314B4250UL
#endif
#ifndef SD_PRESET_VERSION
#define SD_PRESET_VERSION 1
#endif
#ifndef SD_BANK_VERSION
#define SD_BANK_VERSION 1
#endif

// SD wavetable diagnostics. If the SD-wavetable loader also declares this later,
// keep only this early declaration.
volatile uint32_t sdWavetableLoadErrors = 0;

// SD preset/bank diagnostics.
volatile uint32_t sdPresetLoadErrors = 0;
volatile uint32_t sdPresetSaveErrors = 0;
volatile uint32_t sdBankLoadErrors = 0;
volatile uint32_t sdBankSaveErrors = 0;

// Preset metadata used by SD shortcuts.
uint8_t morphSnapshotA[PARAM_COUNT];
uint8_t morphSnapshotB[PARAM_COUNT];

// SD preset/bank API.
bool saveCurrentPresetToSD(const char *name, uint8_t category);
bool loadFirstPresetFromSD();
bool saveBankToSD(const char *filename);
bool loadFirstBankFromSD();
void ensureSdDirectories();

const char* paramNames[PARAM_COUNT] = {
  "Table", "Wave Pos", "Wave Mod",
  "Osc Mix", "Osc Det", "OscB Off",
  "Cutoff", "Resonance", "Filt Env",
  "Attack", "Decay", "Sustain", "Release",
  "F Attack", "F Decay", "F Sustain", "F Release",
  "Wave Env", "W Attack", "W Decay", "W Sustain", "W Release",
  "LFO Rate", "LFO Amt", "LFO Target",
  "W LFO Rate", "W LFO Amt",
  "AT Wave", "AT Filter",
  "Vel Amp", "Vel Filt", "Keytrack", "Pan Spread",
  "Drive", "Bitcrush", "Volume",
  "Play Mode", "Glide", "BendRange", "Unison Det", "Sub Level", "Noise", "Chorus",
  "Arp Mode", "Arp Rate", "Arp Oct", "Arp Hold", "Clock Src", "Tempo",
  "Seq Mode", "Seq Rate", "Seq Steps", "Seq Target", "Seq Depth",
  "Morph", "Randomize"
};

enum EnvStage { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
enum LfoTarget { LFO_TO_WAVE, LFO_TO_PITCH, LFO_TO_FILTER, LFO_TO_AMP, LFO_TO_PAN };

// ================================================================
// Program and audio params
// ================================================================
struct ProgramV1 {
  char name[PROGRAM_NAME_LEN];
  uint8_t param[PARAM_COUNT];
  uint8_t ccMap[PARAM_COUNT];
};

struct Program {
  char name[PROGRAM_NAME_LEN];
  uint8_t param[PARAM_COUNT];
  uint8_t ccMap[PARAM_COUNT];
  uint8_t lfoShape;  // Program V2 extension. 0=TRI, 1=SINE, 2=SAW UP, 3=SAW DN, 4=SQUARE, 5=RANDOM
};

struct AudioParams {
  uint8_t p[PARAM_COUNT];
};

Program currentProgram;
Program compareOriginalProgram;
Program compareEditProgram;
bool compareAvailable = false;
bool compareMode = false;
uint32_t compareMessageUntil = 0;
AudioParams audioParams;
AudioParams audioParamsSeq;  // audio-path mirror including temporary non-destructive SEQ modulation


struct AudioSnapshot {
  uint8_t wavetable;
  uint8_t waveEnv;
  uint8_t waveLfoRate;
  uint8_t waveLfoAmount;
  uint8_t waveMod;
  uint8_t aftertouchWave;
  uint8_t lfoRate;
  uint8_t lfoTarget;
  uint8_t lfoShape;
  uint8_t lfoAmount;
  uint8_t playMode;
  uint8_t oscDetune;
  uint8_t unisonDetune;
  uint8_t oscMix;
  uint8_t oscBOffset;
  uint8_t subLevel;
  uint8_t noiseLevel;
  uint8_t glide;
};

uint8_t currentProgramNumber = 0;

volatile bool paramsDirty = false;
portMUX_TYPE paramMux = portMUX_INITIALIZER_UNLOCKED;

// Non-destructive Mod Sequencer state.
// Must be declared before getAParamSeq(), because getAParamSeq() is defined early.
volatile int8_t modSeqOffset = 0;
volatile uint8_t modSeqCurrentTarget = P_WAVE_POS;
volatile bool modSeqAbsoluteTableActive = false;
volatile uint8_t modSeqAbsoluteTableValue = 0;
volatile uint8_t modSeqMirrorLastTarget = P_WAVE_POS;
static inline uint8_t getAParam(uint8_t id) {
  if (id >= PARAM_COUNT) return 0;
  return audioParams.p[id];
}


static inline uint8_t getAParamSeq(uint8_t id) {
  if (id >= PARAM_COUNT) return 0;
  // Fast audio-loop path: one array read only.
  // The temporary sequencer modulation is pre-applied to audioParamsSeq
  // in updateModSeqAudioMirror(), outside the per-sample render path.
  return audioParamsSeq.p[id];
}

uint8_t globalLfoShape = 0;     // Global LFO shape: 0=TRI, 1=SINE, 2=SAW UP, 3=SAW DN, 4=SQUARE, 5=RANDOM.
static inline void readAudioSnapshot(AudioSnapshot &a) {
  // Snapshot all frequently used oscillator parameters once at the beginning
  // of oscRead(). The audio path then uses local values instead of repeatedly
  // calling getAParamSeq().
  a.wavetable       = getAParamSeq(P_WAVETABLE);
  a.waveEnv         = getAParamSeq(P_WAVE_ENV);
  a.waveLfoRate     = getAParamSeq(P_WAVE_LFO_RATE);
  a.waveLfoAmount   = getAParamSeq(P_WAVE_LFO_AMOUNT);
  a.waveMod         = getAParamSeq(P_WAVE_MOD);
  a.aftertouchWave  = getAParamSeq(P_AFTERTOUCH_WAVE);
  a.lfoRate         = getAParamSeq(P_LFO_RATE);
  a.lfoTarget       = getAParamSeq(P_LFO_TARGET);
  a.lfoShape        = globalLfoShape;
  a.lfoAmount       = getAParamSeq(P_LFO_AMOUNT);
  a.playMode        = getAParamSeq(P_PLAY_MODE);
  a.oscDetune       = getAParamSeq(P_OSC_DETUNE);
  a.unisonDetune    = getAParamSeq(P_UNISON_DETUNE);
  a.oscMix          = getAParamSeq(P_OSC_MIX);
  a.oscBOffset      = getAParamSeq(P_OSC_B_OFFSET);
  a.subLevel        = getAParamSeq(P_SUB_LEVEL);
  a.noiseLevel      = getAParamSeq(P_NOISE_LEVEL);
  a.glide           = getAParamSeq(P_GLIDE);
}

static inline uint8_t selectedWavetableIndexFromSnapshot(const AudioSnapshot &a) {
  uint8_t v = a.wavetable;
  if (v >= WT_VISIBLE_SLOTS) v = WT_LAST_SLOT;
  return v;
}


static inline bool isSwitchParam(uint8_t id) {
  return id == P_ARP_HOLD;
}

void arpAllNotesOff();
void setParam(uint8_t id, uint8_t value);
void setClockSourceGlobal(uint8_t v, bool store);
bool ensureSdWavetableCached(uint8_t sd);
void requestSdWavetableCache(uint8_t sd);
void processSdWavetableCacheRequest();
bool loadSdWavetableIntoCache(uint8_t sd, uint8_t cacheIndex);

// ================================================================
// Voice
// ================================================================
struct Voice {
  bool active;
  uint8_t note;
  uint8_t velocity;
  uint32_t age;
  uint32_t noteOnMillis;
  uint32_t phaseA, phaseB;
  uint32_t incA, incB;
  uint32_t targetInc;
  int32_t ampEnv;
  uint8_t ampStage;
  int32_t filtEnv;
  uint8_t filtStage;
  int32_t waveEnv;
  uint8_t waveStage;
  uint16_t lfoPhase;
  uint16_t waveLfoPhase;
  uint16_t lfoRandomLastPhase;
  int16_t lfoRandomValue;
  int32_t svfLow;
  int32_t svfBand;
  int16_t pan;
  uint32_t noiseSeed;
  int8_t drift;
  bool sustained;

};

Voice voices[NUM_VOICES];
uint32_t voiceAgeCounter = 0;
int8_t monoVoice = 0;
uint32_t lastPlayedIncForPolyGlide = 0;
bool lastPlayedIncValid = false;
uint8_t heldNotes[16];
uint8_t heldCount = 0;
volatile bool sustainPedal = false;

enum AudioEventType : uint8_t {
  AE_NOTE_ON = 1,
  AE_NOTE_OFF = 2,
  AE_ALL_NOTES_OFF = 3,
  AE_SUSTAIN_RELEASE = 4
};

struct AudioEvent {
  uint8_t type;
  uint8_t note;
  uint8_t velocity;
};

QueueHandle_t audioEventQueue = NULL;

// Runtime diagnostics. Read on Core 0, written mostly on Core 1.
volatile uint32_t audioBlocksRendered = 0;
volatile uint32_t audioMaxBlockMicros = 0;
volatile uint32_t audioLastBlockMicros = 0;
volatile uint32_t i2sShortWrites = 0;
volatile uint32_t audioQueueDrops = 0;
volatile uint32_t limiterHits = 0;
volatile uint32_t hardClipHits = 0;
volatile uint32_t voiceSteals = 0;
volatile uint8_t activeVoicesLast = 0;
volatile uint8_t audioPolyLoad = 0;
volatile uint32_t highPolyFastSamples = 0;
volatile uint32_t programChangeMuteUntil = 0;
volatile uint32_t programChangeCount = 0;
volatile uint32_t panicCount = 0;
volatile uint32_t midiClockTimeouts = 0;
volatile uint32_t stuckVoiceResets = 0;
volatile uint32_t audioQueueMaxDepth = 0;

static inline void resetDiagnostics() {
  audioBlocksRendered = 0;
  audioMaxBlockMicros = 0;
  audioLastBlockMicros = 0;
  i2sShortWrites = 0;
  audioQueueDrops = 0;
  limiterHits = 0;
  hardClipHits = 0;
  voiceSteals = 0;
  highPolyFastSamples = 0;
  programChangeCount = 0;
  panicCount = 0;
  midiClockTimeouts = 0;
  stuckVoiceResets = 0;
  audioQueueMaxDepth = 0;
  presetCrcErrors = 0;
  sdWavetableLoadErrors = 0;
  sdPresetLoadErrors = 0;
  sdPresetSaveErrors = 0;
  sdBankLoadErrors = 0;
  sdBankSaveErrors = 0;
}

static inline void queueAudioEvent(uint8_t type, uint8_t note = 0, uint8_t velocity = 0) {
  if (!audioEventQueue) {
    audioQueueDrops++;
    return;
  }
  UBaseType_t waiting = uxQueueMessagesWaiting(audioEventQueue);
  if (waiting > audioQueueMaxDepth) audioQueueMaxDepth = waiting;
  AudioEvent ev = { type, note, velocity };
  if (xQueueSend(audioEventQueue, &ev, 0) != pdTRUE) audioQueueDrops++;
}


static inline void queuePanicForProgramChange() {
  if (!audioEventQueue) {
    audioQueueDrops++;
    return;
  }
  xQueueReset(audioEventQueue);
  AudioEvent ev = { AE_ALL_NOTES_OFF, 0, 0 };
  if (xQueueSend(audioEventQueue, &ev, 0) != pdTRUE) audioQueueDrops++;
}

// ================================================================
// Tables
// ================================================================
EXT_RAM_ATTR int16_t chorusBufL[CHORUS_SIZE];
EXT_RAM_ATTR int16_t chorusBufR[CHORUS_SIZE];

uint32_t notePhaseInc[128];
uint16_t pitchBendTables[PB_RANGES][256];
uint16_t lfoRateTable[128];
uint16_t waveLfoRateTable[128];
uint32_t envRateTableQ8[128];

// ================================================================
// Optional SD-card wavetables
// indexes /PPGWT files at boot and loads only the active table
// into a dual internal SRAM cache. Supports 64/128 waves, WTB/UWT/RAW.
// ================================================================
char sdWavetableNames[SD_WT_MAX_TABLES][13];
char sdWavetablePaths[SD_WT_MAX_TABLES][96];
uint16_t sdWavetableWaves[SD_WT_MAX_TABLES];
uint32_t sdWavetableSizes[SD_WT_MAX_TABLES];
bool sdWavetableUnsigned[SD_WT_MAX_TABLES];
volatile uint8_t sdWavetableCount = 0;

// Dual SRAM cache for SD wavetables.
// Audio reads only from the active cache, never from PSRAM.
#define SD_WT_CACHE_BYTES SD_WT_FILE_SIZE_128
int8_t *sdWtCache[2] = {nullptr, nullptr};
uint16_t sdWtCacheWaves[2] = {0, 0};
int16_t sdWtCacheSlot[2] = {-1, -1};
volatile uint8_t activeSdWtCache = 0;
volatile int16_t activeSdWtSlot = -1;
volatile bool sdWtCacheReady = false;

// Async SD wavetable cache request.
// setParam() and MIDI/UI only request loading; controlTask performs SD I/O.
volatile int16_t requestedSdWtSlot = -1;
volatile uint32_t sdWtCacheRequestCount = 0;
volatile uint32_t sdWtCacheLoadCount = 0;
volatile uint32_t sdWtCacheLoadFailCount = 0;

volatile bool sdCardOk = false;
volatile int16_t pitchBend = 0;
volatile uint8_t modWheel = 0;
volatile uint8_t channelAftertouch = 0;
volatile uint8_t expressionLevel = 127;

// ================================================================
// Smooth params
// ================================================================
struct SmoothParam { int32_t current; int32_t target; };
SmoothParam smoothCutoff = {70 << 8, 70 << 8};
SmoothParam smoothWavePos = {20 << 8, 20 << 8};
SmoothParam smoothVolume = {100 << 8, 100 << 8};

static inline void smoothUpdate(SmoothParam &p, uint8_t target, uint8_t speedShift) {
  p.target = ((int32_t)target) << 8;
  p.current += (p.target - p.current) >> speedShift;
}

static inline void updateSmoothParams() {
  smoothUpdate(smoothCutoff, getAParam(P_CUTOFF), 5);
  smoothUpdate(smoothWavePos, getAParam(P_WAVE_POS), 5);
  smoothUpdate(smoothVolume, getAParam(P_VOLUME), 6);
}

// ================================================================
// UI
// ================================================================
enum UiPage {
  PAGE_OSC,
  PAGE_FILTER,
  PAGE_AMP_ENV,
  PAGE_FILTER_ENV,
  PAGE_WAVE_ENV,
  PAGE_LFO,
  PAGE_PERFORMANCE,
  PAGE_FX,
  PAGE_ARP,
  PAGE_SEQ,
  PAGE_MORPH,
  PAGE_MIDI,
  PAGE_WAVE_MON,
  PAGE_PROGRAM,
  PAGE_COUNT
};

const char* pageNames[PAGE_COUNT] = {
  "OSC", "FILTER", "AMP ENV", "FILTER ENV", "WAVE ENV", "LFO", "PERFORM", "FX", "ARP", "SEQ", "MORPH", "MIDI", "WAVE MON", "PROGRAM"
};

const uint8_t pageParams[PAGE_COUNT][UI_PARAMS_PER_PAGE] = {
  {P_WAVETABLE, P_WAVE_POS, P_WAVE_MOD, P_OSC_MIX, P_OSC_DETUNE, P_OSC_B_OFFSET, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_CUTOFF, P_RESONANCE, P_FILTER_ENV, P_VEL_FILTER, P_KEYTRACK, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_F_ATTACK, P_F_DECAY, P_F_SUSTAIN, P_F_RELEASE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_WAVE_ENV, P_WAVE_ENV_ATTACK, P_WAVE_ENV_DECAY, P_WAVE_ENV_SUSTAIN, P_WAVE_ENV_RELEASE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_LFO_RATE, P_LFO_AMOUNT, P_LFO_TARGET, UI_PARAM_LFO_SHAPE, P_WAVE_LFO_RATE, P_WAVE_LFO_AMOUNT, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_PLAY_MODE, P_GLIDE, P_BEND_RANGE, P_VEL_AMP, P_PAN_SPREAD, P_UNISON_DETUNE, UI_PARAM_CC_MODE, UI_PARAM_NONE},
  {P_DRIVE, P_BITCRUSH, P_SUB_LEVEL, P_NOISE_LEVEL, P_CHORUS, P_VOLUME, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_ARP_MODE, P_ARP_RATE, P_ARP_OCTAVES, P_ARP_HOLD, P_CLOCK_SOURCE, P_TAP_TEMPO, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_SEQ_MODE, P_SEQ_RATE, P_SEQ_STEPS, P_SEQ_TARGET, P_SEQ_DEPTH, UI_PARAM_SEQ_TABLE_MODE, UI_PARAM_SEQ_STEP, UI_PARAM_SEQ_VALUE},
  {P_MORPH_AMOUNT, P_RANDOMIZE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_AFTERTOUCH_WAVE, P_AFTERTOUCH_FILTER, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {P_WAVETABLE, P_WAVE_POS, P_WAVE_MOD, P_WAVE_ENV, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE},
  {UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE, UI_PARAM_NONE}
};

uint8_t uiPage = PAGE_OSC;
uint8_t uiRow = 0;
bool ccLearnMode = false;
uint8_t learnParam = 0;
bool storeMessage = false;
uint32_t storeMessageTime = 0;
uint32_t programPreviewUntil = 0;
bool waveMonitorTableSelect = false;
bool waveMonitorCacheValid = false;
uint8_t waveMonitorCacheTable = 255;
uint8_t waveMonitorCacheWave = 255;
int8_t waveMonitorCacheY[128];
bool userSaveMode = false;
uint8_t userSaveSlot = 30;
bool userSaveConfirmOverwrite = false;
char userSaveExistingName[PROGRAM_NAME_LEN] = "";
char userSaveOriginalName[PROGRAM_NAME_LEN] = "";
bool userSaveExistingOccupied = false;
uint8_t userSaveExistingCacheSlot = 255;
bool userNameEditMode = false;
char userNameEditBackup[PROGRAM_NAME_LEN] = "";
uint8_t userNameCursor = 0;
const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";

enum PresetCategory : uint8_t { CAT_PAD, CAT_LEAD, CAT_BASS, CAT_SEQ, CAT_FX, CAT_ORGAN, CAT_MISC };
uint8_t currentProgramCategory = CAT_MISC;

#ifndef SD_BACKUP_DIR
#define SD_BACKUP_DIR "/PPGBACKUP"
#endif
#ifndef SD_META_DIR
#define SD_META_DIR   "/PPGMETA"
#endif
#ifndef SD_PRESET_MAGIC
#define SD_PRESET_MAGIC 0x31504750UL
#endif
#ifndef SD_BANK_MAGIC
#define SD_BANK_MAGIC   0x314B4250UL
#endif
#ifndef SD_PRESET_VERSION
#define SD_PRESET_VERSION 1
#endif
#ifndef SD_BANK_VERSION
#define SD_BANK_VERSION 1
#endif

#ifndef HAVE_V851_SD_GLOBALS
#define HAVE_V851_SD_GLOBALS

bool saveCurrentPresetToSD(const char *name, uint8_t category);
bool loadFirstPresetFromSD();
bool saveBankToSD(const char *filename);
bool loadFirstBankFromSD();
#endif

const char* const categoryNames[] = { "PAD", "LEAD", "BASS", "SEQ", "FX", "ORGAN", "MISC" };

const uint8_t factoryCategory[30] = {
  CAT_MISC, CAT_PAD, CAT_PAD, CAT_PAD, CAT_FX, CAT_PAD, CAT_ORGAN, CAT_LEAD,
  CAT_BASS, CAT_BASS, CAT_BASS, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_LEAD, CAT_SEQ,
  CAT_SEQ, CAT_SEQ, CAT_FX, CAT_MISC, CAT_MISC, CAT_PAD, CAT_PAD, CAT_ORGAN,
  CAT_PAD, CAT_PAD, CAT_PAD, CAT_FX, CAT_FX, CAT_MISC
};

// ================================================================
// Preset Browser by category
// Does not change Program struct/NVS format.
// Factory presets 0..29 use factoryCategory[].
// User presets 30..127 are shown under USER.
// ================================================================
enum BrowserCategory : uint8_t {
  BROWSE_ALL,
  BROWSE_PAD,
  BROWSE_LEAD,
  BROWSE_BASS,
  BROWSE_SEQ,
  BROWSE_FX,
  BROWSE_ORGAN,
  BROWSE_MISC,
  BROWSE_USER,
  BROWSE_COUNT
};

const char* const browserCategoryNames[BROWSE_COUNT] = {
  "ALL", "PAD", "LEAD", "BASS", "SEQ", "FX", "ORGAN", "MISC", "USER"
};

bool presetBrowserActive = false;
uint8_t browserCategory = BROWSE_ALL;
uint8_t browserProgram = 0;

// browser name cache.
// Factory names are read from NVS; User names are read from /PPGPRESETS/Uxxx.PPG.
char browserProgramNames[NUM_PROGRAMS][PROGRAM_NAME_LEN];
bool browserProgramValid[NUM_PROGRAMS];
uint32_t browserCacheBuildCount = 0;
bool browserNamesDirty = true;
bool browserScanActive = false;
uint8_t browserScanIndex = 0;
uint32_t browserScanLastMs = 0;

static inline uint8_t programCategoryForBrowser(uint8_t programNumber) {
  if (programNumber < 30) {
    switch (factoryCategory[programNumber]) {
      case CAT_PAD: return BROWSE_PAD;
      case CAT_LEAD: return BROWSE_LEAD;
      case CAT_BASS: return BROWSE_BASS;
      case CAT_SEQ: return BROWSE_SEQ;
      case CAT_FX: return BROWSE_FX;
      case CAT_ORGAN: return BROWSE_ORGAN;
      default: return BROWSE_MISC;
    }
  }
  return BROWSE_USER;
}

static inline bool browserProgramExists(uint8_t programNumber) {
  if (programNumber < 30) return true;
  if (browserProgramValid[programNumber]) return true;
  // While scanning, allow browsing user slots so the browser opens instantly.
  if (browserScanActive && programNumber >= 30) return true;
  return false;
}

static inline bool browserProgramMatches(uint8_t programNumber) {
  if (!browserProgramExists(programNumber)) return false;
  if (browserCategory == BROWSE_ALL) return true;
  return programCategoryForBrowser(programNumber) == browserCategory;
}

void browserMoveProgram(int8_t delta) {
  uint8_t p = browserProgram;

  for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
    if (delta >= 0) p = (p + 1) & 127;
    else p = (p == 0) ? 127 : p - 1;

    if (browserProgramMatches(p)) {
      browserProgram = p;
      return;
    }
  }
}

void browserMoveCategory(int8_t delta) {
  if (delta >= 0) browserCategory = (browserCategory + 1) % BROWSE_COUNT;
  else browserCategory = (browserCategory == 0) ? BROWSE_COUNT - 1 : browserCategory - 1;

  if (!browserProgramMatches(browserProgram)) {
    // Find first program in new category.
    for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
      if (browserProgramMatches(i)) {
        browserProgram = i;
        break;
      }
    }
  }
}

void loadBrowserProgram() {
  safeLoadProgram(browserProgram);
  presetBrowserActive = false;
}

void drawPresetBrowser() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.setCursor(0, 8);
  u8g2.print("PRESET BROWSER ");

  u8g2.print("[");
  u8g2.print(browserCategoryNames[browserCategory]);
  u8g2.print("]");

  // Draw 4 entries around the current selection.
  uint8_t rows[4];
  rows[0] = browserProgram;

  // Find previous entry for first row if possible.
  uint8_t first = browserProgram;
  for (uint8_t back = 0; back < 1; back++) {
    uint8_t p = first;
    for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
      p = (p == 0) ? 127 : p - 1;
      if (browserProgramMatches(p)) {
        first = p;
        break;
      }
    }
  }

  uint8_t p = first;
  for (uint8_t r = 0; r < 4; r++) {
    rows[r] = p;
    for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
      uint8_t n = (p + 1) & 127;
      p = n;
      if (browserProgramMatches(p)) break;
    }
  }

  for (uint8_t r = 0; r < 4; r++) {
    uint8_t prg = rows[r];
    int y = 22 + r * 10;

    if (prg == browserProgram) {
      u8g2.setCursor(0, y);
      u8g2.print(">");
    }

    u8g2.setCursor(8, y);
    u8g2.print(prg < 30 ? "P" : "U");
    if (prg < 100) u8g2.print("0");
    if (prg < 10) u8g2.print("0");
    u8g2.print(prg);

    u8g2.setCursor(38, y);
    u8g2.print(browserProgramDisplayName(prg));
  }

  u8g2.setCursor(0, 64);
  u8g2.print("Enc Sel  EncSw/B7 Load");

  u8g2.sendBuffer();
}

static inline uint8_t tableToParam(uint8_t table) {
  if (table >= WT_VISIBLE_SLOTS) table = WT_LAST_SLOT;
  return table;
}

void showProgramPreview() {
  programPreviewUntil = millis() + 2000UL;
}

//// Buttons
//BfButton btn1(BfButton::STANDALONE_DIGITAL, BTN_1_PIN, true, LOW);
//BfButton btn2(BfButton::STANDALONE_DIGITAL, BTN_2_PIN, true, LOW);
//BfButton btn3(BfButton::STANDALONE_DIGITAL, BTN_3_PIN, true, LOW);
//BfButton btn4(BfButton::STANDALONE_DIGITAL, BTN_4_PIN, true, LOW);
//BfButton btn5(BfButton::STANDALONE_DIGITAL, BTN_5_PIN, true, LOW);
//BfButton btn6(BfButton::STANDALONE_DIGITAL, BTN_6_PIN, true, LOW);
//BfButton btn7(BfButton::STANDALONE_DIGITAL, BTN_7_PIN, true, LOW);
//BfButton btn8(BfButton::STANDALONE_DIGITAL, BTN_8_PIN, true, LOW);
//int lastEncA = HIGH;

// ================================================================
// Arpeggiator and MIDI clock
// ================================================================
uint8_t arpNotes[ARP_MAX_NOTES];
uint8_t arpVel[ARP_MAX_NOTES];
uint8_t arpCount = 0;
int8_t arpIndex = 0;
int8_t arpDir = 1;
uint32_t arpLastStep = 0;
uint16_t bpm = 120;
uint8_t currentArpNote = 255;

// MIDI channel 1..16. Stored in Preferences via boot menu.
uint8_t midiChannel = 1;
bool bootMidiChannelChanged = false;
uint8_t bootMidiChannelValue = 1;
uint8_t midiCcBank = 0;  // CC32: 0=Bank0, 1=Bank1, 2=Bank2
uint8_t midiCcMode = 0;  // 0=MOPHO, 1=LEARN
uint8_t globalClockSource = 0;
// 0=INT, 1=MIDI. Global preference.
//uint8_t globalLfoShape = 0;     // Global LFO shape: 0=TRI, 1=SINE, 2=SAW UP, 3=SAW DN, 4=SQUARE, 5=RANDOM.
volatile uint32_t lastClockMicros = 0;
volatile uint32_t clockPeriodMicros = 20833;
volatile uint32_t midiBpm_x100 = 12000;
volatile uint32_t midiBpmSmoothed_x100 = 12000;
uint16_t midiBpmDisplayStable = 120;
uint32_t midiBpmDisplayLastMs = 0;
volatile bool midiClockRunning = false;
volatile bool midiClockValid = false;
uint8_t arpClockCounter = 0;
uint8_t modSeqClockCounter = 0;
uint32_t lastTapTime = 0;

// ================================================================
// Utility
// ================================================================
static inline int32_t clip32(int32_t x, int32_t lo, int32_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static inline uint32_t makePhaseInc(float freq) {
  return (uint32_t)((freq * 4294967296.0f) / SAMPLE_RATE);
}

void buildNoteTable() {
  for (int n = 0; n < 128; n++) {
    float freq = 440.0f * powf(2.0f, (n - 69) / 12.0f);
    notePhaseInc[n] = makePhaseInc(freq);
  }
}

void buildPitchBendTables() {
  for (int range = 0; range < PB_RANGES; range++) {
    for (int i = 0; i < 256; i++) {
      float x = ((float)i - 128.0f) / 128.0f;
      float ratio = powf(2.0f, x * range / 12.0f);
      pitchBendTables[range][i] = (uint16_t)(ratio * 4096.0f);
    }
  }
}

void buildLfoRateTable() {
  for (int i = 0; i < 128; i++) {
    float hz = 0.05f + (i * i) * 0.002f;
    lfoRateTable[i] = (uint16_t)((hz * 65536.0f) / SAMPLE_RATE);
  }
}

void buildWaveLfoRateTable() {
  for (int i = 0; i < 128; i++) {
    float hz = 0.02f + (i * i) * 0.0012f;
    waveLfoRateTable[i] = (uint16_t)((hz * 65536.0f) / SAMPLE_RATE);
  }
}

void buildEnvRateTable() {
  // ADSR parameter 0..127 -> Q8 increment per sample.
  // 0 = very fast, 127 = very slow.
  //
  // The old direct formula made most values below about 80 extremely short.
  // This lookup table gives a much more musical response for all three ADSRs:
  // Amp ADSR, Filter ADSR and Wave ADSR.
  //
  // Approximate full-scale times:
  //   0   ->   ~2 ms
  //   32  ->  ~190 ms
  //   64  ->  ~1.5 s
  //   96  ->  ~5.2 s
  //   127 -> ~12.0 s
  const float envMax = (float)(32767L << 8);
  const float minMs = 2.0f;
  const float maxMs = 12000.0f;

  for (int i = 0; i < 128; i++) {
    float t = (float)i / 127.0f;
    float ms = minMs + (maxMs - minMs) * t * t * t;
    float samples = (SAMPLE_RATE * ms) / 1000.0f;
    uint32_t inc = (uint32_t)((envMax / samples) + 0.5f);

    if (inc < 1) inc = 1;
    if (inc > (uint32_t)envMax) inc = (uint32_t)envMax;

    envRateTableQ8[i] = inc;
  }
}

static inline int16_t triangleLfo(uint16_t phase) {
  if (phase < 32768) return ((int32_t)phase * 2) - 32768;
  return 32767 - ((int32_t)(phase - 32768) * 2);
}

static inline int16_t lfoShapeRandom(Voice &v, uint16_t phase) {
  if (phase < v.lfoRandomLastPhase) {
    v.noiseSeed = v.noiseSeed * 1664525UL + 1013904223UL;
    v.lfoRandomValue = (int16_t)((v.noiseSeed >> 16) & 0xFFFF);
  }
  v.lfoRandomLastPhase = phase;
  return v.lfoRandomValue;
}

static inline int16_t lfoShapeValue(Voice &v, uint8_t shape, uint16_t phase) {
  switch (shape) {
    case 1: return lfoShapeSine(phase);
    case 2: return lfoShapeSawUp(phase);
    case 3: return lfoShapeSawDown(phase);
    case 4: return lfoShapeSquare(phase);
    case 5: return lfoShapeRandom(v, phase);
    case 0:
    default: return lfoShapeTriangle(phase);
  }
}


// ================================================================
// Wavetable access: 127 visible slots.
// Flash slots: 0..126.
// SD wavetables override from the back:
//   SD0 -> slot 126
//   SD1 -> slot 125
//   SD2 -> slot 124
// ================================================================
static inline uint8_t sdOverrideCount() {
  uint8_t n = sdWavetableCount;
  if (n > WT_VISIBLE_SLOTS) n = WT_VISIBLE_SLOTS;
  return n;
}

static inline bool visibleSlotIsSd(uint8_t table) {
  return sdIndexForVisibleSlot(table) >= 0;
}

const char* currentWavetableName() {
  uint8_t t = selectedWavetableIndex();
  int16_t sd = sdIndexForVisibleSlot(t);
  if (sd >= 0) return sdWavetableNames[sd];

  // Flash area is fixed to slots 0..126.
  if (t < PPG_WT_TABLES) return PPG_WT_NAMES[t];
  return "WT?";
}

static inline int16_t currentWavetableSeqOffset() {
  return (int16_t)selectedWavetableIndexAudio() - (int16_t)selectedWavetableIndex();
}

// ================================================================
// Direct wavetable stepping helper
// One encoder/button step = one visible wavetable slot.
// ================================================================
static inline int16_t readPPGSample(uint8_t table, uint8_t wave, uint8_t sample) {
  if (table >= WT_VISIBLE_SLOTS) table = WT_LAST_SLOT;

  int16_t sd = sdIndexForVisibleSlot(table);
  if (sd >= 0) {
    if (sd < sdWavetableCount && sdWtCacheReady && activeSdWtSlot == sd) {
      uint8_t cache = activeSdWtCache;
      int8_t *data = sdWtCache[cache];
      if (data == nullptr) return 0;

      uint16_t waveCount = sdWtCacheWaves[cache];
      if (waveCount < 1) waveCount = 64;

      uint16_t w = wave;
      if (waveCount == 128) w = ((uint16_t)wave * 127UL + 31UL) / 63UL;
      else if (w >= waveCount) w = waveCount - 1;

      uint32_t idx = ((uint32_t)w * PPG_WT_SAMPLES) + sample;
      return ((int16_t)data[idx]) << 8;
    }

    // SD slot selected but cache not ready yet.
    return 0;
  }

  // Flash fallback. Only slots 0..126 are used as fixed flash slots.
  if (table < PPG_WT_TABLES) {
    int8_t v = pgm_read_byte_near(&PPG_WAVETABLES[table][wave][sample]);
    return ((int16_t)v) << 8;
  }

  return 0;
}

static inline int16_t readPPGSampleRouted(uint8_t table, int16_t sd, uint8_t wave, uint8_t sample) {
  // Fast audio-path sample reader.
  // The expensive visible-slot -> SD-index routing is done once in readWaveAt(),
  // not once per sample tap.
  if (sd >= 0) {
    if (sd < sdWavetableCount && sdWtCacheReady && activeSdWtSlot == sd) {
      uint8_t cache = activeSdWtCache;
      int8_t *data = sdWtCache[cache];
      if (data == nullptr) return 0;

      uint16_t waveCount = sdWtCacheWaves[cache];
      if (waveCount < 1) waveCount = 64;

      uint16_t w = wave;
      if (waveCount == 128) w = ((uint16_t)wave * 127UL + 31UL) / 63UL;
      else if (w >= waveCount) w = waveCount - 1;

      uint32_t idx = ((uint32_t)w * PPG_WT_SAMPLES) + sample;
      return ((int16_t)data[idx]) << 8;
    }

    return 0;
  }

  if (table < PPG_WT_TABLES) {
    int8_t v = pgm_read_byte_near(&PPG_WAVETABLES[table][wave][sample]);
    return ((int16_t)v) << 8;
  }
  return 0;
}

// ================================================================
// OLED Wave Monitor
// Shows the currently selected wavetable and logical wave position.
// Runs only in UI refresh, never in the audio loop.
// ================================================================
uint8_t modSeqTableMode = 0;  // 0=ABS, 1=REL for Seq Target = Table

static bool hasWtExtension(const char *name) {
  size_t n = strlen(name);
  if (n < 4) return false;
  const char *e = name + n - 4;
  return (strcasecmp(e, ".WTB") == 0) || (strcasecmp(e, ".UWT") == 0) || (strcasecmp(e, ".RAW") == 0);
}

static bool isUnsignedWtFile(const char *name) {
  size_t n = strlen(name);
  if (n < 4) return false;
  const char *e = name + n - 4;
  return strcasecmp(e, ".UWT") == 0;
}

static void makeSdWtName(const char *filename, char *out, size_t outLen) {
  // Keep an OLED-friendly 12 char name without path and extension.
  const char *base = strrchr(filename, '/');
  base = base ? base + 1 : filename;
  strncpy(out, base, outLen);
  out[outLen - 1] = 0;

  char *dot = strrchr(out, '.');
  if (dot) *dot = 0;

  // Replace underscores with spaces for display.
  for (char *p = out; *p; ++p) {
    if (*p == '_') *p = ' ';
  }
}

bool loadSdWavetableFile(const char *path, uint8_t slot) {
  // metadata-only. The actual wavetable data is loaded on demand
  // into one of the two internal SRAM caches.
  if (slot >= SD_WT_MAX_TABLES) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) {
    sdWavetableLoadErrors++;
    return false;
  }

  size_t fileSize = f.size();
  f.close();

  uint16_t waveCount = 0;
  if (fileSize == SD_WT_FILE_SIZE_64) {
    waveCount = 64;
  } else if (fileSize == SD_WT_FILE_SIZE_128) {
    waveCount = 128;
  } else {
    Serial.print("WT size mismatch: ");
    Serial.print(path);
    Serial.print(" size=");
    Serial.println(fileSize);
    sdWavetableLoadErrors++;
    return false;
  }

  strncpy(sdWavetablePaths[slot], path, sizeof(sdWavetablePaths[slot]));
  sdWavetablePaths[slot][sizeof(sdWavetablePaths[slot]) - 1] = 0;
  makeSdWtName(path, sdWavetableNames[slot], sizeof(sdWavetableNames[slot]));
  sdWavetableWaves[slot] = waveCount;
  sdWavetableSizes[slot] = fileSize;
  sdWavetableUnsigned[slot] = isUnsignedWtFile(path);
  return true;
}

void freeSdWavetables() {
  for (uint8_t i = 0; i < SD_WT_MAX_TABLES; i++) {
    sdWavetableNames[i][0] = 0;
    sdWavetablePaths[i][0] = 0;
    sdWavetableWaves[i] = 0;
    sdWavetableSizes[i] = 0;
    sdWavetableUnsigned[i] = false;
  }
  sdWavetableCount = 0;
  activeSdWtSlot = -1;
  sdWtCacheReady = false;
  sdWtCacheSlot[0] = -1;
  sdWtCacheSlot[1] = -1;
  sdWtCacheWaves[0] = 0;
  sdWtCacheWaves[1] = 0;
}

bool allocateSdWtCaches() {
  if (sdWtCache[0] == nullptr) {
    sdWtCache[0] = (int8_t*)heap_caps_malloc(SD_WT_CACHE_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  if (sdWtCache[1] == nullptr) {
    sdWtCache[1] = (int8_t*)heap_caps_malloc(SD_WT_CACHE_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  if (sdWtCache[0] == nullptr || sdWtCache[1] == nullptr) {
    Serial.println("ERROR: not enough internal SRAM for dual WT cache");
    Serial.print("Cache A: "); Serial.println(sdWtCache[0] ? "OK" : "FAIL");
    Serial.print("Cache B: "); Serial.println(sdWtCache[1] ? "OK" : "FAIL");
    return false;
  }
  return true;
}

bool loadSdWavetableIntoCache(uint8_t sd, uint8_t cacheIndex) {
  if (sd >= sdWavetableCount || cacheIndex > 1) return false;
  if (!allocateSdWtCaches()) return false;

  File f = SD.open(sdWavetablePaths[sd], FILE_READ);
  if (!f) {
    sdWavetableLoadErrors++;
    return false;
  }

  size_t fileSize = f.size();
  if (fileSize != sdWavetableSizes[sd] || fileSize > SD_WT_CACHE_BYTES) {
    f.close();
    sdWavetableLoadErrors++;
    return false;
  }

  int8_t *dst = sdWtCache[cacheIndex];
  bool unsignedFormat = sdWavetableUnsigned[sd];

  if (unsignedFormat) {
    const size_t BUF_SIZE = 256;
    uint8_t buf[BUF_SIZE];
    size_t written = 0;

    while (written < fileSize) {
      size_t todo = fileSize - written;
      if (todo > BUF_SIZE) todo = BUF_SIZE;

      size_t got = f.read(buf, todo);
      if (got != todo) {
        f.close();
        sdWavetableLoadErrors++;
        return false;
      }

      for (size_t i = 0; i < got; i++) {
        dst[written + i] = (int8_t)((int16_t)buf[i] - 128);
      }
      written += got;
    }
  } else {
    size_t got = f.read((uint8_t*)dst, fileSize);
    if (got != fileSize) {
      f.close();
      sdWavetableLoadErrors++;
      return false;
    }
  }
  f.close();
  // Clear unused half if this is a 64-wave table, so accidental overread is safe.
  if (fileSize < SD_WT_CACHE_BYTES) {
    memset(dst + fileSize, 0, SD_WT_CACHE_BYTES - fileSize);
  }

  sdWtCacheSlot[cacheIndex] = sd;
  sdWtCacheWaves[cacheIndex] = sdWavetableWaves[sd];
  return true;
}

bool ensureSdWavetableCached(uint8_t sd) {
  if (sd >= sdWavetableCount) return false;

  if (sdWtCacheReady && activeSdWtSlot == sd) return true;

  // If already in inactive cache, just switch active cache.
  uint8_t other = 1 - activeSdWtCache;
  if (sdWtCacheSlot[other] == sd) {
    activeSdWtCache = other;
    activeSdWtSlot = sd;
    sdWtCacheReady = true;
  invalidateWaveMonitorCache();
    return true;
  }

  // Load into inactive cache while current cache remains valid.
  if (!loadSdWavetableIntoCache(sd, other)) return false;

  activeSdWtCache = other;
  activeSdWtSlot = sd;
  sdWtCacheReady = true;
  return true;
}

void buildPPGStyleTables() {
  // Built-in wavetables are supplied by PPGWavetables.h in PROGMEM.
  // Optional SD wavetables are indexed and loaded on demand into dual SRAM cache.
}

// ================================================================
// Synth helpers
// ================================================================
static inline uint32_t applyPitchBend(uint32_t base) {
  int pb = pitchBend + 8192;
  int idx = constrain(pb >> 6, 0, 255);
  uint8_t range = getAParam(P_BEND_RANGE);
  if (range >= PB_RANGES) range = 12;
  return ((uint64_t)base * pitchBendTables[range][idx]) >> 12;
}

static inline uint32_t detunePhaseInc(uint32_t base, int8_t amount) {
  int32_t det = ((int32_t)(base >> 12)) * amount;
  return base + det;
}

static inline void updateGlide(Voice &v) {
  uint8_t g = getAParamSeq(P_GLIDE);
  if (g == 0) {
    v.incA = v.targetInc;
    return;
  }
  // higher values mean slower/more audible glide.
  uint8_t shift = 4 + (g >> 3);  // 4..19

  int32_t diff = (int32_t)v.targetInc - (int32_t)v.incA;
  if (diff == 0) return;

  int32_t step = diff >> shift;

  // Prevent glide from getting stuck near the target when diff becomes tiny.
  if (step == 0) step = (diff > 0) ? 1 : -1;

  if ((diff > 0 && step > diff) || (diff < 0 && step < diff)) {
    v.incA = v.targetInc;
  } else {
    v.incA += step;
  }
}

static inline void updateGlideSnap(Voice &v, uint8_t g) {
  if (g == 0) {
    v.incA = v.targetInc;
    return;
  }

  uint8_t shift = 4 + (g >> 3);  // 4..19
  int32_t diff = (int32_t)v.targetInc - (int32_t)v.incA;
  if (diff == 0) return;
  int32_t step = diff >> shift;
  if (step == 0) step = (diff > 0) ? 1 : -1;
  if ((diff > 0 && step > diff) || (diff < 0 && step < diff)) {
    v.incA = v.targetInc;
  } else {
    v.incA += step;
  }
}


static inline int32_t envRateFromParamQ8(uint8_t v) {
  // Fast audio-loop function: lookup only.
  // Table is built once in setup() by buildEnvRateTable().
  uint32_t inc = envRateTableQ8[v & 0x7F];
  // Safety fallback in case the table is ever queried before setup finished.
  if (inc == 0) inc = 24;
  return (int32_t)inc;
}

static inline int32_t processEnvGeneric(int32_t &env, uint8_t &stage, uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release) {
  // env is Q8 internally: 0 .. (32767 << 8)
  const int32_t ENV_MAX_Q8 = 32767L << 8;
  int32_t sustainLevelQ8 = ((int32_t)sustain << 8) << 8; // sustain 0..127 -> approx 0..32512, then Q8

  switch (stage) {
    case ENV_ATTACK: {
      env += envRateFromParamQ8(attack);
      if (env >= ENV_MAX_Q8) {
        env = ENV_MAX_Q8;
        stage = ENV_DECAY;
      }
      break;
    }

    case ENV_DECAY: {
      env -= envRateFromParamQ8(decay);
      if (env <= sustainLevelQ8) {
        env = sustainLevelQ8;
        stage = ENV_SUSTAIN;
      }
      break;
    }

    case ENV_SUSTAIN:
      env = sustainLevelQ8;
      break;

    case ENV_RELEASE: {
      env -= envRateFromParamQ8(release);
      if (env <= 0) {
        env = 0;
        stage = ENV_OFF;
      }
      break;
    }

    default:
      env = 0;
      break;
  }
  return env >> 8; // return normal 0..32767 level
}

static inline int32_t processAmpEnv(Voice &v) {
  return processEnvGeneric(v.ampEnv, v.ampStage, getAParamSeq(P_ATTACK), getAParamSeq(P_DECAY), getAParamSeq(P_SUSTAIN), getAParamSeq(P_RELEASE));
}

static inline int32_t processFilterEnv(Voice &v) {
  return processEnvGeneric(v.filtEnv, v.filtStage, getAParamSeq(P_F_ATTACK), getAParamSeq(P_F_DECAY), getAParamSeq(P_F_SUSTAIN), getAParamSeq(P_F_RELEASE));
}
static inline int32_t processWaveEnv(Voice &v) {
  return processEnvGeneric(v.waveEnv, v.waveStage, getAParamSeq(P_WAVE_ENV_ATTACK), getAParamSeq(P_WAVE_ENV_DECAY), getAParamSeq(P_WAVE_ENV_SUSTAIN), getAParamSeq(P_WAVE_ENV_RELEASE));
}

static inline int16_t fastNoise(Voice &v) {
  v.noiseSeed = v.noiseSeed * 1664525UL + 1013904223UL;
  return (int16_t)(v.noiseSeed >> 16);
}

static inline int16_t bitCrunch(int16_t x) {
  uint8_t crush = getAParamSeq(P_BITCRUSH);
  if (crush < 8) return x;
  uint8_t shift = crush >> 4;
  return (x >> shift) << shift;
}

static inline int16_t ppgDrive(int32_t x) {
  int32_t drive = 256 + (getAParamSeq(P_DRIVE) << 3);
  x = (x * drive) >> 8;
  if (x > 26000) x = 26000 + ((x - 26000) >> 3);
  if (x < -26000) x = -26000 + ((x + 26000) >> 3);
  return constrain(x, -32768, 32767);
}

static inline int16_t readWaveAt(uint32_t &phase, uint32_t inc, int32_t wavePos, uint8_t table, int16_t sd) {
  uint8_t si = phase >> 24;
  uint8_t sf = (phase >> 16) & 0xFF;

  wavePos = clip32(wavePos, 0, ((PPG_WT_WAVES - 1) << 8));
  uint8_t wa = wavePos >> 8;
  uint8_t wb = wa < (PPG_WT_WAVES - 1) ? wa + 1 : wa;
  uint8_t wf = wavePos & 0xFF;

  int16_t a0, a1, b0, b1;

  if (sd < 0 && table < PPG_WT_TABLES) {
    // Fast common path: internal flash wavetable.
    a0 = ((int16_t)pgm_read_byte_near(&PPG_WAVETABLES[table][wa][si])) << 8;
    a1 = ((int16_t)pgm_read_byte_near(&PPG_WAVETABLES[table][wa][(si + 1) & WT_MASK])) << 8;
    b0 = ((int16_t)pgm_read_byte_near(&PPG_WAVETABLES[table][wb][si])) << 8;
    b1 = ((int16_t)pgm_read_byte_near(&PPG_WAVETABLES[table][wb][(si + 1) & WT_MASK])) << 8;
  } else {
    // SD override path.
    a0 = readPPGSampleRouted(table, sd, wa, si);
    a1 = readPPGSampleRouted(table, sd, wa, (si + 1) & WT_MASK);
    b0 = readPPGSampleRouted(table, sd, wb, si);
    b1 = readPPGSampleRouted(table, sd, wb, (si + 1) & WT_MASK);
  }

  int32_t sa = a0 + (((int32_t)(a1 - a0) * sf) >> 8);
  int32_t sb = b0 + (((int32_t)(b1 - b0) * sf) >> 8);

  phase += applyPitchBend(inc);
  return (int16_t)(sa + (((sb - sa) * wf) >> 8));
}

// Cheaper oscillator path used only under high polyphony pressure.
// It keeps sample interpolation but skips interpolation between adjacent waves.
// This reduces table reads from 4 to 2 per oscillator.
static inline int16_t readWaveAtFast(uint32_t &phase, uint32_t inc, int32_t wavePos, uint8_t table, int16_t sd) {
  uint8_t si = phase >> 24;
  uint8_t sf = (phase >> 16) & 0xFF;

  wavePos = clip32(wavePos, 0, ((PPG_WT_WAVES - 1) << 8));
  uint8_t wa = wavePos >> 8;

  int16_t a0, a1;

  if (sd < 0 && table < PPG_WT_TABLES) {
    // Fast common path: internal flash wavetable.
    a0 = ((int16_t)pgm_read_byte_near(&PPG_WAVETABLES[table][wa][si])) << 8;
    a1 = ((int16_t)pgm_read_byte_near(&PPG_WAVETABLES[table][wa][(si + 1) & WT_MASK])) << 8;
  } else {
    // SD override path.
    a0 = readPPGSampleRouted(table, sd, wa, si);
    a1 = readPPGSampleRouted(table, sd, wa, (si + 1) & WT_MASK);
  }

  phase += applyPitchBend(inc);
  return (int16_t)(a0 + (((int32_t)(a1 - a0) * sf) >> 8));
}

static inline int16_t oscRead(Voice &v) {
  AudioSnapshot a;
  readAudioSnapshot(a);

  updateGlideSnap(v, a.glide);

  const bool fastPoly = (PPG_ENABLE_FAST_POLY && audioPolyLoad >= PPG_POLY_FAST_THRESHOLD);
  if (fastPoly) highPolyFastSamples++;

  uint8_t wtTable = selectedWavetableIndexFromSnapshot(a);
  int16_t wtSd = sdIndexForVisibleSlot(wtTable);

  int32_t wavePos = smoothWavePos.current;

  int32_t we = processWaveEnv(v);
  wavePos += (we * a.waveEnv) >> 15;

  v.waveLfoPhase += waveLfoRateTable[a.waveLfoRate];
  int16_t wlfo = triangleLfo(v.waveLfoPhase);
  wavePos += (wlfo * a.waveLfoAmount) >> 7;

  wavePos += ((int32_t)modWheel * a.waveMod) >> 1;
  wavePos += ((int32_t)channelAftertouch * a.aftertouchWave) >> 1;

  v.lfoPhase += lfoRateTable[a.lfoRate];
  int16_t lfo = lfoShapeValue(v, a.lfoShape, v.lfoPhase);
  if (a.lfoTarget == LFO_TO_WAVE) wavePos += (lfo * a.lfoAmount) >> 7;

  uint32_t base = detunePhaseInc(v.incA, v.drift);
  uint8_t mode = a.playMode;

  int8_t oscDet = a.oscDetune;
  if (mode == 2) oscDet += a.unisonDetune;

  uint32_t incBTarget = detunePhaseInc(base, oscDet);
  v.incB += ((int32_t)incBTarget - (int32_t)v.incB) >> 6;

  if (a.lfoTarget == LFO_TO_PITCH) {
    int32_t pitchMod = (lfo * a.lfoAmount) >> 10;
    base += pitchMod;
  }

  int32_t s;
  uint8_t mix = a.oscMix;

  if (fastPoly) {
    int32_t waveOffset = ((int32_t)(a.oscBOffset << 8) * mix) >> 7;
    int8_t detWeighted = ((int16_t)oscDet * mix) >> 7;
    uint32_t inc = detunePhaseInc(base, detWeighted);
    s = readWaveAtFast(v.phaseA, inc, wavePos + waveOffset, wtTable, wtSd);
    v.phaseB += applyPitchBend(v.incB);
  } else {
    int32_t oscA = readWaveAt(v.phaseA, base, wavePos, wtTable, wtSd);
    int32_t waveBPos = wavePos + (a.oscBOffset << 8);
    int32_t oscB = readWaveAt(v.phaseB, v.incB, waveBPos, wtTable, wtSd);
    s = ((oscA * (127 - mix)) + (oscB * mix)) >> 7;
  }

  if (a.subLevel > 0) {
    int16_t sub = (v.phaseA & 0x80000000UL) ? 22000 : -22000;
    s += (sub * a.subLevel) >> 8;
  }

  if (a.noiseLevel > 0) s += (fastNoise(v) * a.noiseLevel) >> 8;
  return constrain(s, -32768, 32767);
}

static inline int16_t ppgFilter(Voice &v, int16_t input) {
  int32_t cutoff = smoothCutoff.current >> 8;
  int32_t fe = processFilterEnv(v);
  cutoff += (fe * getAParamSeq(P_FILTER_ENV)) >> 15;
  cutoff += ((int32_t)v.velocity * getAParamSeq(P_VEL_FILTER)) >> 7;
  cutoff += ((int32_t)channelAftertouch * getAParamSeq(P_AFTERTOUCH_FILTER)) >> 7;
  cutoff += (((int32_t)v.note - 60) * getAParamSeq(P_KEYTRACK)) >> 5;

  if (getAParamSeq(P_LFO_TARGET) == LFO_TO_FILTER) {
    cutoff += (triangleLfo(v.lfoPhase) * getAParamSeq(P_LFO_AMOUNT)) >> 12;
  }

  cutoff = clip32(cutoff, 0, 127);
  int32_t f = 180 + cutoff * 120;
  int32_t q = 32767 - (getAParamSeq(P_RESONANCE) * 180);
  if (q < 6000) q = 6000;

  int32_t high = input - v.svfLow - ((v.svfBand * q) >> 15);
  v.svfBand += (f * high) >> 15;
  v.svfLow  += (f * v.svfBand) >> 15;
  v.svfLow  = constrain(v.svfLow, -32768, 32767);
  v.svfBand = constrain(v.svfBand, -32768, 32767);

  return ppgDrive(v.svfLow);
}

static inline int16_t renderVoice(Voice &v) {
  if (!v.active) return 0;

  int32_t s = oscRead(v);
  s = bitCrunch((int16_t)s);
  s = ppgFilter(v, (int16_t)s);

  int32_t env = processAmpEnv(v);
  if (v.ampStage == ENV_OFF) {
    v.active = false;
    return 0;
  }
  s = (s * env) >> 15;

  if (getAParamSeq(P_LFO_TARGET) == LFO_TO_AMP) {
    int32_t trem = 32767 + ((triangleLfo(v.lfoPhase) * getAParamSeq(P_LFO_AMOUNT)) >> 7);
    trem = clip32(trem, 0, 32767);
    s = (s * trem) >> 15;
  }

  int32_t velAmp = 64 + ((v.velocity * getAParamSeq(P_VEL_AMP)) >> 7);
  int32_t amp = (smoothVolume.current >> 8) * velAmp;
  amp = (amp * expressionLevel) >> 7;
  s = (s * amp) >> 14;

  return constrain(s, -32768, 32767);
}

struct StereoSample { int16_t l; int16_t r; };

static inline StereoSample renderVoiceStereo(Voice &v) {
  StereoSample out = {0, 0};
  int16_t s = renderVoice(v);
  int32_t pan = v.pan;
  if (getAParamSeq(P_LFO_TARGET) == LFO_TO_PAN) pan += (triangleLfo(v.lfoPhase) * getAParamSeq(P_LFO_AMOUNT)) >> 7;
  pan = clip32(pan, -32768, 32767);
  int32_t leftGain  = 32767 - ((pan + 32768) >> 1);
  int32_t rightGain = 32767 - leftGain;
  out.l = (s * leftGain) >> 15;
  out.r = (s * rightGain) >> 15;
  return out;
}

// ================================================================
// Voice allocation and MIDI note logic
// ================================================================
int findFreeVoice() {
  // v8.2 Voice Stealing V2:
  // 1) unused voice, 2) quietest releasing voice, 3) quietest active voice, 4) oldest fallback.
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].active) return i;
  }

  int bestRelease = -1;
  int32_t bestReleaseEnv = 0x7FFFFFFF;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].ampStage == ENV_RELEASE || voices[i].ampStage == ENV_OFF) {
      if (voices[i].ampEnv < bestReleaseEnv) {
        bestReleaseEnv = voices[i].ampEnv;
        bestRelease = i;
      }
    }
  }
  if (bestRelease >= 0) { voiceSteals++; return bestRelease; }

  int quietest = 0;
  int32_t quietestLevel = 0x7FFFFFFF;
  uint32_t oldestAge = 0xFFFFFFFF;
  int oldest = 0;
  for (int i = 0; i < NUM_VOICES; i++) {
    int32_t level = ((int32_t)voices[i].ampEnv * (int32_t)voices[i].velocity) >> 7;
    if (level < quietestLevel) { quietestLevel = level; quietest = i; }
    if (voices[i].age < oldestAge) { oldestAge = voices[i].age; oldest = i; }
  }
  voiceSteals++;
  return quietestLevel < 8000 ? quietest : oldest;
}

void envNoteOn(Voice &v) {
  v.ampStage = ENV_ATTACK; v.filtStage = ENV_ATTACK; v.waveStage = ENV_ATTACK;
  v.ampEnv = 0; v.filtEnv = 0; v.waveEnv = 0;
}

void envNoteOff(Voice &v) {
  v.ampStage = ENV_RELEASE; v.filtStage = ENV_RELEASE; v.waveStage = ENV_RELEASE;
}

void startVoice(uint8_t idx, uint8_t note, uint8_t vel, bool resetPhase) {
  Voice &v = voices[idx];
  uint8_t playMode = getAParam(P_PLAY_MODE);
  uint32_t newInc = notePhaseInc[note];

  v.active = true;
  v.note = note;
  v.velocity = vel;
  v.age = voiceAgeCounter++;
  v.noteOnMillis = millis();
  v.targetInc = newInc;

  if (resetPhase) {
    v.phaseA = 0;
    v.phaseB = 0x40000000UL;

    // POLY GLIDE:
    // In Play Mode 3, a new poly voice starts at the previously played pitch
    // and glides toward its own targetInc. Normal POLY/MONO/UNISON start
    // directly at the target pitch.
    if (playMode == 3 && lastPlayedIncValid && getAParam(P_GLIDE) > 0) {
      v.incA = lastPlayedIncForPolyGlide;
    } else {
      v.incA = v.targetInc;
    }

    v.incB = detunePhaseInc(v.incA, getAParam(P_OSC_DETUNE));
    v.svfLow = 0;
    v.svfBand = 0;
    v.lfoPhase = 0;
    v.waveLfoPhase = 0;
    v.lfoRandomLastPhase = 0;
    v.lfoRandomValue = 0;
    v.noiseSeed = 0x12345678UL ^ ((uint32_t)note << 8) ^ millis();
    v.drift = (int8_t)((v.noiseSeed >> 24) % 7) - 3; // tiny analog-style per-voice drift
    v.sustained = false;
    v.pan = ((idx % 2) ? getAParam(P_PAN_SPREAD) : -getAParam(P_PAN_SPREAD)) << 8;
    envNoteOn(v);
  }

  lastPlayedIncForPolyGlide = newInc;
  lastPlayedIncValid = true;
}

void pushHeldNote(uint8_t note) {
  for (int i = 0; i < heldCount; i++) if (heldNotes[i] == note) return;
  if (heldCount < 16) heldNotes[heldCount++] = note;
}

void removeHeldNote(uint8_t note) {
  for (int i = 0; i < heldCount; i++) if (heldNotes[i] == note) {
    for (int j = i; j < heldCount - 1; j++) heldNotes[j] = heldNotes[j + 1];
    heldCount--; return;
  }
}

void monoNoteOn(uint8_t note, uint8_t vel) {
  pushHeldNote(note);
  Voice &v = voices[monoVoice];
  bool wasActive = v.active;
  startVoice(monoVoice, note, vel, !wasActive || getAParam(P_GLIDE) == 0);
}

void monoNoteOff(uint8_t note) {
  removeHeldNote(note);
  if (heldCount > 0) {
    uint8_t last = heldNotes[heldCount - 1];
    voices[monoVoice].note = last;
    voices[monoVoice].targetInc = notePhaseInc[last];
  } else envNoteOff(voices[monoVoice]);
}

void audioSustainRelease() {
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].sustained) {
      voices[i].sustained = false;
      envNoteOff(voices[i]);
    }
  }
}

void resetModSequencer();

void prepareForProgramLoad() {
  // Shared front-panel/MIDI Program Change protection.
  // Flush stale note events, force all voices off on Core 1, reset performance controllers
  // and mute the output shortly while NVS parameters are copied into the live audio set.
  channelAftertouch = 0;
  modWheel = 0;
  expressionLevel = 127;
  sustainPedal = false;
  heldCount = 0;
  arpCount = 0;
  arpIndex = 0;
  arpDir = 1;
  currentArpNote = 255;
  resetModSequencer();
  queuePanicForProgramChange();
  programChangeMuteUntil = millis() + 80;
  programChangeCount++;
}

void safeLoadProgram(uint8_t number) {
  static uint32_t lastSafeLoadMs = 0;
  static uint8_t lastSafeLoadNumber = 255;

  number &= 127;
  uint32_t now = millis();

  // Ignore accidental duplicate program requests inside the mute window.
  if (number == lastSafeLoadNumber && (now - lastSafeLoadMs) < 120) return;

  lastSafeLoadMs = now;
  lastSafeLoadNumber = number;

  prepareForProgramLoad();
  loadProgram(number);
}

// Forward declarations for Arp interaction
void arpAddNote(uint8_t note, uint8_t vel);
void arpRemoveNote(uint8_t note);
void audioHandleNoteOff(uint8_t note);
void audioHandleNoteOn(uint8_t note, uint8_t vel) {
  if (vel == 0) { audioHandleNoteOff(note); return; }
  if (getAParam(P_PLAY_MODE) == 1) { monoNoteOn(note, vel); return; }

  int v = findFreeVoice();
  startVoice(v, note, vel, true);
}

void audioHandleNoteOff(uint8_t note) {
  if (getAParam(P_PLAY_MODE) == 1) { monoNoteOff(note); return; }
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].active && voices[i].note == note) {
      if (sustainPedal) voices[i].sustained = true;
      else envNoteOff(voices[i]);
    }
  }
}

void handleNoteOff(byte ch, byte note, byte vel);

// ================================================================
// Simple 16-step modulation sequencer, Core 0 only
// ================================================================
#define MODSEQ_STEPS 16
uint8_t modSeqValues[MODSEQ_STEPS] = {0, 32, 64, 96, 127, 96, 64, 32, 0, 48, 96, 127, 96, 48, 24, 12};
uint8_t modSeqEditStep = 0;  // UI editor cursor for modSeqValues[]
uint8_t modSeqIndex = 0;
int8_t modSeqDir = 1;
uint32_t modSeqLastStep = 0;

// ================================================================
// Programs
// ================================================================
void syncProgramToAudio() {
  normalizeProgramCompatibility(currentProgram);
  globalLfoShape = currentProgram.lfoShape;
  portENTER_CRITICAL(&paramMux);
  for (int i = 0; i < PARAM_COUNT; i++) {
    audioParams.p[i] = currentProgram.param[i];
    audioParamsSeq.p[i] = currentProgram.param[i];
  }
  paramsDirty = true;
  portEXIT_CRITICAL(&paramMux);
}

void setParam(uint8_t id, uint8_t value) {
  if (id >= PARAM_COUNT) return;
  restoreCompareEditIfNeeded();
  if (id == P_WAVETABLE && value >= WT_VISIBLE_SLOTS) value = WT_LAST_SLOT;
  if (id == P_CLOCK_SOURCE) { setClockSourceGlobal(value, true); return; }

  bool arpHoldWasOnBeforeChange = (id == P_ARP_HOLD) && switchIsOn(getAParam(P_ARP_HOLD));

  if (id == P_ARP_HOLD) value = switchValue(value >= 64);
  if (id == P_LFO_TARGET && value > 4) value = 4;
  if (id == P_PLAY_MODE && value > 3) value = 3;
  if (id == P_ARP_MODE && value > 4) value = 4;
  if (id == P_ARP_RATE && value > 5) value = 5;
  if (id == P_SEQ_MODE && value > 3) value = 3;
  if (id == P_ARP_OCTAVES && value > 4) value = 4;
  if (id == P_SEQ_RATE && value > 5) value = 5;
  if (id == P_SEQ_STEPS) {
    if (value < 1) value = 1;
    if (value > MODSEQ_STEPS) value = MODSEQ_STEPS;
    if (modSeqEditStep >= value) modSeqEditStep = value - 1;
  }
  if (id == P_SEQ_TARGET && value >= PARAM_COUNT) value = P_WAVE_POS;
  if (id == P_AFTERTOUCH_WAVE && value > 127) value = 127;
  if (id == P_AFTERTOUCH_FILTER && value > 127) value = 127;

  portENTER_CRITICAL(&paramMux);
  currentProgram.param[id] = value;
  audioParams.p[id] = value;
  audioParamsSeq.p[id] = value;
  paramsDirty = true;
  portEXIT_CRITICAL(&paramMux);

  if (id == P_TAP_TEMPO) bpm = 40 + ((uint16_t)value * 200) / 127;
  if (id == P_SEQ_MODE || id == P_SEQ_TARGET || id == P_SEQ_DEPTH || id == P_SEQ_STEPS) { modSeqIndex = 0; modSeqDir = 1; modSeqOffset = 0; modSeqAbsoluteTableActive = false; modSeqCurrentTarget = modSeqTargetParam(); updateModSeqAudioMirror(); } else { updateModSeqAudioMirror(); }

  if (id == P_ARP_HOLD && arpHoldWasOnBeforeChange && !switchIsOn(value)) {
    // ARP HOLD OFF releases all latched arp notes immediately.
    arpAllNotesOff();
    arpCount = 0;
    arpIndex = 0;
    arpDir = 1;
    currentArpNote = 255;
  }

  if (id == P_WAVETABLE) {
    int16_t sd = sdIndexForVisibleSlot(value);
    if (sd >= 0) requestSdWavetableCache((uint8_t)sd);
  }
}

// ================================================================
// Default MIDI CC map
// 255 = not assigned. These defaults are copied into every new Program.
// CC Learn can overwrite them per program.
// ================================================================
const uint8_t defaultCcMap[PARAM_COUNT] = {
  /* P_WAVETABLE            */ 22,
  /* P_WAVE_POS             */ 23,
  /* P_WAVE_MOD             */ 30,
  /* P_OSC_MIX              */ 26,
  /* P_OSC_DETUNE           */ 27,
  /* P_OSC_B_OFFSET         */ 31,
  /* P_CUTOFF               */ 102,
  /* P_RESONANCE            */ 103,
  /* P_FILTER_ENV           */ 106,
  /* P_ATTACK               */ 118,
  /* P_DECAY                */ 119,
  /* P_SUSTAIN              */ 75,
  /* P_RELEASE              */ 76,
  /* P_F_ATTACK             */ 109,
  /* P_F_DECAY              */ 110,
  /* P_F_SUSTAIN            */ 111,
  /* P_F_RELEASE            */ 112,
  /* P_WAVE_ENV             */ 86,
  /* P_WAVE_ENV_ATTACK      */ 89,
  /* P_WAVE_ENV_DECAY       */ 90,
  /* P_WAVE_ENV_SUSTAIN     */ 77,
  /* P_WAVE_ENV_RELEASE     */ 78,
  /* P_LFO_RATE             */ 28,
  /* P_LFO_AMOUNT           */ 52,
  /* P_LFO_TARGET           */ 29,
  /* P_WAVE_LFO_RATE        */ 20,
  /* P_WAVE_LFO_AMOUNT      */ 24,
  /* P_AFTERTOUCH_WAVE      */ 255,
  /* P_AFTERTOUCH_FILTER    */ 255,
  /* P_VEL_AMP              */ 115,
  /* P_VEL_FILTER           */ 105,
  /* P_KEYTRACK             */ 14,
  /* P_PAN_SPREAD           */ 255,
  /* P_DRIVE                */ 255,
  /* P_BITCRUSH             */ 255,
  /* P_VOLUME               */ 7,
  /* P_PLAY_MODE            */ 255,
  /* P_GLIDE                */ 255,
  /* P_BEND_RANGE           */ 255,
  /* P_UNISON_DETUNE        */ 255,
  /* P_SUB_LEVEL            */ 255,
  /* P_NOISE_LEVEL          */ 255,
  /* P_CHORUS               */ 255,
  /* P_ARP_MODE             */ 255,
  /* P_ARP_RATE             */ 255,
  /* P_ARP_OCTAVES          */ 255,
  /* P_ARP_HOLD             */ 255,
  /* P_CLOCK_SOURCE         */ 255,
  /* P_TAP_TEMPO            */ 255,
  /* P_SEQ_MODE             */ 255,
  /* P_SEQ_RATE             */ 255,
  /* P_SEQ_STEPS            */ 255,
  /* P_SEQ_TARGET           */ 255,
  /* P_SEQ_DEPTH            */ 255,
  /* P_MORPH_AMOUNT         */ 255,
  /* P_RANDOMIZE            */ 255
};

struct CcBankEntry { uint8_t cc; uint8_t param; };

const CcBankEntry midiCcBank0Map[] = {
  {22,P_WAVETABLE},{23,P_WAVE_POS},{30,P_WAVE_MOD},{26,P_OSC_MIX},{27,P_OSC_DETUNE},{31,P_OSC_B_OFFSET},
  {102,P_CUTOFF},{103,P_RESONANCE},{106,P_FILTER_ENV},{118,P_ATTACK},{119,P_DECAY},{75,P_SUSTAIN},{76,P_RELEASE},
  {109,P_F_ATTACK},{110,P_F_DECAY},{111,P_F_SUSTAIN},{112,P_F_RELEASE},{86,P_WAVE_ENV},{89,P_WAVE_ENV_ATTACK},
  {90,P_WAVE_ENV_DECAY},{77,P_WAVE_ENV_SUSTAIN},{78,P_WAVE_ENV_RELEASE},{28,P_LFO_RATE},{52,P_LFO_AMOUNT},
  {29,P_LFO_TARGET},{20,P_WAVE_LFO_RATE},{24,P_WAVE_LFO_AMOUNT},{115,P_VEL_AMP},{105,P_VEL_FILTER},{14,P_KEYTRACK}
};

const CcBankEntry midiCcBank1Map[] = {
  {102,P_AFTERTOUCH_WAVE},{103,P_AFTERTOUCH_FILTER},{86,P_PAN_SPREAD},{89,P_DRIVE},{90,P_BITCRUSH},
  {77,P_PLAY_MODE},{78,P_GLIDE},{26,P_BEND_RANGE},{28,P_UNISON_DETUNE},{52,P_SUB_LEVEL},{29,P_NOISE_LEVEL},
  {105,P_CHORUS},{106,P_ARP_MODE},{109,P_ARP_RATE},{110,P_ARP_OCTAVES},{111,P_ARP_HOLD},{112,P_CLOCK_SOURCE},
  {14,P_TAP_TEMPO},{115,P_SEQ_MODE},{118,P_SEQ_RATE},{119,P_SEQ_STEPS},{75,P_SEQ_TARGET},{76,P_SEQ_DEPTH},
  {22,P_MORPH_AMOUNT},{23,P_RANDOMIZE}
};

bool applyCcBankMap(uint8_t cc, uint8_t val) {
  const CcBankEntry *map = nullptr;
  uint8_t count = 0;
  if (midiCcBank == 0) {
    map = midiCcBank0Map;
    count = sizeof(midiCcBank0Map) / sizeof(midiCcBank0Map[0]);
  } else if (midiCcBank == 1) {
    map = midiCcBank1Map;
    count = sizeof(midiCcBank1Map) / sizeof(midiCcBank1Map[0]);
  } else return false;

  for (uint8_t i = 0; i < count; i++) {
    if (map[i].cc == cc) {
      setParam(map[i].param, val);
      return true;
    }
  }
  return false;
}

void initDefaultCcMap(Program &prog) {
  for (uint8_t i = 0; i < PARAM_COUNT; i++) {
    prog.ccMap[i] = defaultCcMap[i];
  }
}

void defaultProgram() {
  memset(&currentProgram, 0, sizeof(currentProgram));
  strncpy(currentProgram.name, "PPG INIT", PROGRAM_NAME_LEN);
  currentProgram.param[P_WAVETABLE] = 0;
  currentProgram.param[P_WAVE_POS] = 20;
  currentProgram.param[P_WAVE_MOD] = 30;
  currentProgram.param[P_OSC_MIX] = 64;
  currentProgram.param[P_OSC_DETUNE] = 12;
  currentProgram.param[P_OSC_B_OFFSET] = 3;
  currentProgram.param[P_CUTOFF] = 70;
  currentProgram.param[P_RESONANCE] = 25;
  currentProgram.param[P_FILTER_ENV] = 45;
  currentProgram.param[P_ATTACK] = 5;
  currentProgram.param[P_DECAY] = 45;
  currentProgram.param[P_SUSTAIN] = 100;
  currentProgram.param[P_RELEASE] = 99;
  currentProgram.param[P_F_ATTACK] = 2;
  currentProgram.param[P_F_DECAY] = 55;
  currentProgram.param[P_F_SUSTAIN] = 20;
  currentProgram.param[P_F_RELEASE] = 30;
  currentProgram.param[P_WAVE_ENV] = 55;
  currentProgram.param[P_WAVE_ENV_ATTACK] = 8;
  currentProgram.param[P_WAVE_ENV_DECAY] = 70;
  currentProgram.param[P_WAVE_ENV_SUSTAIN] = 15;
  currentProgram.param[P_WAVE_ENV_RELEASE] = 45;
  currentProgram.param[P_LFO_RATE] = 25;
  currentProgram.param[P_LFO_AMOUNT] = 15;
  currentProgram.param[P_LFO_TARGET] = LFO_TO_WAVE;
  currentProgram.param[P_WAVE_LFO_RATE] = 18;
  currentProgram.param[P_WAVE_LFO_AMOUNT] = 12;
  currentProgram.param[P_AFTERTOUCH_WAVE] = 20;
  currentProgram.param[P_AFTERTOUCH_FILTER] = 30;
  currentProgram.param[P_VEL_AMP] = 80;
  currentProgram.param[P_VEL_FILTER] = 30;
  currentProgram.param[P_KEYTRACK] = 40;
  currentProgram.param[P_PAN_SPREAD] = 30;
  currentProgram.param[P_DRIVE] = 25;
  currentProgram.param[P_BITCRUSH] = 8;
  currentProgram.param[P_VOLUME] = 80;
  currentProgram.param[P_PLAY_MODE] = 0;
  currentProgram.param[P_GLIDE] = 0;
  currentProgram.param[P_BEND_RANGE] = 2;
  currentProgram.param[P_UNISON_DETUNE] = 18;
  currentProgram.param[P_SUB_LEVEL] = 20;
  currentProgram.param[P_NOISE_LEVEL] = 0;
  currentProgram.param[P_CHORUS] = 25;
  currentProgram.param[P_ARP_MODE] = 0;
  currentProgram.param[P_ARP_RATE] = 3;
  currentProgram.param[P_ARP_OCTAVES] = 1;
  currentProgram.param[P_ARP_HOLD] = 1;
  currentProgram.param[P_CLOCK_SOURCE] = 1;
  currentProgram.param[P_TAP_TEMPO] = 51; // about 120 BPM
  currentProgram.param[P_SEQ_MODE] = 0;
  currentProgram.param[P_SEQ_RATE] = 3;
  currentProgram.param[P_SEQ_STEPS] = 16;
  currentProgram.param[P_SEQ_TARGET] = P_WAVE_POS;
  currentProgram.param[P_SEQ_DEPTH] = 32;
  currentProgram.param[P_MORPH_AMOUNT] = 0;
  currentProgram.param[P_RANDOMIZE] = 0;
  currentProgram.param[P_CLOCK_SOURCE] = globalClockSource;
  currentProgram.lfoShape = 0;
  initDefaultCcMap(currentProgram);
  syncProgramToAudio();
}

uint32_t fnv1aProgramBytes(const uint8_t *data, size_t len) {
  uint32_t crc = 2166136261UL; // FNV-1a basis
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    crc *= 16777619UL;
  }
  return crc;
}

uint32_t programCrc32(const Program &p) {
  return fnv1aProgramBytes((const uint8_t*)&p, sizeof(Program));
}

uint32_t programV1Crc32(const ProgramV1 &p) {
  return fnv1aProgramBytes((const uint8_t*)&p, sizeof(ProgramV1));
}

void convertProgramV1ToV2(const ProgramV1 &src, Program &dst) {
  memset(&dst, 0, sizeof(dst));
  strncpy(dst.name, src.name, PROGRAM_NAME_LEN - 1);
  for (uint8_t i = 0; i < PARAM_COUNT; i++) {
    dst.param[i] = src.param[i];
    dst.ccMap[i] = src.ccMap[i];
  }
  dst.lfoShape = 0; // Old presets default to TRI.
}

void normalizeProgramCompatibility(Program &p) {
  if (p.lfoShape > 5) p.lfoShape = 0;
}


void programCrcKey(uint8_t number, char *key, size_t keyLen) {
  snprintf(key, keyLen, "crc%03d", number);
}

void snapshotAudioParamsToCurrentProgram() {
  portENTER_CRITICAL(&paramMux);
  for (uint8_t i = 0; i < PARAM_COUNT; i++) {
    currentProgram.param[i] = audioParams.p[i];
  }
  currentProgram.param[P_CLOCK_SOURCE] = globalClockSource;
  currentProgram.lfoShape = globalLfoShape;
  portEXIT_CRITICAL(&paramMux);
}

// ================================================================
// User Programs on SD
// Factory presets 0..29 are stored in NVS.
// User presets 30..127 are stored on SD as fixed slot files:
//   /PPGPRESETS/U030.PPG ... /PPGPRESETS/U127.PPG
// ================================================================
struct SDUserProgramHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t crc;
};

static inline bool isUserProgramNumber(uint8_t number) {
  return number >= 30 && number < NUM_PROGRAMS;
}

void userProgramPath(uint8_t number, char *path, size_t pathLen) {
  snprintf(path, pathLen, "%s/U%03d.PPG", SD_PRESET_DIR, number);
}

bool writeUserProgramToSD(uint8_t number, const Program &program) {
  if (!sdCardOk) {
    sdPresetSaveErrors++;
    return false;
  }

  ensureSdDirectories();

  char path[96];
  userProgramPath(number, path, sizeof(path));

  // FILE_WRITE appends on some SD implementations, so remove first.
  if (SD.exists(path)) SD.remove(path);

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    sdPresetSaveErrors++;
    return false;
  }

  SDUserProgramHeader hdr;
  hdr.magic = SD_USER_PROGRAM_MAGIC;
  hdr.version = SD_USER_PROGRAM_VERSION;
  hdr.size = sizeof(Program);
  hdr.crc = programCrc32(program);

  bool ok = true;
  ok &= (f.write((const uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr));
  ok &= (f.write((const uint8_t*)&program, sizeof(Program)) == sizeof(Program));
  f.close();

  if (!ok) {
    sdPresetSaveErrors++;
    return false;
  }

  return true;
}

bool readUserProgramFromSD(uint8_t number, Program &program) {
  if (!sdCardOk) {
    sdPresetLoadErrors++;
    return false;
  }

  char path[96];
  userProgramPath(number, path, sizeof(path));

  File f = SD.open(path, FILE_READ);
  if (!f) {
    sdPresetLoadErrors++;
    return false;
  }

  SDUserProgramHeader hdr;
  bool ok = (f.read((uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr));
  ok &= (hdr.magic == SD_USER_PROGRAM_MAGIC);

  if (ok && hdr.version == 2 && hdr.size == sizeof(Program)) {
    Program tmp;
    ok &= (f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp));
    f.close();
    ok &= (hdr.crc == programCrc32(tmp));
    if (ok) {
      normalizeProgramCompatibility(tmp);
      program = tmp;
      //Serial.print("Loaded SD user program V2: ");
      //Serial.println(path);
      return true;
    }
  } else if (ok && hdr.version == 1 && hdr.size == sizeof(ProgramV1)) {
    ProgramV1 oldp;
    ok &= (f.read((uint8_t*)&oldp, sizeof(oldp)) == sizeof(oldp));
    f.close();
    ok &= (hdr.crc == programV1Crc32(oldp));
    if (ok) {
      convertProgramV1ToV2(oldp, program);
      //Serial.print("Loaded SD user program V1->V2: ");
      //Serial.println(path);
      return true;
    }
  } else {
    f.close();
  }

  sdPresetLoadErrors++;
  Serial.print("USER LOAD FAIL: ");
  Serial.println(path);
  return false;
}

bool readProgramNameForBrowser(uint8_t number, char *out, size_t outLen) {
  if (outLen == 0) return false;
  out[0] = 0;
  Program tmp;

  if (number < 30) {
    char key[16];
    char crcKey[16];
    snprintf(key, sizeof(key), "prog%03d", number);
    programCrcKey(number, crcKey, sizeof(crcKey));

    size_t len = prefs.getBytesLength(key);
    uint32_t storedCrc = prefs.getUInt(crcKey, 0xFFFFFFFFUL);

    if (len == sizeof(Program)) {
      prefs.getBytes(key, &tmp, sizeof(Program));
      uint32_t actualCrc = programCrc32(tmp);
      if (storedCrc == 0xFFFFFFFFUL || storedCrc == actualCrc) {
        strncpy(out, tmp.name, outLen - 1);
        out[outLen - 1] = 0;
        return true;
      }
    } else if (len == sizeof(ProgramV1)) {
      ProgramV1 oldp;
      prefs.getBytes(key, &oldp, sizeof(ProgramV1));
      uint32_t actualCrc = programV1Crc32(oldp);
      if (storedCrc == 0xFFFFFFFFUL || storedCrc == actualCrc) {
        strncpy(out, oldp.name, outLen - 1);
        out[outLen - 1] = 0;
        return true;
      }
    }

    snprintf(out, outLen, "FACTORY %03d", number);
    return true;
  }

  if (!sdCardOk) return false;

  char path[96];
  userProgramPath(number, path, sizeof(path));

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  SDUserProgramHeader hdr;
  bool ok = true;
  ok &= (f.read((uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr));
  ok &= (hdr.magic == SD_USER_PROGRAM_MAGIC);

  if (ok && hdr.version == 2 && hdr.size == sizeof(Program)) {
    ok &= (f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp));
    ok &= (hdr.crc == programCrc32(tmp));
  } else if (ok && hdr.version == 1 && hdr.size == sizeof(ProgramV1)) {
    ProgramV1 oldp;
    ok &= (f.read((uint8_t*)&oldp, sizeof(oldp)) == sizeof(oldp));
    ok &= (hdr.crc == programV1Crc32(oldp));
    if (ok) convertProgramV1ToV2(oldp, tmp);
  } else {
    ok = false;
  }
  f.close();

  if (!ok) return false;

  strncpy(out, tmp.name, outLen - 1);
  out[outLen - 1] = 0;
  if (strlen(out) == 0) snprintf(out, outLen, "USER %03d", number);
  return true;
}

void processPresetBrowserNameScan() {
  if (!browserScanActive) return;
  uint8_t processed = 0;
  while (browserScanIndex < NUM_PROGRAMS && processed < 2) {
    uint8_t i = browserScanIndex++;

    if (i >= 30) {
      browserProgramNames[i][0] = 0;
      browserProgramValid[i] = false;

      if (readProgramNameForBrowser(i, browserProgramNames[i], PROGRAM_NAME_LEN)) {
        browserProgramValid[i] = true;
      }
    }
    processed++;
  }
  if (browserScanIndex >= NUM_PROGRAMS) {
    browserScanActive = false;
    browserNamesDirty = false;
    browserCacheBuildCount++;
  }
}

const char* browserProgramDisplayName(uint8_t programNumber) {
  if (programNumber == currentProgramNumber) {
    // Show edited/current name immediately even before it is saved.
    if (strlen(currentProgram.name) > 0) return currentProgram.name;
  }

  if (programNumber < NUM_PROGRAMS && browserProgramValid[programNumber]) {
    return browserProgramNames[programNumber];
  }

  if (programNumber >= 30 && browserScanActive) return "SCANNING";
  return programNumber < 30 ? "FACTORY" : "EMPTY";
}

void saveProgram(uint8_t number) {
  number &= 127;
  snapshotAudioParamsToCurrentProgram();
  bool ok = false;
  size_t written = 0;

  if (isUserProgramNumber(number)) {
    // User programs live on SD, not in NVS.
    ok = writeUserProgramToSD(number, currentProgram);
    written = ok ? sizeof(Program) : 0;
  } else {
    // Factory/showcase presets 0..29 remain in NVS.
    char key[16]; snprintf(key, sizeof(key), "prog%03d", number);
    char crcKey[16]; programCrcKey(number, crcKey, sizeof(crcKey));

    prefs.remove(key);
    prefs.remove(crcKey);

    written = prefs.putBytes(key, &currentProgram, sizeof(Program));
    prefs.putUInt(crcKey, programCrc32(currentProgram));
    ok = (written == sizeof(Program));
  }
  storeMessage = true;
  storeMessageTime = millis();
}

void captureCompareOriginal() {
  compareOriginalProgram = currentProgram;
  compareAvailable = true;
  compareMode = false;
}

void restoreCompareEditIfNeeded() {
  if (!compareMode) return;

  currentProgram = compareEditProgram;
  syncProgramToAudio();
  compareMode = false;
  compareMessageUntil = millis() + 900;
}

void toggleCompareMode() {
  if (!compareAvailable) return;

  if (compareMode) {
    currentProgram = compareEditProgram;
    syncProgramToAudio();
    compareMode = false;
  } else {
    compareEditProgram = currentProgram;
    currentProgram = compareOriginalProgram;
    syncProgramToAudio();
    compareMode = true;
  }

  compareMessageUntil = millis() + 1200;
}

void loadProgram(uint8_t number) {
  number &= 127;
  bool loaded = false;
  if (isUserProgramNumber(number)) {
    // User programs are loaded from SD.
    Program tmp;
    if (readUserProgramFromSD(number, tmp)) {
      currentProgram = tmp;
      loaded = true;
    }
  } else {
    // Factory/showcase presets 0..29 are loaded from NVS.
    char key[16]; snprintf(key, sizeof(key), "prog%03d", number);
    char crcKey[16]; programCrcKey(number, crcKey, sizeof(crcKey));

    size_t len = prefs.getBytesLength(key);
    uint32_t storedCrc = prefs.getUInt(crcKey, 0xFFFFFFFFUL);

    if (len == sizeof(Program)) {
      Program tmp;
      prefs.getBytes(key, &tmp, sizeof(Program));
      uint32_t actualCrc = programCrc32(tmp);

      if (storedCrc == 0xFFFFFFFFUL || storedCrc == actualCrc) {
        normalizeProgramCompatibility(tmp);
        currentProgram = tmp;
        loaded = true;
      } else {
        presetCrcErrors++;
      }
    } else if (len == sizeof(ProgramV1)) {
      ProgramV1 oldp;
      prefs.getBytes(key, &oldp, sizeof(ProgramV1));
      uint32_t actualCrc = programV1Crc32(oldp);

      if (storedCrc == 0xFFFFFFFFUL || storedCrc == actualCrc) {
        convertProgramV1ToV2(oldp, currentProgram);
        loaded = true;
      } else {
        presetCrcErrors++;
      }
    }
  }

  if (!loaded) {
    defaultProgram();
    if (isUserProgramNumber(number)) {
      snprintf(currentProgram.name, PROGRAM_NAME_LEN, "EMPTY %03d", number);
    } else {
      snprintf(currentProgram.name, PROGRAM_NAME_LEN, "INIT %03d", number);
    }
  }
  currentProgram.param[P_CLOCK_SOURCE] = globalClockSource;
  currentProgramNumber = number;
  uint8_t t = selectedWavetableIndex();
  if (t >= PPG_WT_TABLES) {
    requestSdWavetableCache((uint8_t)(t - PPG_WT_TABLES));
  }
  syncProgramToAudio();
  resetMorphForLoadedProgram();
  setClockSourceGlobal(globalClockSource, false);
  captureCompareOriginal();
  bpm = 40 + ((uint16_t)getAParam(P_TAP_TEMPO) * 200) / 127;
  showProgramPreview();
}

void makePreset(uint8_t n, const char* name, uint8_t wave, uint8_t cutoff, uint8_t res, uint8_t env, uint8_t lfoAmt, uint8_t drive) {
  defaultProgram();
  strncpy(currentProgram.name, name, PROGRAM_NAME_LEN);
  currentProgram.param[P_WAVE_POS] = wave;
  currentProgram.param[P_CUTOFF] = cutoff;
  currentProgram.param[P_RESONANCE] = res;
  currentProgram.param[P_FILTER_ENV] = env;
  currentProgram.param[P_LFO_AMOUNT] = lfoAmt;
  currentProgram.param[P_DRIVE] = drive;
  syncProgramToAudio();
  saveProgram(n);
}

bool bankIsInitialized() { return prefs.getBool(FW_BANK_KEY, false); }
void markBankInitialized() { prefs.putBool(FW_BANK_KEY, true); }

void makePresetDetailed(uint8_t n, const char* name, uint8_t wave, uint8_t cutoff, uint8_t res, uint8_t env,
                        uint8_t lfoAmt, uint8_t drive, uint8_t attack, uint8_t release,
                        uint8_t chorus, uint8_t sub, uint8_t noise, uint8_t playMode) {
  defaultProgram();
  strncpy(currentProgram.name, name, PROGRAM_NAME_LEN);
  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
  currentProgram.param[P_WAVE_POS] = wave;
  currentProgram.param[P_CUTOFF] = cutoff;
  currentProgram.param[P_RESONANCE] = res;
  currentProgram.param[P_FILTER_ENV] = env;
  currentProgram.param[P_LFO_AMOUNT] = lfoAmt;
  currentProgram.param[P_DRIVE] = drive;
  currentProgram.param[P_ATTACK] = attack;
  currentProgram.param[P_RELEASE] = release;
  currentProgram.param[P_CHORUS] = chorus;
  currentProgram.param[P_SUB_LEVEL] = sub;
  currentProgram.param[P_NOISE_LEVEL] = noise;
  currentProgram.param[P_PLAY_MODE] = playMode;
  syncProgramToAudio();
  saveProgram(n);
}

void makeShowcasePreset(uint8_t n, const char* name, uint8_t table, uint8_t wave, uint8_t cutoff, uint8_t res,
                        uint8_t filterEnv, uint8_t waveEnv, uint8_t waveLfo, uint8_t attack, uint8_t decay,
                        uint8_t sustain, uint8_t release, uint8_t chorus, uint8_t drive, uint8_t sub,
                        uint8_t noise, uint8_t playMode, uint8_t arpMode, uint8_t seqMode) {
  defaultProgram();
  strncpy(currentProgram.name, name, PROGRAM_NAME_LEN);
  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
  currentProgram.param[P_WAVETABLE] = table;
  currentProgram.param[P_WAVE_POS] = wave;
  currentProgram.param[P_CUTOFF] = cutoff;
  currentProgram.param[P_RESONANCE] = res;
  currentProgram.param[P_FILTER_ENV] = filterEnv;
  currentProgram.param[P_WAVE_ENV] = waveEnv;
  currentProgram.param[P_WAVE_LFO_AMOUNT] = waveLfo;
  currentProgram.param[P_ATTACK] = attack;
  currentProgram.param[P_DECAY] = decay;
  currentProgram.param[P_SUSTAIN] = sustain;
  currentProgram.param[P_RELEASE] = release;
  currentProgram.param[P_CHORUS] = chorus;
  currentProgram.param[P_DRIVE] = drive;
  currentProgram.param[P_SUB_LEVEL] = sub;
  currentProgram.param[P_NOISE_LEVEL] = noise;
  currentProgram.param[P_PLAY_MODE] = playMode;
  currentProgram.param[P_ARP_MODE] = arpMode;
  currentProgram.param[P_SEQ_MODE] = seqMode;
  if (arpMode) { currentProgram.param[P_ARP_RATE] = 3; currentProgram.param[P_ARP_HOLD] = 1; currentProgram.param[P_ARP_OCTAVES] = 2; }
  if (seqMode) { currentProgram.param[P_SEQ_TARGET] = P_WAVE_POS; currentProgram.param[P_SEQ_DEPTH] = 42; currentProgram.param[P_SEQ_RATE] = 3; }
  syncProgramToAudio();
  saveProgram(n);
}

void initFactoryBank() {
  // Factory Showcase Bank: 30 curated sounds demonstrating all uploaded PPG tables.
  makeShowcasePreset(0,  "INIT",          0,  4,  92, 10,  0,  0,  0,  2, 30,110, 25,  0,  8,  0,  0, 0, 0, 0);
  makeShowcasePreset(1,  "GLASS PAD",     4, 42,  62, 18, 46, 70, 20, 42, 75,105, 85, 62, 12,  0,  0, 0, 0, 0);
  makeShowcasePreset(2,  "WAVE STRINGS", 13, 34,  74, 20, 38, 64, 16, 28, 68,112, 72, 58, 14,  8,  0, 0, 0, 0);
  makeShowcasePreset(3,  "ICE CHOIR",     5, 46,  66, 24, 58, 76, 26, 36, 80,108, 96, 70, 10,  0,  0, 0, 0, 0);
  makeShowcasePreset(4,  "CRYSTAL BELL",  3, 54, 104, 16, 18, 48, 10,  0, 35, 60, 72, 42, 18,  0,  0, 0, 0, 0);
  makeShowcasePreset(5,  "HOLLOW PWM",    2, 28,  76, 18, 28, 32, 48,  6, 44, 96, 48, 40, 18,  4,  0, 0, 0, 0);
  makeShowcasePreset(6,  "DIGI ORGAN",   12, 16,  96,  8,  0,  0,  4,  0, 10,127, 18, 20, 12,  0,  0, 0, 0, 0);
  makeShowcasePreset(7,  "PPG BRASS",    19, 20,  72, 30, 70, 36,  6,  4, 52, 92, 38, 28, 24,  6,  0, 0, 0, 0);
  makeShowcasePreset(8,  "DIGI BASS",    21, 12,  48, 36, 70, 20,  0,  0, 28, 82, 18, 12, 36, 46,  0, 1, 0, 0);
  makeShowcasePreset(9,  "WAVE BASS",     0, 22,  52, 28, 62, 58,  8,  0, 36, 76, 22, 16, 30, 58,  0, 1, 0, 0);
  makeShowcasePreset(10, "INDUSTR BASS", 24, 38,  60, 40, 55, 24,  4,  0, 24, 70, 20,  8, 74, 45, 30, 1, 0, 0);
  makeShowcasePreset(11, "SYNC LEAD",     9, 40,  94, 20, 22, 30, 12,  0, 35, 90, 24, 24, 34,  0,  0, 2, 0, 0);
  makeShowcasePreset(12, "ICE LEAD",      4, 58, 108, 18, 20, 38, 10,  0, 28, 88, 26, 18, 44,  0,  0, 2, 0, 0);
  makeShowcasePreset(13, "UNISON LEAD",   0, 24,  96, 14, 18, 20,  4,  0, 30, 90, 24, 28, 36, 12,  0, 2, 0, 0);
  makeShowcasePreset(14, "METAL SOLO",    8, 48,  90, 22, 30, 42,  8,  0, 32, 82, 25, 18, 56,  0,  6, 2, 0, 0);
  makeShowcasePreset(15, "BERLIN SEQ",   16, 18,  78, 18, 30, 38, 18,  0, 30, 96, 30, 34, 20, 10,  0, 0, 1, 0);
  makeShowcasePreset(16, "PULSE SEQ",     1, 34,  82, 18, 34, 50, 20,  0, 28, 96, 28, 34, 24,  4,  0, 0, 1, 1);
  makeShowcasePreset(17, "WAVE RUNNER",  14, 10,  88, 16, 42, 80, 34,  0, 40,100, 42, 46, 30,  0,  0, 0, 1, 1);
  makeShowcasePreset(18, "MORPH DEMO",   29, 26,  74, 22, 48, 64, 24, 12, 55, 96, 70, 48, 18,  0,  0, 0, 0, 0);
  makeShowcasePreset(19, "VELOCITY DEMO",20, 20,  58, 26, 84, 30,  6,  0, 32, 92, 30, 22, 22, 28,  0, 0, 0, 0);
  makeShowcasePreset(20, "AFTERTOUCH",    6, 30,  72, 22, 45, 60, 10, 10, 45,102, 62, 36, 18,  0,  0, 0, 0, 0);
  makeShowcasePreset(21, "STEREO DEMO",  23, 44,  76, 18, 36, 54, 22, 20, 70,110, 82, 76, 12,  0,  0, 0, 0, 0);
  makeShowcasePreset(22, "CRYSTAL HEAV", 4,  52,  70, 16, 40, 82, 32, 55, 90,112,110, 85,  8,  0,  0, 0, 0, 0);
  makeShowcasePreset(23, "CYBER ORGAN",  12, 26,  98, 12, 10, 16,  2,  0, 12,127, 22, 34, 20,  0,  0, 0, 0, 0);
  makeShowcasePreset(24, "GHOST CHOIR",   5, 50,  64, 26, 60, 86, 36, 50, 88,106,120, 70,  4,  0,  0, 0, 0, 0);
  makeShowcasePreset(25, "FROZEN PLANET",23, 56,  58, 18, 42, 92, 20, 70,100,118,126, 95,  8,  0,  0, 0, 0, 0);
  makeShowcasePreset(26, "DIGITAL OCEAN",16, 40,  68, 20, 36, 72, 44, 35, 84,110,100, 80,  6,  0,  0, 0, 0, 0);
  makeShowcasePreset(27, "TERMINATOR",    8, 60,  82, 35, 45, 36, 10,  0, 35, 80, 22, 18, 82, 18, 18, 2, 0, 0);
  makeShowcasePreset(28, "ALIEN VOICE",  27, 42,  66, 28, 60, 74, 28, 18, 70,100, 80, 48, 16,  0,  4, 0, 0, 0);
  makeShowcasePreset(29, "PPG SHOWCASE", 29, 36,  78, 24, 66, 88, 38, 18, 70,105, 76, 64, 26, 10,  2, 0, 1, 1);

  for (int i = 30; i < NUM_PROGRAMS; i++) {
    char name[PROGRAM_NAME_LEN]; snprintf(name, PROGRAM_NAME_LEN, "USER %03d", i);
    makePreset(i, name, 20, 75, 20, 35, 10, 20);
  }
  markBankInitialized();
}

// ================================================================
// UI helpers
// ================================================================
uint8_t getSelectedParam() {
  uint8_t p = pageParams[uiPage][uiRow];
  return p == UI_PARAM_NONE ? 0 : p;
}

uint8_t countParamsOnPage(uint8_t page) {
  uint8_t c = 0;
  for (int i = 0; i < UI_PARAMS_PER_PAGE; i++) if (pageParams[page][i] != UI_PARAM_NONE) c++;
  return c;
}

static inline bool seqTargetIsTable() {
  return getAParam(P_SEQ_TARGET) == P_WAVETABLE;
}

const char* arpRateName(uint8_t v) {
  switch (v) {
    case 0: return "1/4";
    case 1: return "1/8";
    case 2: return "1/8T";
    case 3: return "1/16";
    case 4: return "1/16T";
    case 5: return "1/32";
    default: return "?";
  }
}

void startCcLearn(uint8_t param) {
  ccLearnMode = true;
  learnParam = param;
}

void cancelCcLearn() {
  ccLearnMode = false;
  storeMessage = true;
  storeMessageTime = millis();
}

// ================================================================
// memory diagnostics
// Must be declared before updateDisplay() for Arduino IDE 1.8.x.
// ================================================================
volatile uint32_t freeHeapLast = 0;
volatile uint32_t freePsramLast = 0;
volatile uint32_t largestHeapBlockLast = 0;

static inline void updateMemoryDiagnostics() {
  static uint32_t lastMemMs = 0;
  uint32_t now = millis();
  if (now - lastMemMs < 1000) return;
  lastMemMs = now;
  freeHeapLast = ESP.getFreeHeap();
  freePsramLast = ESP.getFreePsram();
  largestHeapBlockLast = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

// ================================================================
// Snapshot morphing and musical randomize
// ================================================================
Program morphProgramA;
Program morphProgramB;
bool morphAValid = false;
bool morphBValid = false;
uint8_t lastMorphAmount = 255;

uint8_t morphSource = 0;  // 0=AMOUNT, 1=MODWHEEL, 2=AFTERTOUCH
uint8_t morphEffectiveAmount = 0;
char morphNameA[PROGRAM_NAME_LEN] = "";
char morphNameB[PROGRAM_NAME_LEN] = "";
uint32_t morphCaptureMessageUntil = 0;
char morphCaptureMessage[18] = "";
static inline uint8_t lerpU7(uint8_t a, uint8_t b, uint8_t amount) {
  return a + (((int16_t)b - (int16_t)a) * amount) / 127;
}

bool morphProtectedParam(uint8_t p) {
  return p == P_MORPH_AMOUNT || p == P_RANDOMIZE || p == P_TAP_TEMPO || p == P_CLOCK_SOURCE;
}

const char* morphSourceName(uint8_t s) {
  switch (s) {
    case 1: return "MODWHEEL";
    case 2: return "AFTERTOUCH";
    default: return "AMOUNT";
  }
}

uint8_t currentMorphControlAmount() {
  if (morphSource == 1) return modWheel;
  if (morphSource == 2) return channelAftertouch;
  return getAParam(P_MORPH_AMOUNT);
}

void setMorphSource(uint8_t s) {
  if (s > 2) s = 0;
  morphSource = s;
  lastMorphAmount = 255;  // force morph refresh with new source
  saveSystemConfigToSD();
}

void stepMorphSource(int8_t delta) {
  int v = (int)morphSource + delta;
  if (v < 0) v = 0;
  if (v > 2) v = 2;
  setMorphSource((uint8_t)v);
}


void resetMorphForLoadedProgram() {
  // a newly loaded preset becomes Morph A automatically.
  // This prevents an old A/B morph pair from affecting a newly loaded program.
  currentProgram.param[P_MORPH_AMOUNT] = 0;

  portENTER_CRITICAL(&paramMux);
  audioParams.p[P_MORPH_AMOUNT] = 0;
  audioParamsSeq.p[P_MORPH_AMOUNT] = 0;
  paramsDirty = true;
  portEXIT_CRITICAL(&paramMux);

  memcpy(&morphProgramA, &currentProgram, sizeof(Program));
  strncpy(morphNameA, currentProgram.name, PROGRAM_NAME_LEN - 1);
  morphNameA[PROGRAM_NAME_LEN - 1] = 0;
  morphNameB[0] = 0;
  morphAValid = true;
  morphBValid = false;
  morphEffectiveAmount = 0;
  lastMorphAmount = 255;
}
void captureMorphA() {
  snapshotAudioParamsToCurrentProgram();
  memcpy(&morphProgramA, &currentProgram, sizeof(Program));
  morphProgramA.param[P_MORPH_AMOUNT] = 0;
  strncpy(morphNameA, currentProgram.name, PROGRAM_NAME_LEN - 1);
  morphNameA[PROGRAM_NAME_LEN - 1] = 0;
  morphAValid = true;
  lastMorphAmount = 255;  // force recalculation
  strncpy(morphCaptureMessage, "Captured A", sizeof(morphCaptureMessage) - 1);
  morphCaptureMessage[sizeof(morphCaptureMessage) - 1] = 0;
  morphCaptureMessageUntil = millis() + 900;
  storeMessage = true;
  storeMessageTime = millis();
  Serial.println("Morph A captured");
}
void captureMorphB() {
  snapshotAudioParamsToCurrentProgram();
  memcpy(&morphProgramB, &currentProgram, sizeof(Program));
  morphProgramB.param[P_MORPH_AMOUNT] = 127;
  strncpy(morphNameB, currentProgram.name, PROGRAM_NAME_LEN - 1);
  morphNameB[PROGRAM_NAME_LEN - 1] = 0;
  morphBValid = true;
  lastMorphAmount = 255;  // force recalculation
  strncpy(morphCaptureMessage, "Captured B", sizeof(morphCaptureMessage) - 1);
  morphCaptureMessage[sizeof(morphCaptureMessage) - 1] = 0;
  morphCaptureMessageUntil = millis() + 900;
  storeMessage = true;
  storeMessageTime = millis();
  Serial.println("Morph B captured");
}
void updateMorphEngine() {
  if (!morphAValid || !morphBValid) return;

  uint8_t amount = currentMorphControlAmount();
  if (amount == lastMorphAmount) return;
  lastMorphAmount = amount;
  morphEffectiveAmount = amount;

  portENTER_CRITICAL(&paramMux);
  for (int i = 0; i < PARAM_COUNT; i++) {
    if (morphProtectedParam(i)) continue;
    uint8_t v = lerpU7(morphProgramA.param[i], morphProgramB.param[i], amount);
    currentProgram.param[i] = v;
    audioParams.p[i] = v;
    audioParamsSeq.p[i] = v;
  }
  if (morphSource == 0) {
    audioParams.p[P_MORPH_AMOUNT] = amount;
    audioParamsSeq.p[P_MORPH_AMOUNT] = amount;
    currentProgram.param[P_MORPH_AMOUNT] = amount;
  }
  paramsDirty = true;
  portEXIT_CRITICAL(&paramMux);

  // Keep important derived/global states coherent after morph interpolation.
  globalLfoShape = currentProgram.lfoShape;
  updateModSeqAudioMirror();
}

void initCurrentProgram() {
  defaultProgram();
  snprintf(currentProgram.name, PROGRAM_NAME_LEN, "INIT EDIT");
  syncProgramToAudio();
}

uint8_t rndRange(uint8_t lo, uint8_t hi) {
  return lo + random((int)hi - (int)lo + 1);
}

void randomizeMusicalProgram() {
  // Conservative ranges to avoid silent, unstable or excessively harsh patches.
  setParam(P_WAVE_POS, rndRange(0, 60));
  setParam(P_WAVE_MOD, rndRange(0, 70));
  setParam(P_OSC_MIX, rndRange(25, 100));
  setParam(P_OSC_DETUNE, rndRange(0, 26));
  setParam(P_OSC_B_OFFSET, rndRange(0, 10));
  setParam(P_CUTOFF, rndRange(45, 105));
  setParam(P_RESONANCE, rndRange(5, 55));
  setParam(P_FILTER_ENV, rndRange(0, 85));
  setParam(P_ATTACK, rndRange(0, 45));
  setParam(P_DECAY, rndRange(20, 95));
  setParam(P_SUSTAIN, rndRange(35, 127));
  setParam(P_RELEASE, rndRange(15, 90));
  setParam(P_F_ATTACK, rndRange(0, 35));
  setParam(P_F_DECAY, rndRange(20, 105));
  setParam(P_F_SUSTAIN, rndRange(0, 80));
  setParam(P_F_RELEASE, rndRange(10, 90));
  setParam(P_WAVE_ENV, rndRange(0, 100));
  setParam(P_WAVE_ENV_ATTACK, rndRange(0, 45));
  setParam(P_WAVE_ENV_DECAY, rndRange(20, 110));
  setParam(P_WAVE_ENV_SUSTAIN, rndRange(0, 75));
  setParam(P_WAVE_ENV_RELEASE, rndRange(10, 90));
  setParam(P_LFO_RATE, rndRange(5, 55));
  setParam(P_LFO_AMOUNT, rndRange(0, 55));
  setParam(P_LFO_TARGET, rndRange(0, 4));
  setParam(P_WAVE_LFO_RATE, rndRange(5, 60));
  setParam(P_WAVE_LFO_AMOUNT, rndRange(0, 45));
  setParam(P_VEL_AMP, rndRange(50, 110));
  setParam(P_VEL_FILTER, rndRange(0, 70));
  setParam(P_KEYTRACK, rndRange(10, 70));
  setParam(P_PAN_SPREAD, rndRange(0, 60));
  setParam(P_DRIVE, rndRange(0, 55));
  setParam(P_BITCRUSH, rndRange(0, 36));
  setParam(P_SUB_LEVEL, rndRange(0, 45));
  setParam(P_NOISE_LEVEL, rndRange(0, 24));
  setParam(P_CHORUS, rndRange(0, 55));
  setParam(P_VOLUME, 100);
  snprintf(currentProgram.name, PROGRAM_NAME_LEN, "RANDOM EDIT");
  setParam(P_RANDOMIZE, 0);
}

// ================================================================
// User program save mode
// Factory presets 0..29 stay protected. User slots are 30..127.
// ================================================================
static inline bool isUserProgramSlot(uint8_t number) {
  return number >= 30 && number < NUM_PROGRAMS;
}

static inline uint8_t clampUserProgramSlot(uint8_t number) {
  if (number < 30) return 30;
  if (number >= NUM_PROGRAMS) return NUM_PROGRAMS - 1;
  return number;
}


int nameCharIndex(char c) {
  for (uint8_t i = 0; i < sizeof(NAME_CHARS) - 1; i++) {
    if (NAME_CHARS[i] == c) return i;
  }
  return 0;
}

char normalizeNameChar(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') return c;
  return ' ';
}

void normalizeProgramName() {
  for (uint8_t i = 0; i < PROGRAM_NAME_LEN - 1; i++) {
    if (currentProgram.name[i] == 0) currentProgram.name[i] = ' ';
    currentProgram.name[i] = normalizeNameChar(currentProgram.name[i]);
  }
  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
}
void startUserNameEdit() {
  strncpy(userNameEditBackup, currentProgram.name, PROGRAM_NAME_LEN - 1);
  userNameEditBackup[PROGRAM_NAME_LEN - 1] = 0;

  normalizeProgramName();
  userNameEditMode = true;
  userNameCursor = 0;
}
void cancelUserNameEdit() {
  strncpy(currentProgram.name, userNameEditBackup, PROGRAM_NAME_LEN - 1);
  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;

  userNameEditMode = false;
  userNameCursor = 0;
}

void moveNameCursor(int8_t delta) {
  int16_t c = userNameCursor + delta;
  if (c < 0) c = PROGRAM_NAME_LEN - 2;
  if (c > PROGRAM_NAME_LEN - 2) c = 0;
  userNameCursor = (uint8_t)c;
}

void stepNameChar(int8_t delta) {
  normalizeProgramName();
  int idx = nameCharIndex(currentProgram.name[userNameCursor]);
  int count = sizeof(NAME_CHARS) - 1;
  idx += delta;
  while (idx < 0) idx += count;
  while (idx >= count) idx -= count;
  currentProgram.name[userNameCursor] = NAME_CHARS[idx];
}

void drawUserNameEditPage() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 8);
  u8g2.print("EDIT PRESET NAME");
  u8g2.setCursor(0, 22);
  u8g2.print("U");
  if (userSaveSlot < 100) u8g2.print("0");
  if (userSaveSlot < 10) u8g2.print("0");
  u8g2.print(userSaveSlot);
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(0, 40);
  for (uint8_t i = 0; i < PROGRAM_NAME_LEN - 1; i++) {
    char c = currentProgram.name[i];
    if (c == 0) c = ' ';
    u8g2.print(c);
  }
  uint8_t cx = userNameCursor * 7;
  u8g2.drawHLine(cx, 43, 6);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 56);
  u8g2.print("Enc Cursor B5/6 Char");
  u8g2.setCursor(0, 64);
  u8g2.print("BTN8 Done LongEnc Cancel");
  u8g2.sendBuffer();
}

bool getUserSaveSlotExistingName(char *out, size_t outLen) {
  if (outLen == 0) return false;
  out[0] = 0;

  uint8_t slot = clampUserProgramSlot(userSaveSlot);
  if (!readProgramNameForBrowser(slot, out, outLen)) {
    out[0] = 0;
    return false;
  }

  return strlen(out) > 0;
}

void refreshUserSaveExistingName(bool force) {
  uint8_t slot = clampUserProgramSlot(userSaveSlot);

  if (!force && userSaveExistingCacheSlot == slot) {
    return;
  }

  userSaveExistingCacheSlot = slot;
  userSaveExistingName[0] = 0;
  userSaveExistingOccupied =
    getUserSaveSlotExistingName(userSaveExistingName, sizeof(userSaveExistingName));
}

void invalidateUserSaveExistingNameCache() {
  userSaveExistingCacheSlot = 255;
  userSaveExistingName[0] = 0;
  userSaveExistingOccupied = false;
}

bool userSaveSlotOccupied() {
  refreshUserSaveExistingName(false);
  return userSaveExistingOccupied;
}

void clearUserSaveOverwriteConfirm() {
  userSaveConfirmOverwrite = false;
}
void deleteNameChar() {
  size_t len = strlen(currentProgram.name);
  if (userNameCursor >= len) return;

  memmove(&currentProgram.name[userNameCursor],
          &currentProgram.name[userNameCursor + 1],
          len - userNameCursor);

  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
  if (len > 0 && len < PROGRAM_NAME_LEN) {
    currentProgram.name[len - 1] = 0;
  }
}
void clearProgramName() {
  memset(currentProgram.name, 0, PROGRAM_NAME_LEN);
  userNameCursor = 0;
}

void restoreUserSaveOriginalName() {
  strncpy(currentProgram.name, userSaveOriginalName, PROGRAM_NAME_LEN - 1);
  currentProgram.name[PROGRAM_NAME_LEN - 1] = 0;
}

void openUserSaveMode() {
  strncpy(userSaveOriginalName, currentProgram.name, PROGRAM_NAME_LEN - 1);
  userSaveOriginalName[PROGRAM_NAME_LEN - 1] = 0;

  userSaveMode = true;
  userNameEditMode = false;
  clearUserSaveOverwriteConfirm();

  if (isUserProgramSlot(currentProgramNumber)) userSaveSlot = currentProgramNumber;
  else userSaveSlot = 30;

  if (strncmp(currentProgram.name, "INIT", 4) == 0 || strlen(currentProgram.name) == 0) {
    snprintf(currentProgram.name, PROGRAM_NAME_LEN, "USER %03d", userSaveSlot);
  }

  normalizeProgramName();
  invalidateUserSaveExistingNameCache();
  refreshUserSaveExistingName(true);
}
void closeUserSaveMode() {
  if (userNameEditMode) cancelUserNameEdit();

  restoreUserSaveOriginalName();

  userSaveMode = false;
  clearUserSaveOverwriteConfirm();
  invalidateUserSaveExistingNameCache();
}
void moveUserSaveSlot(int8_t delta) {
  int16_t s = userSaveSlot + delta;
  if (s < 30) s = 30;
  if (s >= NUM_PROGRAMS) s = NUM_PROGRAMS - 1;

  uint8_t newSlot = (uint8_t)s;
  if (newSlot != userSaveSlot) {
    userSaveSlot = newSlot;
    clearUserSaveOverwriteConfirm();
    invalidateUserSaveExistingNameCache();
    refreshUserSaveExistingName(true);
  }
}
void saveCurrentProgramToUserSlot() {
  if (userNameEditMode) finishUserNameEdit();
  restoreCompareEditIfNeeded();

  userSaveSlot = clampUserProgramSlot(userSaveSlot);

  if (!userSaveConfirmOverwrite && userSaveSlotOccupied()) {
    userSaveConfirmOverwrite = true;
    Serial.print("Overwrite confirmation requested for user program ");
    Serial.print(userSaveSlot);
    Serial.print(": ");
    Serial.println(userSaveExistingName);
    return;
  }

  snapshotAudioParamsToCurrentProgram();

  if (strlen(currentProgram.name) == 0 || strspn(currentProgram.name, " ") == strlen(currentProgram.name)) {
    snprintf(currentProgram.name, PROGRAM_NAME_LEN, "USER %03d", userSaveSlot);
  }

  normalizeProgramName();
  saveProgram(userSaveSlot);

  if (userSaveSlot < NUM_PROGRAMS) {
    strncpy(browserProgramNames[userSaveSlot], currentProgram.name, PROGRAM_NAME_LEN - 1);
    browserProgramNames[userSaveSlot][PROGRAM_NAME_LEN - 1] = 0;
    browserProgramValid[userSaveSlot] = true;
  }

  userSaveExistingCacheSlot = userSaveSlot;
  strncpy(userSaveExistingName, currentProgram.name, PROGRAM_NAME_LEN - 1);
  userSaveExistingName[PROGRAM_NAME_LEN - 1] = 0;
  userSaveExistingOccupied = true;

  browserNamesDirty = false;
  browserScanActive = false;
  currentProgramNumber = userSaveSlot;
  strncpy(userSaveOriginalName, currentProgram.name, PROGRAM_NAME_LEN - 1);
  userSaveOriginalName[PROGRAM_NAME_LEN - 1] = 0;

  userSaveMode = false;
  clearUserSaveOverwriteConfirm();
  showProgramPreview();

  Serial.print("Saved user program ");
  Serial.println(userSaveSlot);
}
void drawUserSavePage() {
  if (userNameEditMode) {
    drawUserNameEditPage();
    return;
  }

  // cached slot name; no SD read on every OLED refresh.
  refreshUserSaveExistingName(false);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.setCursor(0, 8);
  if (userSaveConfirmOverwrite) u8g2.print("OVERWRITE USER?");
  else u8g2.print("SAVE USER TO SD");

  u8g2.setCursor(0, 20);
  u8g2.print("Slot: U");
  if (userSaveSlot < 100) u8g2.print("0");
  if (userSaveSlot < 10) u8g2.print("0");
  u8g2.print(userSaveSlot);

  u8g2.setCursor(0, 32);
  u8g2.print("Old:");
  u8g2.setCursor(30, 32);
  if (userSaveExistingOccupied) u8g2.print(userSaveExistingName);
  else u8g2.print("<empty>");

  u8g2.setCursor(0, 44);
  u8g2.print("New:");
  u8g2.setCursor(30, 44);
  u8g2.print(currentProgram.name);

  u8g2.setCursor(0, 56);
  if (userSaveConfirmOverwrite) u8g2.print("BTN8 Yes  BTN2 No");
  else u8g2.print("Enc/B5/B6 Slot  B1 Name");

  u8g2.setCursor(0, 64);
  if (userSaveConfirmOverwrite) u8g2.print("Overwrite existing slot");
  else if (userSaveExistingOccupied) u8g2.print("BTN8 asks overwrite");
  else u8g2.print("BTN8 Save");

  u8g2.sendBuffer();
}

// ================================================================
// SD Preset / Bank system
// ================================================================
const char* categoryName(uint8_t c) {
  switch (c) {
    case CAT_PAD: return "PAD";
    case CAT_LEAD: return "LEAD";
    case CAT_BASS: return "BASS";
    case CAT_SEQ: return "SEQ";
    case CAT_FX: return "FX";
    case CAT_ORGAN: return "ORGAN";
    default: return "MISC";
  }
}

struct SDPresetHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t crc;
};

struct SDPresetFileV1 {
  char name[24];
  uint8_t category;
  uint8_t reserved[7];
  ProgramV1 program;
  uint8_t morphA[PARAM_COUNT];
  uint8_t morphB[PARAM_COUNT];
};

struct SDPresetFile {
  char name[24];
  uint8_t category;
  uint8_t reserved[7];
  Program program;
  uint8_t morphA[PARAM_COUNT];
  uint8_t morphB[PARAM_COUNT];
};

struct SDBankHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t programSize;
  uint16_t programCount;
  uint16_t reserved;
  uint32_t crc;
};

uint32_t fnv1a32(const uint8_t *data, size_t len) {
  uint32_t crc = 2166136261UL;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    crc *= 16777619UL;
  }
  return crc;
}


void makeSafeFilename(const char *name, char *out, size_t outLen, const char *ext) {
  size_t pos = 0;
  for (const char *p = name; *p && pos < outLen - 6; ++p) {
    char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[pos++] = c;
    else if (c == ' ' || c == '_' || c == '-') out[pos++] = '_';
  }
  if (pos == 0) {
    strncpy(out, "PRESET", outLen);
    pos = strlen(out);
  } else {
    out[pos] = 0;
  }
  strncat(out, ext, outLen - strlen(out) - 1);
}

bool saveCurrentPresetToSD(const char *name, uint8_t category) {
  if (!sdCardOk) {
    sdPresetSaveErrors++;
    return false;
  }
  ensureSdDirectories();

  SDPresetFile body;
  memset(&body, 0, sizeof(body));
  strncpy(body.name, name && name[0] ? name : currentProgram.name, sizeof(body.name) - 1);
  body.category = category;
  body.program = currentProgram;

  for (int i = 0; i < PARAM_COUNT; i++) {
    body.morphA[i] = morphSnapshotA[i];
    body.morphB[i] = morphSnapshotB[i];
  }

  SDPresetHeader hdr;
  hdr.magic = SD_PRESET_MAGIC;
  hdr.version = SD_PRESET_VERSION;
  hdr.size = sizeof(SDPresetFile);
  hdr.crc = fnv1a32((const uint8_t*)&body, sizeof(body));

  char fname[40];
  char path[80];
  makeSafeFilename(body.name, fname, sizeof(fname), ".PPG");
  snprintf(path, sizeof(path), "%s/%s", SD_PRESET_DIR, fname);

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    sdPresetSaveErrors++;
    return false;
  }

  bool ok = true;
  ok &= (f.write((const uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr));
  ok &= (f.write((const uint8_t*)&body, sizeof(body)) == sizeof(body));
  f.close();

  if (!ok) {
    sdPresetSaveErrors++;
    return false;
  }

  Serial.print("Saved SD preset: ");
  Serial.println(path);
  return true;
}

bool loadPresetFromSD(const char *path) {
  if (!sdCardOk) {
    sdPresetLoadErrors++;
    return false;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    sdPresetLoadErrors++;
    return false;
  }

  SDPresetHeader hdr;
  bool ok = (f.read((uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr));
  ok &= (hdr.magic == SD_PRESET_MAGIC);

  Program loadedProgram;
  uint8_t loadedCategory = CAT_MISC;
  uint8_t tmpMorphA[PARAM_COUNT];
  uint8_t tmpMorphB[PARAM_COUNT];
  memset(tmpMorphA, 0, sizeof(tmpMorphA));
  memset(tmpMorphB, 0, sizeof(tmpMorphB));

  if (ok && hdr.version == 2 && hdr.size == sizeof(SDPresetFile)) {
    SDPresetFile body;
    ok &= (f.read((uint8_t*)&body, sizeof(body)) == sizeof(body));
    ok &= (fnv1a32((const uint8_t*)&body, sizeof(body)) == hdr.crc);
    if (ok) {
      loadedProgram = body.program;
      normalizeProgramCompatibility(loadedProgram);
      loadedCategory = body.category;
      for (int i = 0; i < PARAM_COUNT; i++) {
        tmpMorphA[i] = body.morphA[i];
        tmpMorphB[i] = body.morphB[i];
      }
    }
  } else if (ok && hdr.version == 1 && hdr.size == sizeof(SDPresetFileV1)) {
    SDPresetFileV1 oldBody;
    ok &= (f.read((uint8_t*)&oldBody, sizeof(oldBody)) == sizeof(oldBody));
    ok &= (fnv1a32((const uint8_t*)&oldBody, sizeof(oldBody)) == hdr.crc);
    if (ok) {
      convertProgramV1ToV2(oldBody.program, loadedProgram);
      loadedCategory = oldBody.category;
      for (int i = 0; i < PARAM_COUNT; i++) {
        tmpMorphA[i] = oldBody.morphA[i];
        tmpMorphB[i] = oldBody.morphB[i];
      }
    }
  } else {
    ok = false;
  }

  f.close();

  if (!ok) {
    sdPresetLoadErrors++;
    return false;
  }

  prepareForProgramLoad();
  currentProgram = loadedProgram;
  currentProgramCategory = loadedCategory;
  for (int i = 0; i < PARAM_COUNT; i++) {
    morphSnapshotA[i] = tmpMorphA[i];
    morphSnapshotB[i] = tmpMorphB[i];
  }

  syncProgramToAudio();
  showProgramPreview();
  Serial.print("Loaded SD preset: ");
  Serial.println(path);
  return true;
}


bool saveBankToSD(const char *filename) {
  if (!sdCardOk) {
    sdBankSaveErrors++;
    return false;
  }
  ensureSdDirectories();

  char path[96];
  snprintf(path, sizeof(path), "%s/%s", SD_BACKUP_DIR, filename && filename[0] ? filename : "BANK.PBK");

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    sdBankSaveErrors++;
    return false;
  }

  SDBankHeader hdr;
  hdr.magic = SD_BANK_MAGIC;
  hdr.version = SD_BANK_VERSION;
  hdr.programSize = sizeof(Program);
  hdr.programCount = NUM_PROGRAMS;
  hdr.reserved = 0;
  hdr.crc = 0;

  // For streaming simplicity, CRC is over available Program records as written.
  uint32_t crc = 2166136261UL;

  f.write((const uint8_t*)&hdr, sizeof(hdr));

  for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
    Program p;
    char key[16];
    snprintf(key, sizeof(key), "prog%03d", i);

    if (isUserProgramNumber(i)) {
      if (!readUserProgramFromSD(i, p)) {
        Program old = currentProgram;
        defaultProgram();
        snprintf(currentProgram.name, PROGRAM_NAME_LEN, "EMPTY %03d", i);
        p = currentProgram;
        currentProgram = old;
      }
    } else if (prefs.getBytesLength(key) == sizeof(Program)) {
      prefs.getBytes(key, &p, sizeof(Program));
    } else {
      Program old = currentProgram;
      defaultProgram();
      snprintf(currentProgram.name, PROGRAM_NAME_LEN, "INIT %03d", i);
      p = currentProgram;
      currentProgram = old;
    }

    const uint8_t *d = (const uint8_t*)&p;
    for (size_t k = 0; k < sizeof(Program); k++) {
      crc ^= d[k];
      crc *= 16777619UL;
    }
    f.write((const uint8_t*)&p, sizeof(Program));
  }

  hdr.crc = crc;
  f.seek(0);
  f.write((const uint8_t*)&hdr, sizeof(hdr));
  f.close();

  Serial.print("Saved SD bank: ");
  Serial.println(path);
  return true;
}

bool loadBankFromSD(const char *path) {
  if (!sdCardOk) {
    sdBankLoadErrors++;
    return false;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    sdBankLoadErrors++;
    return false;
  }

  SDBankHeader hdr;
  if (f.read((uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr)) {
    f.close();
    sdBankLoadErrors++;
    return false;
  }

  bool isV2 = (hdr.magic == SD_BANK_MAGIC && hdr.version == 2 && hdr.programSize == sizeof(Program) && hdr.programCount == NUM_PROGRAMS);
  bool isV1 = (hdr.magic == SD_BANK_MAGIC && hdr.version == 1 && hdr.programSize == sizeof(ProgramV1) && hdr.programCount == NUM_PROGRAMS);
  if (!isV2 && !isV1) {
    f.close();
    sdBankLoadErrors++;
    return false;
  }

  uint32_t crc = 2166136261UL;
  Program bankPrograms[NUM_PROGRAMS];

  for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
    Program p;

    if (isV2) {
      if (f.read((uint8_t*)&p, sizeof(Program)) != sizeof(Program)) {
        f.close();
        sdBankLoadErrors++;
        return false;
      }

      const uint8_t *d = (const uint8_t*)&p;
      for (size_t k = 0; k < sizeof(Program); k++) {
        crc ^= d[k];
        crc *= 16777619UL;
      }
      normalizeProgramCompatibility(p);
    } else {
      ProgramV1 oldp;
      if (f.read((uint8_t*)&oldp, sizeof(ProgramV1)) != sizeof(ProgramV1)) {
        f.close();
        sdBankLoadErrors++;
        return false;
      }

      const uint8_t *d = (const uint8_t*)&oldp;
      for (size_t k = 0; k < sizeof(ProgramV1); k++) {
        crc ^= d[k];
        crc *= 16777619UL;
      }
      convertProgramV1ToV2(oldp, p);
    }

    bankPrograms[i] = p;
  }

  f.close();

  if (crc != hdr.crc) {
    sdBankLoadErrors++;
    return false;
  }

  // CRC verified: now commit programs.
  for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
    Program &p = bankPrograms[i];

    if (isUserProgramNumber(i)) {
      writeUserProgramToSD(i, p);
    } else {
      char key[16];
      snprintf(key, sizeof(key), "prog%03d", i);
      prefs.putBytes(key, &p, sizeof(Program));

      char crcKey[16];
      programCrcKey(i, crcKey, sizeof(crcKey));
      prefs.putUInt(crcKey, programCrc32(p));
    }
  }

  safeLoadProgram(0);
  Serial.print("Loaded SD bank: ");
  Serial.println(path);
  return true;
}

// ================================================================
// Boot MIDI channel setup
// Hold BTN1 during boot to open this menu immediately.
// BTN5 = channel down, BTN6 = channel up, BTN8 = save and continue.
// ================================================================
void drawMidiChannelBootScreen(uint8_t ch) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 12);
  u8g2.print("MIDI CHANNEL SETUP");
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.setCursor(0, 34);
  u8g2.print("CHANNEL: ");
  u8g2.print(ch);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 52);
  u8g2.print("BTN5 -   BTN6 +");
  u8g2.setCursor(0, 64);
  u8g2.print("BTN8 SAVE");
  u8g2.sendBuffer();
}

// ================================================================
// Boot Splash Screen
// ================================================================
void drawSplashWave() {
  // Small stylized wavetable bars / wave hint
  const uint8_t baseY = 46;
  const uint8_t xs[16] = {15, 21, 27, 33, 39, 45, 51, 57, 63, 69, 75, 81, 87, 93, 99, 105};
  const uint8_t h[16]  = { 2,  5,  9, 13, 15, 13,  9,  5,  2,  5, 10, 13, 15, 12,  7,  3};

  for (uint8_t i = 0; i < 16; i++) {
    u8g2.drawBox(xs[i], baseY - h[i], 4, h[i]);
  }
}

static uint32_t lastDisplayRefreshMs = 0;
static inline bool displayRefreshDue() {
  uint32_t now = millis();
  if (now - lastDisplayRefreshMs >= 50) {
    lastDisplayRefreshMs = now;
    return true;
  }
  return false;
}

// ================================================================
// Control Task
// ================================================================
uint8_t lastRandomizeTrigger = 0;
void controlTask(void *param) {
  while (true) {
    pollButtons();
    pollEncoder();
    pollEncoderButton();
    processSdWavetableCacheRequest();
    processPresetBrowserNameScan();
    updateArp();
    updateModSequencer();
    updateMorphEngine();
    uint8_t randomizeNow = getAParam(P_RANDOMIZE);
    if (randomizeNow > 0 && lastRandomizeTrigger == 0) {
      // Stop all currently sounding/held notes before changing many sound parameters.
      // This avoids stuck voices or old envelopes continuing through a randomize.
      sustainPedal = false;
      heldCount = 0;
      arpAllNotesOff();
      allNotesOff();

      randomizeMusicalProgram();
    }
    lastRandomizeTrigger = randomizeNow;
    updateMidiClockMonitor();
    updateMemoryDiagnostics();
    if (displayRefreshDue()) updateDisplay();
    vTaskDelay(pdMS_TO_TICKS(1));
 }    
}

// ================================================================
// Setup
// ================================================================
TaskHandle_t midiTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;
void setup() {
  Serial.begin(115200);
  Serial.print("Free heap: "); Serial.println(ESP.getFreeHeap());
  Serial.print("Free PSRAM: "); Serial.println(ESP.getFreePsram());
  Serial.print("Largest free block: "); Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  delay(200);

  prefs.begin("ppg22", false);

  midiChannel = prefs.getUChar("midiCh", 1);
  if (midiChannel < 1 || midiChannel > 16) midiChannel = 1;
midiCcMode = prefs.getUChar("ccMode", 0);
  if (midiCcMode > 1) midiCcMode = 0;
  midiCcBank = prefs.getUChar("ccBank", 0);
  if (midiCcBank > 2) midiCcBank = 0;
  if (morphSource > 2) morphSource = 0;

  bootCounter = prefs.getUInt("bootCount", 0) + 1;
  prefs.putUInt("bootCount", bootCounter);
  Serial.print(FW_NAME); Serial.print(" "); Serial.print(FW_VERSION);
  Serial.print(" build "); Serial.print(FW_BUILD_DATE); Serial.print(" "); Serial.println(FW_BUILD_TIME);
  Serial.print("Boot #"); Serial.println(bootCounter);
  Serial.print("PSRAM size: "); Serial.println(ESP.getPsramSize());
  Serial.print("Free PSRAM: "); Serial.println(ESP.getFreePsram());

  SPI.begin(OLED_SCK, SD_MISO_PIN, OLED_MOSI, OLED_CS);
  u8g2.begin();

  // Boot menu: hold BTN1 while powering on/resetting.
  pinMode(BTN_1_PIN, INPUT_PULLUP);
  pinMode(BTN_5_PIN, INPUT_PULLUP);
  pinMode(BTN_6_PIN, INPUT_PULLUP);
  pinMode(BTN_8_PIN, INPUT_PULLUP);
  if (digitalRead(BTN_1_PIN) == LOW) {
    midiChannelBootMenu();
    bootMidiChannelChanged = true;
    bootMidiChannelValue = midiChannel;
  }
  showSplashScreen();
  u8g2.setFont(u8g2_font_6x10_tf);
  delay(1600);

  SerialMIDI.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleCC);
  MIDI.setHandleProgramChange(handlePC);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.setHandleAfterTouchChannel(handleAfterTouchChannel);
  MIDI.setHandleAfterTouchPoly(handleAfterTouchPoly);
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);
  MIDI.setHandleStop(handleStop);
  MIDI.setHandleContinue(handleContinue);
  MIDI.setHandleSongPosition(handleSPP);
  MIDI.setHandleSystemExclusive(handleSystemExclusive);

  setupButtons();
  setupEncoder();
  setupI2S();

  u8g2.clearBuffer(); u8g2.setCursor(0, 12); u8g2.print("Building tables...");
  u8g2.setCursor(0, 24); u8g2.print("Boot "); u8g2.print(bootCounter);
  u8g2.sendBuffer();
  buildNoteTable();
  buildPitchBendTables();
  buildLfoRateTable();
  buildWaveLfoRateTable();
  buildEnvRateTable();
  buildPPGStyleTables();

  u8g2.clearBuffer();
  u8g2.setCursor(0, 12); u8g2.print("Loading SD WT...");
  u8g2.sendBuffer();
  loadSdWavetables();
  ensureSdDirectories();

  if (!loadSystemConfigFromSD()) {
    Serial.println("System config: using NVS/default fallback");
    saveSystemConfigToSD();
  }

  if (bootMidiChannelChanged) {
    midiChannel = bootMidiChannelValue;
    if (midiChannel < 1 || midiChannel > 16) midiChannel = 1;
    saveSystemConfigToSD();
    Serial.print("Boot MIDI channel saved to CONFIG.TXT: ");
    Serial.println(midiChannel);
  }

  setClockSourceGlobal(globalClockSource, false);

  defaultProgram();
  if (!bankIsInitialized()) {
    initFactoryBank();
    markBankInitialized();
  }

  loadProgram(0);
  setClockSourceGlobal(globalClockSource, false);

  memset(chorusBufL, 0, sizeof(chorusBufL));
  memset(chorusBufR, 0, sizeof(chorusBufR));

  audioEventQueue = xQueueCreate(64, sizeof(AudioEvent));
  if (!audioEventQueue) {
    Serial.println("ERROR: audioEventQueue allocation failed");
  }

  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "ControlTask", 8192, NULL, 2, NULL, 0);  
  xTaskCreatePinnedToCore(midiTask, "MidiTask", 4096, NULL, 5, &midiTaskHandle,0);  
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
