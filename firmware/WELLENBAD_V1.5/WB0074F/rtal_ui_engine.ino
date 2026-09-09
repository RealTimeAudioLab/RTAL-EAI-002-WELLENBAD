#include "RTALUILayout.h"

#if RTAL_UI_ENGINE_ENABLED

namespace {

struct RtalUiState {
  bool active = RTAL_UI_ENGINE_DEFAULT_ACTIVE != 0;
  RtalScreenId screen = RtalScreenId::PlayHome;
  RtalScreenId lastSound = RtalScreenId::SoundOsc1;
  RtalScreenId lastEdit = RtalScreenId::EditFilter;
  RtalScreenId lastMod = RtalScreenId::ModLfo;
  RtalScreenId lastPlay = RtalScreenId::PlayPerformance;
  RtalScreenId lastFx = RtalScreenId::FxMain;
  RtalScreenId lastSystem = RtalScreenId::SystemMidi;
  uint8_t selected = 0;
  uint8_t selectedMultiPart = 0;
  uint8_t multiNameCursor = 0;
  bool waveMonitorTableSelect = false;
  char message[24] = {0};
  uint32_t messageUntil = 0;
};

RtalUiState rtalUi;

enum class RtalUiLevel : uint8_t { Performance, Edit, System };

struct RtalScreenDescriptor {
  RtalScreenId id;
  const char* section;
  const char* title;
  const uint8_t* params;
  uint8_t paramCount;
  uint8_t flowStage;
};

const uint8_t rtalParamsOsc1[]  = { P_WAVETABLE, P_WAVE_POS, P_WAVE_MOD };
const uint8_t rtalParamsOsc2[]  = { P_OSC_B_OFFSET, P_OSC_DETUNE };
const uint8_t rtalParamsMixer[] = { P_OSC_MIX };
const uint8_t rtalParamsNoise[] = { P_NOISE_LEVEL };
const uint8_t rtalParamsSub[]   = { P_SUB_LEVEL };
const uint8_t rtalParamsOutput[]= { P_VOLUME, P_PAN_SPREAD };

const uint8_t rtalParamsFilter[]    = { P_CUTOFF, P_RESONANCE, P_FILTER_ENV };
const uint8_t rtalParamsAmpEnv[]    = { P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE };
const uint8_t rtalParamsFilterEnv[] = { P_F_ATTACK, P_F_DECAY, P_F_SUSTAIN, P_F_RELEASE };
const uint8_t rtalParamsWaveEnv[]   = { P_WAVE_ENV, P_WAVE_ENV_ATTACK, P_WAVE_ENV_DECAY, P_WAVE_ENV_SUSTAIN, P_WAVE_ENV_RELEASE };

const uint8_t rtalParamsLfo[]         = { P_LFO_RATE, P_LFO_AMOUNT, P_LFO_TARGET, P_LFO_SHAPE };
const uint8_t rtalParamsWaveLfo[]     = { P_WAVE_LFO_RATE, P_WAVE_LFO_AMOUNT };
const uint8_t rtalParamsModPerf[]     = { P_AFTERTOUCH_WAVE, P_AFTERTOUCH_FILTER, P_VEL_AMP, P_VEL_FILTER, P_KEYTRACK };
const uint8_t rtalParamsMorph[]       = { P_MORPH_AMOUNT, P_RANDOMIZE };
const uint8_t rtalParamsPlayPerf[]    = { P_PLAY_MODE, P_GLIDE, P_BEND_RANGE, P_UNISON_DETUNE, P_PAN_SPREAD };
const uint8_t rtalParamsArp[]         = { P_ARP_MODE, P_ARP_RATE, P_ARP_OCTAVES, P_ARP_HOLD, P_CLOCK_SOURCE, P_TAP_TEMPO };
const uint8_t rtalParamsSeq[]         = { P_SEQ_MODE, P_SEQ_RATE, P_SEQ_STEPS, P_SEQ_TARGET, P_SEQ_DEPTH };
const uint8_t rtalParamsSeqEdit[]     = { UI_PARAM_SEQ_TABLE_MODE, UI_PARAM_SEQ_STEP, UI_PARAM_SEQ_VALUE };
const uint8_t rtalParamsFx[]          = { P_CHORUS, P_DRIVE, P_BITCRUSH };

const RtalScreenDescriptor rtalScreens[] = {
  { RtalScreenId::SoundOsc1,    "SOUND", "OSC1",       rtalParamsOsc1,      3, 0 },
  { RtalScreenId::SoundOsc2,    "SOUND", "OSC2",       rtalParamsOsc2,      2, 1 },
  { RtalScreenId::SoundMixer,   "SOUND", "MIXER",      rtalParamsMixer,     1, 2 },
  { RtalScreenId::SoundNoise,   "SOUND", "NOISE",      rtalParamsNoise,     1, 3 },
  { RtalScreenId::SoundSub,     "SOUND", "SUB",        rtalParamsSub,       1, 4 },
  { RtalScreenId::SoundOutput,  "SOUND", "OUTPUT",     rtalParamsOutput,    2, 5 },
  { RtalScreenId::SoundWaveMon, "SOUND", "WAVE MON",   nullptr,             0, 0 },

  { RtalScreenId::EditFilter,   "EDIT",  "FILTER",     rtalParamsFilter,    3, 3 },
  { RtalScreenId::EditAmpEnv,   "EDIT",  "AMP ENV",    rtalParamsAmpEnv,    4, 4 },
  { RtalScreenId::EditFilterEnv,"EDIT",  "FILTER ENV", rtalParamsFilterEnv, 4, 3 },
  { RtalScreenId::EditWaveEnv,  "EDIT",  "WAVE ENV",   rtalParamsWaveEnv,   5, 0 },

  { RtalScreenId::ModLfo,         "MOD",   "LFO",         rtalParamsLfo,       4, 0 },
  { RtalScreenId::ModWaveLfo,     "MOD",   "WAVE LFO",    rtalParamsWaveLfo,   2, 1 },
  { RtalScreenId::ModPerformance, "MOD",   "PERFORMANCE", rtalParamsModPerf,   5, 2 },
  { RtalScreenId::ModMorph,       "MOD",   "MORPH",       rtalParamsMorph,     2, 5 },

  { RtalScreenId::FxMain,          "FX",    "EFFECTS",     rtalParamsFx,        3, 0 },

  { RtalScreenId::PlayPerformance, "PLAY",  "PERFORMANCE", rtalParamsPlayPerf,  5, 1 },
  { RtalScreenId::PlayArpeggiator, "PLAY",  "ARPEGGIATOR", rtalParamsArp,       6, 3 },
  { RtalScreenId::PlaySequencer,   "PLAY",  "SEQUENCER",   rtalParamsSeq,       5, 4 },
  { RtalScreenId::PlaySeqEdit,     "PLAY",  "SEQ EDIT",    rtalParamsSeqEdit,   3, 5 },
  { RtalScreenId::PlaySeqShow,     "PLAY",  "SEQ SHOW",    nullptr,             0, 6 }
};

const RtalScreenDescriptor* descriptorFor(RtalScreenId id) {
  for (const auto &screen : rtalScreens) if (screen.id == id) return &screen;
  return nullptr;
}

RtalScreenId nextSoundScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::SoundOsc1: return RtalScreenId::SoundOsc2;
    case RtalScreenId::SoundOsc2: return RtalScreenId::SoundMixer;
    case RtalScreenId::SoundMixer:return RtalScreenId::SoundNoise;
    case RtalScreenId::SoundNoise:return RtalScreenId::SoundSub;
    case RtalScreenId::SoundSub:  return RtalScreenId::SoundOutput;
    case RtalScreenId::SoundOutput: return RtalScreenId::SoundWaveMon;
    case RtalScreenId::SoundWaveMon: return RtalScreenId::SoundOsc1;
    default: return RtalScreenId::SoundOsc1;
  }
}

bool isSoundScreen(RtalScreenId id) {
  return id >= RtalScreenId::SoundOsc1 && id <= RtalScreenId::SoundWaveMon;
}

bool isEditScreen(RtalScreenId id) {
  return id >= RtalScreenId::EditFilter && id <= RtalScreenId::EditWaveEnv;
}

RtalScreenId nextEditScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::EditFilter:    return RtalScreenId::EditAmpEnv;
    case RtalScreenId::EditAmpEnv:    return RtalScreenId::EditFilterEnv;
    case RtalScreenId::EditFilterEnv: return RtalScreenId::EditWaveEnv;
    default: return RtalScreenId::EditFilter;
  }
}


bool isModScreen(RtalScreenId id) {
  return id >= RtalScreenId::ModLfo && id <= RtalScreenId::ModMorph;
}

RtalScreenId nextModScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::ModLfo: return RtalScreenId::ModWaveLfo;
    case RtalScreenId::ModWaveLfo: return RtalScreenId::ModPerformance;
    case RtalScreenId::ModPerformance: return RtalScreenId::ModMorph;
    default: return RtalScreenId::ModLfo;
  }
}

bool isPlayScreen(RtalScreenId id) {
  return id == RtalScreenId::PlayHome ||
         id == RtalScreenId::PlayPerformance ||
         id == RtalScreenId::PlayArpeggiator ||
         id == RtalScreenId::PlaySequencer ||
         id == RtalScreenId::PlaySeqEdit ||
         id == RtalScreenId::PlaySeqShow ||
         id == RtalScreenId::PlayMultiOverview ||
         id == RtalScreenId::PlayMultiPart;
}

RtalScreenId nextPlayScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::PlayHome: return RtalScreenId::PlayPerformance;
    case RtalScreenId::PlayPerformance: return RtalScreenId::PlayMultiOverview;
    case RtalScreenId::PlayMultiOverview: return RtalScreenId::PlayMultiPart;
    case RtalScreenId::PlayMultiPart: return RtalScreenId::PlayArpeggiator;
    case RtalScreenId::PlayArpeggiator: return RtalScreenId::PlaySequencer;
    case RtalScreenId::PlaySequencer: return RtalScreenId::PlaySeqEdit;
    case RtalScreenId::PlaySeqEdit: return RtalScreenId::PlaySeqShow;
    case RtalScreenId::PlaySeqShow: return RtalScreenId::PlayHome;
    default: return RtalScreenId::PlayHome;
  }
}

void drawTruncated(const char* text, uint8_t x, uint8_t y, uint8_t maxChars) {
  char buffer[32];
  if (!text) text = "";
  size_t len = strnlen(text, sizeof(buffer)-1);
  if (len <= maxChars) {
    u8g2.setCursor(x, y); u8g2.print(text); return;
  }
  if (maxChars < 2) return;
  memcpy(buffer, text, maxChars - 1);
  buffer[maxChars - 1] = '.';
  buffer[maxChars] = 0;
  u8g2.setCursor(x, y); u8g2.print(buffer);
}

static inline uint8_t rtalContextPart() {
  return rtalUi.selectedMultiPart < RTAL_MULTI_PART_COUNT ? rtalUi.selectedMultiPart : 0;
}

static inline uint8_t rtalContextParam(uint8_t id) {
  if (id >= PARAM_COUNT) return 0;
  // Clock and tempo are one shared master state. FIX3 persists that state per
  // Multi, but it must never be read from a stale per-Part Program copy.
  if (id == P_CLOCK_SOURCE || id == P_TAP_TEMPO) return getAParam(id);
  if (multiPerformanceContextActive()) return multiParam(rtalContextPart(), id);
  return getAParam(id);
}

const char* rtalContextWavetableName() {
  uint8_t t = rtalContextParam(P_WAVETABLE);
  if (t >= WT_VISIBLE_SLOTS) t = WT_LAST_SLOT;
  int16_t sd = sdIndexForVisibleSlot(t);
  if (sd >= 0) return sdWavetableNames[sd];
  if (t < RTAL_WT_TABLES) return RTAL_WT_NAMES[t];
  return "WT?";
}

uint8_t rtalDisplayedVoiceCountForPart(uint8_t partIndex) {
  uint8_t displayedVoices = 0;
  for (uint8_t i = 0; i < NUM_VOICES; ++i) {
    if (voices[i].active && voices[i].ampStage != ENV_OFF &&
        voices[i].ampStage != ENV_RELEASE && voices[i].partIndex == partIndex) {
      ++displayedVoices;
    }
  }
  return displayedVoices;
}

void buildProgramHeaderName(char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  if (multiPerformanceContextActive()) {
    snprintf(out, outSize, "M%03u %s", (unsigned)currentMultiSlot, currentMultiName);
    return;
  }
  if (currentProgramNumber >= 30) {
    snprintf(out, outSize, "U%03u %s", currentProgramNumber, currentProgram.name);
  } else {
    snprintf(out, outSize, "F%03u %s", currentProgramNumber, currentProgram.name);
  }
}

uint8_t rtalDisplayedVoiceCount() {
  uint8_t displayedVoices = 0;
  for (uint8_t i = 0; i < NUM_VOICES; ++i) {
    if (voices[i].active && voices[i].ampStage != ENV_OFF && voices[i].ampStage != ENV_RELEASE) {
      ++displayedVoices;
    }
  }
  return displayedVoices;
}

void drawStatus() {
  u8g2.setFont(u8g2_font_5x8_tf);
  char programLabel[32];
  buildProgramHeaderName(programLabel, sizeof(programLabel));

  if (multiPerformanceContextActive()) {
    const uint8_t part = rtalContextPart();
    char right[24];
    snprintf(right, sizeof(right), "P%u CH%02u V%u",
             (unsigned)(part + 1),
             (unsigned)multiParts[part].midiChannel,
             (unsigned)rtalDisplayedVoiceCountForPart(part));
    int x = 127 - u8g2.getStrWidth(right);
    if (x < 72) x = 72;
    // Leave the right-hand Part/Channel/Voice field readable.  The Multi name
    // is truncated, but Mxxx remains visible at all times.
    drawTruncated(programLabel, 1, RTALUILayout::STATUS_BASELINE, 13);
    u8g2.setCursor(x, RTALUILayout::STATUS_BASELINE);
    u8g2.print(right);
  } else {
    // FIX28: while Compare ORIGINAL is active, reserve header space for
    // a persistent and unambiguous ORIG indicator.
    drawTruncated(programLabel, 1, RTALUILayout::STATUS_BASELINE,
                  compareMode ? 10 : 17);

    char right[24];
    if (compareMode) {
      snprintf(right, sizeof(right), "ORIG CH%02u V%u",
               midiChannel, rtalDisplayedVoiceCount());
    } else {
      snprintf(right, sizeof(right), "CH%02u V%u",
               midiChannel, rtalDisplayedVoiceCount());
    }

    int x = 127 - u8g2.getStrWidth(right);
    const int minX = compareMode ? 52 : 87;
    if (x < minX) x = minX;
    u8g2.setCursor(x, RTALUILayout::STATUS_BASELINE);
    u8g2.print(right);
  }
  u8g2.drawHLine(0, RTALUILayout::STATUS_DIVIDER_Y, 128);
}

void drawTitle(const char* section, const char* subtitle) {
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(1, RTALUILayout::TITLE_BASELINE);
  u8g2.print(section);
  u8g2.print(" / ");
  u8g2.print(subtitle);
}

const char* shortParamName(uint8_t p) {
  switch (p) {
    case P_WAVETABLE: return "Table";
    case P_WAVE_POS: return "Wave Position";
    case P_WAVE_MOD: return "Wave Mod";
    case P_OSC_B_OFFSET: return "OSC2 Offset";
    case P_OSC_DETUNE: return "OSC Detune";
    case P_OSC_MIX: return "OSC Mix";
    case P_NOISE_LEVEL: return "Noise Level";
    case P_SUB_LEVEL: return "Sub Level";
    case P_VOLUME: return "Volume";
    case P_PAN_SPREAD: return "Pan Spread";
    case P_CUTOFF: return "Cutoff";
    case P_RESONANCE: return "Resonance";
    case P_FILTER_ENV: return "Env Amount";
    case P_ATTACK: return "Attack";
    case P_DECAY: return "Decay";
    case P_SUSTAIN: return "Sustain";
    case P_RELEASE: return "Release";
    case P_F_ATTACK: return "Attack";
    case P_F_DECAY: return "Decay";
    case P_F_SUSTAIN: return "Sustain";
    case P_F_RELEASE: return "Release";
    case P_WAVE_ENV: return "Amount";
    case P_WAVE_ENV_ATTACK: return "Attack";
    case P_WAVE_ENV_DECAY: return "Decay";
    case P_WAVE_ENV_SUSTAIN: return "Sustain";
    case P_WAVE_ENV_RELEASE: return "Release";
    case P_LFO_RATE: return "LFO Rate";
    case P_LFO_AMOUNT: return "LFO Amount";
    case P_LFO_TARGET: return "Target";
    case P_LFO_SHAPE: return "Shape";
    case P_WAVE_LFO_RATE: return "Wave Rate";
    case P_WAVE_LFO_AMOUNT: return "Wave Amount";
    case P_AFTERTOUCH_WAVE: return "AT Wave";
    case P_AFTERTOUCH_FILTER: return "AT Filter";
    case P_VEL_AMP: return "Velocity Amp";
    case P_VEL_FILTER: return "Velocity Fil";
    case P_KEYTRACK: return "Keytrack";
    case P_PLAY_MODE: return "Play Mode";
    case P_GLIDE: return "Glide";
    case P_BEND_RANGE: return "Bend Range";
    case P_UNISON_DETUNE: return "Uni Detune";
    case P_ARP_MODE: return "Mode";
    case P_ARP_RATE: return "Rate";
    case P_ARP_OCTAVES: return "Octaves";
    case P_ARP_HOLD: return "Hold";
    case P_CLOCK_SOURCE: return "Clock";
    case P_TAP_TEMPO: return "Tempo";
    case P_SEQ_MODE: return "Mode";
    case P_SEQ_RATE: return "Rate";
    case P_SEQ_STEPS: return "Steps";
    case P_SEQ_TARGET: return "Target";
    case P_SEQ_DEPTH: return "Depth";
    case UI_PARAM_SEQ_TABLE_MODE: return "Table Mode";
    case UI_PARAM_SEQ_STEP: return "Step";
    case UI_PARAM_SEQ_VALUE: return "Value";
    case P_CHORUS: return "Chorus";
    case P_DRIVE: return "Drive";
    case P_BITCRUSH: return "Bitcrusher";
    case P_MORPH_AMOUNT: return "Morph Amount";
    case P_RANDOMIZE: return "Randomize";
    default: return uiParamName(p);
  }
}

uint8_t rtalParamNumericValue(uint8_t p) {
  if (multiPerformanceContextActive() && rtalUi.screen == RtalScreenId::PlayArpeggiator &&
      (p == P_ARP_MODE || p == P_ARP_RATE || p == P_ARP_OCTAVES || p == P_ARP_HOLD)) {
    return multiArpParam(rtalUi.selectedMultiPart, p);
  }
  if (multiPerformanceContextActive() &&
      (rtalUi.screen == RtalScreenId::PlaySequencer ||
       rtalUi.screen == RtalScreenId::PlaySeqEdit ||
       rtalUi.screen == RtalScreenId::PlaySeqShow)) {
    if (p == P_SEQ_MODE || p == P_SEQ_RATE || p == P_SEQ_STEPS ||
        p == P_SEQ_TARGET || p == P_SEQ_DEPTH)
      return multiSeqParam(rtalUi.selectedMultiPart, p);
    if (p == UI_PARAM_SEQ_TABLE_MODE) return multiSeqTableModeForPart(rtalUi.selectedMultiPart) ? 127 : 0;
    if (p == UI_PARAM_SEQ_STEP) return modSeqEditStep + 1;
    if (p == UI_PARAM_SEQ_VALUE) return multiSeqValueForPart(rtalUi.selectedMultiPart, modSeqEditStep);
  }
  if (p == P_LFO_SHAPE) return rtalContextParam(P_LFO_SHAPE);
  if (p == UI_PARAM_SEQ_TABLE_MODE || p == UI_PARAM_SEQ_STEP || p == UI_PARAM_SEQ_VALUE) return uiParamValue(p);
  return rtalContextParam(p);
}

void formatParam(uint8_t p, char* out, size_t outSize) {
  uint8_t value = rtalParamNumericValue(p);
  if (p == P_WAVETABLE) {
    snprintf(out, outSize, "%s", rtalContextWavetableName());
  } else if ((p == P_NOISE_LEVEL || p == P_SUB_LEVEL) && value == 0) {
    snprintf(out, outSize, "OFF");
  } else if (p == P_PAN_SPREAD && value == 0) {
    snprintf(out, outSize, "CENTER");
  } else if (p == P_LFO_TARGET) {
    snprintf(out, outSize, "%s", lfoTargetName(value));
  } else if (p == P_LFO_SHAPE) {
    snprintf(out, outSize, "%s", lfoShapeName(value));
  } else if (p == P_PLAY_MODE) {
    snprintf(out, outSize, "%s", playModeName(value));
  } else if (p == P_ARP_MODE) {
    snprintf(out, outSize, "%s", arpModeName(value));
  } else if (p == P_ARP_RATE) {
    snprintf(out, outSize, "%s", arpRateName(value));
  } else if (p == P_ARP_HOLD) {
    snprintf(out, outSize, "%s", switchIsOn(value) ? "ON" : "OFF");
  } else if (p == P_CLOCK_SOURCE) {
    snprintf(out, outSize, "%s", clockName(getClockSource()));
  } else if (p == P_TAP_TEMPO) {
    // v1.3.12 FIX1: P_TAP_TEMPO is stored internally as a 0..127 parameter,
    // but the UI must show the musical master tempo, never the raw byte.
    // INT: display the active global BPM. MIDI: display the measured external
    // clock BPM when valid/running; otherwise make the missing clock explicit.
    if (getClockSource() == 0) {
      snprintf(out, outSize, "%u BPM", (unsigned)bpm);
    } else if (midiClockRunning && midiClockValid) {
      snprintf(out, outSize, "%u BPM", (unsigned)getMidiBpmForDisplay());
    } else {
      snprintf(out, outSize, "--- BPM");
    }
  } else if (p == P_SEQ_MODE) {
    snprintf(out, outSize, "%s", seqModeName(value));
  } else if (p == P_SEQ_RATE) {
    snprintf(out, outSize, "%s", seqRateName(value));
  } else if (p == P_SEQ_TARGET) {
    snprintf(out, outSize, "%s", seqTargetName(value));
  } else if (p == UI_PARAM_SEQ_TABLE_MODE) {
    uint8_t tm = multiPerformanceContextActive() ? multiSeqTableModeForPart(rtalUi.selectedMultiPart) : modSeqTableMode;
    snprintf(out, outSize, "%s", seqTableModeName(tm));
  } else if (p == UI_PARAM_SEQ_STEP) {
    uint8_t steps = multiPerformanceContextActive() ? multiSeqParam(rtalUi.selectedMultiPart, P_SEQ_STEPS) : getAParam(P_SEQ_STEPS);
    if (steps < 1) steps = 1;
    if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;
    if (modSeqEditStep >= steps) modSeqEditStep = steps - 1;
    snprintf(out, outSize, "%u/%u", (unsigned)(modSeqEditStep + 1), (unsigned)steps);
  } else if (p == UI_PARAM_SEQ_VALUE) {
    const uint8_t stepValue = multiPerformanceContextActive() ? multiSeqValueForPart(rtalUi.selectedMultiPart, modSeqEditStep) : modSeqValues[modSeqEditStep];
    const bool targetTable = multiPerformanceContextActive()
      ? (multiSeqParam(rtalUi.selectedMultiPart, P_SEQ_TARGET) == P_WAVETABLE)
      : seqTargetIsTable();
    const uint8_t tableMode = multiPerformanceContextActive() ? multiSeqTableModeForPart(rtalUi.selectedMultiPart) : modSeqTableMode;
    if (targetTable) {
      if (tableMode == 0) snprintf(out, outSize, "WT%03u", (unsigned)stepValue);
      else snprintf(out, outSize, "+%u", (unsigned)stepValue);
    } else {
      snprintf(out, outSize, "%u", (unsigned)stepValue);
    }
  } else if (p == P_RANDOMIZE) {
    snprintf(out, outSize, "READY");
  } else {
    snprintf(out, outSize, "%u", value);
  }
}



bool rtalParamUsesBar(uint8_t p) {
  switch (p) {
    case P_WAVETABLE:
    case P_LFO_TARGET:
    case P_LFO_SHAPE:
    case P_PLAY_MODE:
    case P_ARP_MODE:
    case P_ARP_RATE:
    case P_ARP_HOLD:
    case P_ARP_OCTAVES:
    case P_CLOCK_SOURCE:
    case P_TAP_TEMPO:
    case P_SEQ_MODE:
    case P_SEQ_RATE:
    case P_SEQ_TARGET:
    case UI_PARAM_SEQ_TABLE_MODE:
    case UI_PARAM_SEQ_STEP:
    case P_RANDOMIZE:
      return false;
    default:
      return true;
  }
}

void drawBar(uint8_t value, uint8_t baselineY) {
  // All numeric widgets use the same fixed right-hand geometry.
  u8g2.drawFrame(96, baselineY - 6, 31, 6);
  uint8_t fill = ((uint16_t)value * 28U) / 127U;
  if (fill) u8g2.drawBox(98, baselineY - 4, fill, 2);
}

void drawParameter(uint8_t p, uint8_t baselineY, bool selected) {
  u8g2.setFont(u8g2_font_5x8_tf);

  // Phoenix/HIS experience: never use inverse text.  A separated arrow is
  // faster to read and remains legible from oblique viewing angles.
  if (selected) { u8g2.setCursor(0, baselineY); u8g2.print('>'); }
  u8g2.setCursor(7, baselineY); u8g2.print(shortParamName(p));

  char value[24];
  formatParam(p, value, sizeof(value));

  if (!rtalParamUsesBar(p)) {
    const int valueX = (p == P_WAVETABLE) ? 39 : 73;
    drawTruncated(value, valueX, baselineY, (p == P_WAVETABLE) ? 17 : 10);
    return;
  }

  // Numeric value is right-aligned immediately before the fixed bar widget.
  int valueX = 91 - u8g2.getStrWidth(value);
  if (valueX < 73) valueX = 73;
  u8g2.setCursor(valueX, baselineY);
  u8g2.print(value);
  drawBar(rtalParamNumericValue(p), baselineY);
}

void drawOsc2Parameter(uint8_t p, uint8_t baselineY, bool selected) {
  u8g2.setFont(u8g2_font_5x8_tf);
  if (selected) { u8g2.setCursor(0, baselineY); u8g2.print('>'); }

  const char* name = (p == P_OSC_B_OFFSET) ? "Offset" : "Detune";
  u8g2.setCursor(7, baselineY);
  u8g2.print(name);

  char value[12];
  formatParam(p, value, sizeof(value));
  int valueX = 91 - u8g2.getStrWidth(value);
  if (valueX < 73) valueX = 73;
  u8g2.setCursor(valueX, baselineY);
  u8g2.print(value);
  drawBar(rtalParamNumericValue(p), baselineY);
}

void drawSignalFlow(uint8_t activeStage) {
  // FIX28: WAVE MON is a real SOUND page and therefore appears as WAV in
  // the same structural footer as the other SOUND pages.
  static const char* flow = "OSC1-OSC2-MIX-NOI-SUB-OUT-WAV";
  // Character starts in the 4-pixel RTAL footer grid.
  static const uint8_t startChar[] = {0, 5, 10, 14, 18, 22, 26};
  static const uint8_t charCount[] = {4, 4, 3, 3, 3, 3, 3};

  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, flow, false);

  if (activeStage < 7) {
    const uint8_t x = 1 + startChar[activeStage] * RTALFooterFont::ADV;
    const uint8_t width = charCount[activeStage] * RTALFooterFont::ADV - 1;
    u8g2.drawHLine(x, RTALUILayout::FOOTER_UNDERLINE_Y, width);
  }
}

void drawEditSignalFlow(uint8_t activeStage) {
  static const char* flow = "OSC1-OSC2-MIX-FIL-AMP-FX-OUT";
  static const uint8_t startChar[] = {0, 5, 10, 14, 18, 22, 25};
  static const uint8_t charCount[] = {4, 4, 3, 3, 3, 2, 3};

  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, flow, false);

  if (activeStage < 7) {
    const uint8_t x = 1 + startChar[activeStage] * RTALFooterFont::ADV;
    const uint8_t width = charCount[activeStage] * RTALFooterFont::ADV - 1;
    u8g2.drawHLine(x, RTALUILayout::FOOTER_UNDERLINE_Y, width);
  }
}



void drawModFooter(uint8_t activeStage) {
  static const char* flow = "LFO-WLFO-AFT-VEL-KEY-MOD";
  static const uint8_t startChar[] = {0, 4, 9, 13, 17, 21};
  static const uint8_t charCount[] = {3, 4, 3, 3, 3, 3};
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, flow, false);
  if (activeStage < 6) {
    uint8_t x = 1 + startChar[activeStage] * RTALFooterFont::ADV;
    uint8_t width = charCount[activeStage] * RTALFooterFont::ADV - 1;
    u8g2.drawHLine(x, RTALUILayout::FOOTER_UNDERLINE_Y, width);
  }
}

void drawPlayFooter(uint8_t activeStage) {
  // FIX3: MULTI is a first-class PLAY stage.  Keep all seven stages visible
  // inside the 128 px footer.  "HM" is the only abbreviated label so that
  // MULTI, ARP, SEQ, EDIT and SHOW remain unambiguous.
  static const char* flow = "HM-PERF-MULTI-ARP-SEQ-EDIT-SHOW";
  static const uint8_t startChar[] = {0, 3, 8, 14, 18, 22, 27};
  static const uint8_t charCount[] = {2, 4, 5, 3, 3, 4, 4};
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, flow, false);
  if (activeStage < 7) {
    uint8_t x = 1 + startChar[activeStage] * RTALFooterFont::ADV;
    uint8_t width = charCount[activeStage] * RTALFooterFont::ADV - 1;
    u8g2.drawHLine(x, RTALUILayout::FOOTER_UNDERLINE_Y, width);
  }
}

void drawFxFooter() {
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, "CHORUS-DRIVE-BITCRUSH", false);
  u8g2.drawHLine(1, RTALUILayout::FOOTER_UNDERLINE_Y, 6 * RTALFooterFont::ADV - 1);
}



enum : uint8_t {
  RTAL_MORPH_ITEM_AMOUNT = 0,
  RTAL_MORPH_ITEM_SOURCE,
  RTAL_MORPH_ITEM_CAPTURE_A,
  RTAL_MORPH_ITEM_CAPTURE_B,
  RTAL_MORPH_ITEM_RANDOMIZE,
  RTAL_MORPH_ITEM_COUNT
};

const char* rtalMorphItemName(uint8_t item) {
  switch (item) {
    case RTAL_MORPH_ITEM_AMOUNT: return "Morph Amount";
    case RTAL_MORPH_ITEM_SOURCE: return "Morph Source";
    case RTAL_MORPH_ITEM_CAPTURE_A: return "Capture A";
    case RTAL_MORPH_ITEM_CAPTURE_B: return "Capture B";
    default: return "Randomize";
  }
}

void rtalMorphItemValue(uint8_t item, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  switch (item) {
    case RTAL_MORPH_ITEM_AMOUNT:
      snprintf(out, outSize, "%u", currentMorphControlAmount());
      break;
    case RTAL_MORPH_ITEM_SOURCE:
      snprintf(out, outSize, "%s", morphSourceName(morphSource));
      break;
    case RTAL_MORPH_ITEM_CAPTURE_A:
      snprintf(out, outSize, "%s", morphAValid ? "READY" : "EMPTY");
      break;
    case RTAL_MORPH_ITEM_CAPTURE_B:
      snprintf(out, outSize, "%s", morphBValid ? "READY" : "EMPTY");
      break;
    default:
      snprintf(out, outSize, "TURN");
      break;
  }
}

void drawMorphItem(uint8_t item, uint8_t baselineY, bool selected) {
  u8g2.setFont(u8g2_font_5x8_tf);
  if (selected) {
    u8g2.setCursor(0, baselineY);
    u8g2.print('>');
  }
  u8g2.setCursor(7, baselineY);
  u8g2.print(rtalMorphItemName(item));

  char value[16];
  rtalMorphItemValue(item, value, sizeof(value));
  if (item == RTAL_MORPH_ITEM_AMOUNT) {
    int valueX = 91 - u8g2.getStrWidth(value);
    if (valueX < 73) valueX = 73;
    u8g2.setCursor(valueX, baselineY);
    u8g2.print(value);
    drawBar(currentMorphControlAmount(), baselineY);
  } else {
    drawTruncated(value, 73, baselineY, 10);
  }
}

void drawMorphPage() {
  drawStatus();
  drawTitle("MOD", "MORPH");

  // Compact always-visible A/B snapshot status.  The RTAL footer font keeps
  // both names readable without crowding the two editable rows.
  char aName[10] = "--";
  char bName[10] = "--";
  if (morphAValid) {
    snprintf(aName, sizeof(aName), "%.8s", morphNameA[0] ? morphNameA : "A READY");
  }
  if (morphBValid) {
    snprintf(bName, sizeof(bName), "%.8s", morphNameB[0] ? morphNameB : "B READY");
  }
  char snapshots[28];
  snprintf(snapshots, sizeof(snapshots), "A:%s B:%s", aName, bName);
  RTALFooterFont::drawText(u8g2, 7, 20, snapshots, false);

  if (rtalUi.selected >= RTAL_MORPH_ITEM_COUNT) rtalUi.selected = 0;
  const uint8_t first = rtalUi.selected;
  const uint8_t second = (first + 1) % RTAL_MORPH_ITEM_COUNT;
  drawMorphItem(first, 35, true);
  drawMorphItem(second, 47, false);
  drawModFooter(5);
}

void triggerMorphUiAction(uint8_t item) {
  switch (item) {
    case RTAL_MORPH_ITEM_CAPTURE_A:
      captureMorphA();
      rtalUiShowMessage("CAPTURED A", 900);
      break;
    case RTAL_MORPH_ITEM_CAPTURE_B:
      captureMorphB();
      rtalUiShowMessage("CAPTURED B", 900);
      break;
    case RTAL_MORPH_ITEM_RANDOMIZE:
      allNotesOff();
      randomizeMusicalProgram();
      rtalUiShowMessage("RANDOMIZED", 900);
      break;
    default:
      break;
  }
}

void drawHome() {
  drawStatus();
  u8g2.setFont(u8g2_font_5x8_tf);

  char value[28];
  u8g2.setCursor(3, RTALUILayout::TITLE_BASELINE); u8g2.print("HOME / PERFORMANCE");

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW1_BASELINE); u8g2.print("WT");
  drawTruncated(rtalContextWavetableName(), RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW1_BASELINE, 16);

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW2_BASELINE); u8g2.print("Filter");
  snprintf(value, sizeof(value), "%u", rtalContextParam(P_CUTOFF));
  u8g2.setCursor(RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW2_BASELINE); u8g2.print(value);

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW3_BASELINE); u8g2.print("ARP");
  snprintf(value, sizeof(value), "%s %s", arpModeName(rtalContextParam(P_ARP_MODE)), arpRateName(rtalContextParam(P_ARP_RATE)));
  drawTruncated(value, RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW3_BASELINE, 16);

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW4_BASELINE); u8g2.print("SEQ");
  snprintf(value, sizeof(value), "%s %s", seqModeName(rtalContextParam(P_SEQ_MODE)), seqRateName(rtalContextParam(P_SEQ_RATE)));
  drawTruncated(value, RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW4_BASELINE, 16);

  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, "OSC1-OSC2-MIX-FIL-AMP-FX-OUT", false);
}

void drawMixer() {
  uint8_t mix = rtalContextParam(P_OSC_MIX);
  uint8_t leftPct = ((uint16_t)(127 - mix) * 100U) / 127U;
  uint8_t rightPct = 100 - leftPct;
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(1, 28); u8g2.print(">");
  u8g2.setCursor(8, 28); u8g2.print("OSC Mix");
  char line[24]; snprintf(line, sizeof(line), "OSC1 %u%% | %u%% OSC2", leftPct, rightPct);
  u8g2.setCursor(8, 39); u8g2.print(line);
  u8g2.drawVLine(64, 42, 5);
  uint8_t split = ((uint16_t)mix * 120U) / 127U;
  u8g2.drawFrame(4, 43, 120, 5);
  if (split < 118) u8g2.drawBox(5, 44, 118 - split, 3);
  drawSignalFlow(2);
}

void drawRtalWaveMonitor() {
  drawStatus();
  drawTitle("SOUND", "WAVE MON");

  uint8_t liveTable = selectedWavetableIndexAudio();
  uint8_t logicalWave = currentLogicalWaveIndex();
  bool liveVoice = false;

  // FIX28: follow the most recently triggered voice while it is still active.
  // Its monitor fields are written by the audio task after all wave-position
  // modulation has been applied. A local copy prevents mixed cross-core reads.
  int8_t monitorIndex = waveMonitorVoice;
  if (!rtalUi.waveMonitorTableSelect && monitorIndex >= 0 && monitorIndex < NUM_VOICES) {
    Voice &mv = voices[(uint8_t)monitorIndex];
    if (mv.active && mv.ampStage != ENV_OFF) {
      logicalWave = mv.monitorWave;
      liveTable = mv.monitorTable;
      if (logicalWave >= RTAL_WT_WAVES) logicalWave = RTAL_WT_WAVES - 1;
      if (liveTable >= WT_VISIBLE_SLOTS) liveTable = WT_LAST_SLOT;
      liveVoice = true;
    }
  }

  u8g2.setFont(u8g2_font_5x8_tf);
  const char* tableName;
  int16_t sd = sdIndexForVisibleSlot(liveTable);
  if (sd >= 0) tableName = sdWavetableNames[sd];
  else if (liveTable < RTAL_WT_TABLES) tableName = RTAL_WT_NAMES[liveTable];
  else tableName = "WT?";
  drawTruncated(tableName, 1, 26, 15);

  char right[18];
  const char* mode = rtalUi.waveMonitorTableSelect ? "TABLE" : (liveVoice ? "LIVE" : "SCAN");
  snprintf(right, sizeof(right), "%s W%02u", mode, logicalWave);
  int x = 127 - u8g2.getStrWidth(right);
  if (x < 74) x = 74;
  u8g2.setCursor(x, 26);
  u8g2.print(right);

  // The waveform is the same logical wave used by oscillator A of the
  // monitored voice. The former isolated P-number was intentionally removed;
  // it duplicated Wxx for normal tables and was unclear on the instrument.
  u8g2.drawHLine(0, 37, 128);
  drawWaveformLine(liveTable, logicalWave, 38, 20);

  drawSignalFlow(6); // WAV
}

void drawRtalSeqShow() {
  drawStatus();
  if (multiPerformanceContextActive()) {
    char title[16]; snprintf(title, sizeof(title), "SEQ SHOW P%u", rtalUi.selectedMultiPart + 1);
    drawTitle("PLAY", title);
  } else drawTitle("PLAY", "SEQ SHOW");

  // FIX3: the v1.3 common multi-engine owns sequencer runtime even for
  // normal F/U performance. Part 1 is therefore the runtime owner in single
  // performance, while a loaded Multi uses the selected part.
  const uint8_t runtimePart = multiPerformanceContextActive() ? rtalUi.selectedMultiPart : 0;
  uint8_t steps = multiSeqParam(runtimePart, P_SEQ_STEPS);
  if (steps < 1) steps = 1;
  if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;

  const uint8_t modeValue = multiSeqParam(runtimePart, P_SEQ_MODE);
  const uint8_t targetValue = multiSeqParam(runtimePart, P_SEQ_TARGET);
  char info[28];
  snprintf(info, sizeof(info), "%s  %u STEP  %s",
           seqModeName(modeValue), (unsigned)steps, seqTargetName(targetValue));
  u8g2.setFont(u8g2_font_5x8_tf);
  drawTruncated(info, 1, 28, 25);

  // Sixteen fixed columns make changes in sequence length immediately visible.
  // Active steps are bars, inactive steps are short baseline ticks. The most
  // recently executed step gets a one-pixel frame and therefore remains clear
  // without inverse drawing on the OLED.
  const uint8_t chartTop = 31;
  const uint8_t chartBottom = 49;
  const uint8_t maxHeight = chartBottom - chartTop;
  uint8_t current = multiSeqDisplayStepForPart(runtimePart);
  if (current >= steps) current = 0;

  for (uint8_t i = 0; i < MODSEQ_STEPS; ++i) {
    const uint8_t x = 3 + i * 7;
    if (i < steps) {
      const uint8_t seqVal = multiSeqValueForPart(runtimePart, i);
      uint8_t height = 1 + ((uint16_t)seqVal * (maxHeight - 1)) / 127U;
      const uint8_t y = chartBottom - height;
      u8g2.drawBox(x + 1, y, 4, height);
      if (modeValue != 0 && i == current) {
        uint8_t frameY = y > chartTop ? y - 1 : chartTop;
        uint8_t frameH = chartBottom - frameY + 1;
        u8g2.drawFrame(x, frameY, 6, frameH);
      }
    } else {
      u8g2.drawHLine(x + 1, chartBottom, 4);
    }
  }

  drawPlayFooter(6);
}

// Forward declaration: the remote FX page uses the common System row renderer
// which is defined later in this translation unit.
void drawSystemRow(const char* name, const char* value, uint8_t baselineY, bool selected);

// ----------------------------------------------------------------
// Build0038: complete remote control of the current FX-ESP stereo delay.
// Four OLED pages expose all delay parameters implemented by FX Build0036.
// The FX ESP remains authoritative; SET_PARAM is mirrored locally and echoed
// back with STATE. No DSP work is added to the WELLENBAD audio task.
// ----------------------------------------------------------------
static bool fxRemoteInt(uint8_t param, int32_t &value) {
  return RTALFxLink::getRemoteValue(param, value);
}

static float fxRemoteFloat(uint8_t param, float fallback) {
  int32_t raw = 0;
  if (!fxRemoteInt(param, raw)) return fallback;
  return RTALFxLink::q16ToFloat(raw);
}

static bool fxRemoteBool(uint8_t param, bool fallback) {
  int32_t raw = 0;
  if (!fxRemoteInt(param, raw)) return fallback;
  return raw != 0;
}

static int32_t fxRemoteIntValue(uint8_t param, int32_t fallback) {
  int32_t raw = 0;
  return fxRemoteInt(param, raw) ? raw : fallback;
}

static bool isFxRemoteScreen(RtalScreenId id) {
  return id == RtalScreenId::FxRemoteDelay ||
         id == RtalScreenId::FxRemoteModSelect ||
         id == RtalScreenId::FxRemoteTiming ||
         id == RtalScreenId::FxRemoteTone ||
         id == RtalScreenId::FxRemoteDuck ||
         id == RtalScreenId::FxRemoteModCore ||
         id == RtalScreenId::FxRemoteModStereo ||
         id == RtalScreenId::FxRemoteChorus ||
         id == RtalScreenId::FxRemoteFlanger ||
         id == RtalScreenId::FxRemoteFlangerTone ||
         id == RtalScreenId::FxRemotePhaser ||
         id == RtalScreenId::FxRemotePhaserTone ||
         id == RtalScreenId::FxRemoteWidth ||
         id == RtalScreenId::FxRemoteReverb ||
         id == RtalScreenId::FxRemoteReverbTone;
}

static RtalScreenId nextFxRemoteScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::FxRemoteDelay:  return RtalScreenId::FxRemoteTiming;
    case RtalScreenId::FxRemoteTiming: return RtalScreenId::FxRemoteTone;
    case RtalScreenId::FxRemoteTone:      return RtalScreenId::FxRemoteDuck;
    case RtalScreenId::FxRemoteDuck:      return RtalScreenId::FxRemoteModSelect;
    case RtalScreenId::FxRemoteModSelect: return RtalScreenId::FxRemoteModCore;
    case RtalScreenId::FxRemoteModCore:   return RtalScreenId::FxRemoteModStereo;
    case RtalScreenId::FxRemoteModStereo: return RtalScreenId::FxRemoteChorus;
    case RtalScreenId::FxRemoteChorus:      return RtalScreenId::FxRemoteFlanger;
    case RtalScreenId::FxRemoteFlanger:     return RtalScreenId::FxRemoteFlangerTone;
    case RtalScreenId::FxRemoteFlangerTone: return RtalScreenId::FxRemotePhaser;
    case RtalScreenId::FxRemotePhaser:      return RtalScreenId::FxRemotePhaserTone;
    case RtalScreenId::FxRemotePhaserTone:  return RtalScreenId::FxRemoteReverb;
    case RtalScreenId::FxRemoteReverb:      return RtalScreenId::FxRemoteReverbTone;
    case RtalScreenId::FxRemoteReverbTone:  return RtalScreenId::FxRemoteWidth;
    case RtalScreenId::FxRemoteWidth:       return RtalScreenId::FxMain;
    default:                               return RtalScreenId::FxRemoteDelay;
  }
}

static const char* fxDivisionName(int32_t div) {
  static const char* const names[12] = {
    "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
    "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T"
  };
  return (div >= 0 && div < 12) ? names[div] : "---";
}

void drawFxRemoteFooter(const char* page) {
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  char footer[24];
  snprintf(footer, sizeof(footer), "LOCAL FX-%s", page);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, footer, false);
}

void drawFxRemoteDelay() {
  drawStatus();
  drawTitle("FX DELAY", RTALFxLink::connected() ? "MAIN" : "OFFLINE");
  char value[18];
  const bool enabled = fxRemoteBool(RTALFxProtocol::P_DELAY_ENABLE, true);
  const bool freeze = fxRemoteBool(RTALFxProtocol::P_DELAY_FREEZE, false);
  const float level = fxRemoteFloat(RTALFxProtocol::P_DELAY_LEVEL, 0.25f);
  const float feedback = fxRemoteFloat(RTALFxProtocol::P_DELAY_FEEDBACK, 0.35f);
  snprintf(value, sizeof(value), "%s", enabled ? "ON" : "OFF");
  drawSystemRow("Enable", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%s", freeze ? "ON" : "OFF");
  drawSystemRow("Freeze", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(level * 100.0f + 0.5f), 0, 100));
  drawSystemRow("Level", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(feedback * 100.0f + 0.5f), 0, 92));
  drawSystemRow("Feedback", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("MAIN");
}

void drawFxRemoteTiming() {
  drawStatus();
  drawTitle("FX DELAY", RTALFxLink::connected() ? "TIMING" : "OFFLINE");
  char value[18];
  const int32_t mode = fxRemoteIntValue(RTALFxProtocol::P_DELAY_MODE, 0);
  snprintf(value, sizeof(value), "%s", mode ? "SYNC" : "FREE");
  drawSystemRow("Mode", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  if (mode) {
    snprintf(value, sizeof(value), "%s", fxDivisionName(fxRemoteIntValue(RTALFxProtocol::P_DELAY_LEFT_DIV, 7)));
    drawSystemRow("Left Div", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
    snprintf(value, sizeof(value), "%s", fxDivisionName(fxRemoteIntValue(RTALFxProtocol::P_DELAY_RIGHT_DIV, 2)));
    drawSystemRow("Right Div", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  } else {
    snprintf(value, sizeof(value), "%u ms", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_LEFT_MS, 250.0f)+0.5f),1,1000));
    drawSystemRow("Left Time", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
    snprintf(value, sizeof(value), "%u ms", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_RIGHT_MS, 375.0f)+0.5f),1,1000));
    drawSystemRow("Right Time", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  }
  const int32_t bpm100 = fxRemoteIntValue(RTALFxProtocol::P_CLOCK_BPM_X100, 12000);
  snprintf(value, sizeof(value), "%ld.%02ld BPM", (long)(bpm100/100), (long)(bpm100%100));
  drawSystemRow("Clock", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("TIME");
}

void drawFxRemoteTone() {
  drawStatus();
  drawTitle("FX DELAY", RTALFxLink::connected() ? "TONE" : "OFFLINE");
  char value[18];
  snprintf(value, sizeof(value), "%u Hz", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_LOWPASS_HZ,6000.0f)+0.5f),250,18000));
  drawSystemRow("Lowpass", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%u Hz", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_HIGHPASS_HZ,20.0f)+0.5f),20,4000));
  drawSystemRow("Highpass", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_SATURATION,0.0f)*100.0f+0.5f),0,100));
  drawSystemRow("Saturation", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_CROSSFEED,0.0f)*100.0f+0.5f),0,100));
  drawSystemRow("Crossfeed", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("TONE");
}

void drawFxRemoteDuck() {
  drawStatus();
  drawTitle("FX DELAY", RTALFxLink::connected() ? "DUCK" : "OFFLINE");
  char value[18];
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_DUCK_AMOUNT,0.0f)*100.0f+0.5f),0,100));
  drawSystemRow("Amount", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%.1f dB", (double)fxRemoteFloat(RTALFxProtocol::P_DELAY_DUCK_THRESHOLD_DB,-24.0f));
  drawSystemRow("Threshold", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "%u ms", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_DELAY_DUCK_RELEASE_MS,350.0f)+0.5f),50,2000));
  drawSystemRow("Release", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  const RTALFxLinkStats st = RTALFxLink::stats();
  snprintf(value, sizeof(value), "%s C%lu", st.connected ? "OK" : "OFF", (unsigned long)st.crcErrors);
  drawSystemRow("FX Link", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("DUCK");
}

// ----------------------------------------------------------------
// Build0044: central MOD-FX selector. Only one modulation effect runs at
// a time on the FX ESP.
// ----------------------------------------------------------------
static const char* fxModEffectName(int32_t effect) {
  switch (effect) { case 1:return "CHORUS"; case 2:return "FLANGER"; case 3:return "PHASER"; default:return "OFF"; }
}
static uint8_t fxSelectedMixParam(int32_t effect) {
  switch (effect) { case 1:return RTALFxProtocol::P_CHORUS_MIX; case 2:return RTALFxProtocol::P_FLANGER_MIX; case 3:return RTALFxProtocol::P_PHASER_MIX; default:return 0; }
}
void drawFxRemoteModSelect() {
  drawStatus(); drawTitle("FX MOD", RTALFxLink::connected()?"SELECT":"OFFLINE"); char value[18];
  const int32_t effect=constrain((int)fxRemoteIntValue(RTALFxProtocol::P_MOD_EFFECT_SELECT,1),0,3);
  snprintf(value,sizeof(value),"%s",fxModEffectName(effect)); drawSystemRow("Effect",value,RTALUILayout::ROW1_BASELINE,rtalUi.selected==0);
  if(effect==0) snprintf(value,sizeof(value),"---"); else {const uint8_t mp=fxSelectedMixParam(effect); snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(mp,0.35f)*100.0f+0.5f),0,100));}
  drawSystemRow("Mix",value,RTALUILayout::ROW2_BASELINE,rtalUi.selected==1);
  if(fxRemoteBool(RTALFxProtocol::P_MOD_SYNC,false)) snprintf(value,sizeof(value),"%s",fxDivisionName(fxRemoteIntValue(RTALFxProtocol::P_MOD_DIVISION,3))); else snprintf(value,sizeof(value),"%.2f Hz",(double)fxRemoteFloat(RTALFxProtocol::P_MOD_RATE_HZ,0.25f));
  drawSystemRow("Rate",value,RTALUILayout::ROW3_BASELINE,rtalUi.selected==2);
  snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_MOD_DEPTH,0.50f)*100.0f+0.5f),0,100)); drawSystemRow("Depth",value,RTALUILayout::ROW4_BASELINE,rtalUi.selected==3);
  drawFxRemoteFooter("SELECT");
}

// ----------------------------------------------------------------
// Build0039: shared RTAL Modulation Core test pages.
// The FX ESP owns the LFO. WELLENBAD only edits parameters and displays
// returned STATE values. No modulation DSP runs on the synth ESP.
// ----------------------------------------------------------------
static const char* fxModShapeName(int32_t shape) {
  return shape == 1 ? "TRI" : "SIN";
}

void drawFxRemoteModCore() {
  drawStatus();
  drawTitle("FX MOD", RTALFxLink::connected() ? "CORE" : "OFFLINE");
  char value[18];
  const bool sync = fxRemoteBool(RTALFxProtocol::P_MOD_SYNC, false);
  const int32_t shape = fxRemoteIntValue(RTALFxProtocol::P_MOD_LFO_SHAPE, 0);
  snprintf(value, sizeof(value), "%s", fxModShapeName(shape));
  drawSystemRow("LFO Shape", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%s", sync ? "SYNC" : "FREE");
  drawSystemRow("Mode", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  if (sync) {
    snprintf(value, sizeof(value), "%s", fxDivisionName(fxRemoteIntValue(RTALFxProtocol::P_MOD_DIVISION, 3)));
  } else {
    snprintf(value, sizeof(value), "%.2f Hz", (double)fxRemoteFloat(RTALFxProtocol::P_MOD_RATE_HZ, 0.25f));
  }
  drawSystemRow(sync ? "Division" : "Rate", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_MOD_DEPTH, 0.50f)*100.0f+0.5f),0,100));
  drawSystemRow("Depth", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("MOD");
}

void drawFxRemoteModStereo() {
  drawStatus();
  drawTitle("FX MOD", RTALFxLink::connected() ? "STEREO" : "OFFLINE");
  char value[18];
  snprintf(value, sizeof(value), "%u deg", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_MOD_STEREO_PHASE_DEG,90.0f)+0.5f),0,180));
  drawSystemRow("Stereo Phase", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%u ms", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_MOD_SMOOTHING_MS,50.0f)+0.5f),5,500));
  drawSystemRow("Smoothing", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "%+.2f", (double)fxRemoteFloat(RTALFxProtocol::P_MOD_LFO_LEFT,0.0f));
  drawSystemRow("LFO Left", value, RTALUILayout::ROW3_BASELINE, false);
  snprintf(value, sizeof(value), "%+.2f", (double)fxRemoteFloat(RTALFxProtocol::P_MOD_LFO_RIGHT,0.0f));
  drawSystemRow("LFO Right", value, RTALUILayout::ROW4_BASELINE, false);
  drawFxRemoteFooter("STEREO");
}

// Build0041: high-quality stereo chorus control. Rate, LFO shape, modulation
// depth, sync and stereo phase remain on the shared RTAL Mod Core pages.
void drawFxRemoteChorus() {
  drawStatus();
  drawTitle("FX CHORUS", RTALFxLink::connected() ? "STEREO" : "OFFLINE");
  char value[18];
  const bool enabled = fxRemoteBool(RTALFxProtocol::P_CHORUS_ENABLE, true);
  snprintf(value, sizeof(value), "%s", enabled ? "ON" : "OFF");
  drawSystemRow("Enable", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_CHORUS_MIX,0.35f)*100.0f+0.5f),0,100));
  drawSystemRow("Mix", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "%.1f ms", (double)fxRemoteFloat(RTALFxProtocol::P_CHORUS_DELAY_MS,16.0f));
  drawSystemRow("Delay", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  snprintf(value, sizeof(value), "%u Hz", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_CHORUS_TONE_HZ,12000.0f)+0.5f),2000,18000));
  drawSystemRow("Tone", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("CHORUS");
}

// Build0042: high-quality stereo flanger. Uses the same RTAL ModCore as
// Chorus; enabling Flanger on the FX ESP automatically disables Chorus.
void drawFxRemoteFlanger() {
  drawStatus();
  drawTitle("FX FLANGER", RTALFxLink::connected() ? "STEREO" : "OFFLINE");
  char value[18];
  const bool enabled = fxRemoteBool(RTALFxProtocol::P_FLANGER_ENABLE, false);
  snprintf(value, sizeof(value), "%s", enabled ? "ON" : "OFF");
  drawSystemRow("Enable", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_FLANGER_MIX,0.35f)*100.0f+0.5f),0,100));
  drawSystemRow("Mix", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "%.2f ms", (double)fxRemoteFloat(RTALFxProtocol::P_FLANGER_DELAY_MS,2.5f));
  drawSystemRow("Delay", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  snprintf(value, sizeof(value), "%+d %%", (int)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_FLANGER_FEEDBACK,0.35f)*100.0f),-95,95));
  drawSystemRow("Feedback", value, RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawFxRemoteFooter("FLANGER");
}

void drawFxRemoteFlangerTone() {
  drawStatus();
  drawTitle("FX FLANGER", RTALFxLink::connected() ? "FEEDBACK" : "OFFLINE");
  char value[18];
  snprintf(value, sizeof(value), "%u Hz", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_FLANGER_HPF_HZ,80.0f)+0.5f),20,2000));
  drawSystemRow("FB Highpass", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  snprintf(value, sizeof(value), "%u %%", (unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_FLANGER_SATURATION,0.12f)*100.0f+0.5f),0,100));
  drawSystemRow("Saturation", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  drawSystemRow("Interpolation", "HERMITE", RTALUILayout::ROW3_BASELINE, false);
  drawSystemRow("Mod Core", "SHARED", RTALUILayout::ROW4_BASELINE, false);
  drawFxRemoteFooter("FB");
}


// Build0043: stereo allpass phaser, 2/4/6/8 stages. Shared ModCore provides
// rate, sync, depth, LFO shape and stereo phase.
void drawFxRemotePhaser() {
  drawStatus();
  drawTitle("FX PHASER", RTALFxLink::connected() ? "STEREO" : "OFFLINE");
  char value[18];
  const bool enabled = fxRemoteBool(RTALFxProtocol::P_PHASER_ENABLE, false);
  snprintf(value,sizeof(value),"%s",enabled?"ON":"OFF"); drawSystemRow("Enable",value,RTALUILayout::ROW1_BASELINE,rtalUi.selected==0);
  snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_PHASER_MIX,0.35f)*100.0f+0.5f),0,100)); drawSystemRow("Mix",value,RTALUILayout::ROW2_BASELINE,rtalUi.selected==1);
  snprintf(value,sizeof(value),"%u",(unsigned)fxRemoteIntValue(RTALFxProtocol::P_PHASER_STAGES,4)); drawSystemRow("Stages",value,RTALUILayout::ROW3_BASELINE,rtalUi.selected==2);
  snprintf(value,sizeof(value),"%+d %%",(int)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_PHASER_FEEDBACK,0.30f)*100.0f),-95,95)); drawSystemRow("Feedback",value,RTALUILayout::ROW4_BASELINE,rtalUi.selected==3);
  drawFxRemoteFooter("PHASER");
}
void drawFxRemotePhaserTone() {
  drawStatus();
  drawTitle("FX PHASER", RTALFxLink::connected() ? "COLOR" : "OFFLINE");
  char value[18];
  snprintf(value,sizeof(value),"%u Hz",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_PHASER_CENTER_HZ,900.0f)+0.5f),200,4000)); drawSystemRow("Center",value,RTALUILayout::ROW1_BASELINE,rtalUi.selected==0);
  snprintf(value,sizeof(value),"%u Hz",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_PHASER_HPF_HZ,80.0f)+0.5f),20,2000)); drawSystemRow("FB Highpass",value,RTALUILayout::ROW2_BASELINE,rtalUi.selected==1);
  snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_PHASER_SATURATION,0.08f)*100.0f+0.5f),0,100)); drawSystemRow("Saturation",value,RTALUILayout::ROW3_BASELINE,rtalUi.selected==2);
  drawSystemRow("Mod Core","SHARED",RTALUILayout::ROW4_BASELINE,false);
  drawFxRemoteFooter("COLOR");
}

// Build0046: CPU-conscious 4-line Householder FDN stereo reverb.
void drawFxRemoteReverb() {
  drawStatus();
  drawTitle("FX REVERB", RTALFxLink::connected() ? "FDN 4" : "OFFLINE");
  char value[18];
  const bool enabled=fxRemoteBool(RTALFxProtocol::P_REVERB_ENABLE,false);
  snprintf(value,sizeof(value),"%s",enabled?"ON":"OFF"); drawSystemRow("Enable",value,RTALUILayout::ROW1_BASELINE,rtalUi.selected==0);
  snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_REVERB_MIX,0.28f)*100.0f+0.5f),0,100)); drawSystemRow("Mix",value,RTALUILayout::ROW2_BASELINE,rtalUi.selected==1);
  snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_REVERB_SIZE,0.62f)*100.0f+0.5f),0,100)); drawSystemRow("Size",value,RTALUILayout::ROW3_BASELINE,rtalUi.selected==2);
  snprintf(value,sizeof(value),"%u %%",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_REVERB_DECAY,0.58f)*100.0f+0.5f),0,100)); drawSystemRow("Decay",value,RTALUILayout::ROW4_BASELINE,rtalUi.selected==3);
  drawFxRemoteFooter("REVERB");
}
void drawFxRemoteReverbTone() {
  drawStatus();
  drawTitle("FX REVERB", RTALFxLink::connected() ? "TONE" : "OFFLINE");
  char value[18];
  snprintf(value,sizeof(value),"%u Hz",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_REVERB_DAMPING_HZ,7200.0f)+0.5f),1200,16000)); drawSystemRow("Damping",value,RTALUILayout::ROW1_BASELINE,rtalUi.selected==0);
  snprintf(value,sizeof(value),"%u ms",(unsigned)constrain((int)(fxRemoteFloat(RTALFxProtocol::P_REVERB_PREDELAY_MS,18.0f)+0.5f),0,200)); drawSystemRow("Predelay",value,RTALUILayout::ROW2_BASELINE,rtalUi.selected==1);
  drawSystemRow("Matrix","HOUSEHOLDER",RTALUILayout::ROW3_BASELINE,false);
  drawSystemRow("Position","PRE WIDTH",RTALUILayout::ROW4_BASELINE,false);
  drawFxRemoteFooter("TONE");
}

// Build0045: final Mid/Side stereo width stage. 100% is transparent,
// 0% is mono, values above 100% increase the Side component.
void drawFxRemoteWidth() {
  drawStatus();
  drawTitle("FX STEREO", RTALFxLink::connected() ? "WIDTH" : "OFFLINE");
  char value[18];
  const bool enabled = fxRemoteBool(RTALFxProtocol::P_WIDTH_ENABLE, true);
  snprintf(value, sizeof(value), "%s", enabled ? "ON" : "OFF");
  drawSystemRow("Enable", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  const int widthPct = constrain((int)(fxRemoteFloat(RTALFxProtocol::P_WIDTH_AMOUNT,1.0f)*100.0f+0.5f),0,200);
  snprintf(value, sizeof(value), "%d %%", widthPct);
  drawSystemRow("Width", value, RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  drawSystemRow("Mode", "MID/SIDE", RTALUILayout::ROW3_BASELINE, false);
  drawSystemRow("Position", "FINAL", RTALUILayout::ROW4_BASELINE, false);
  drawFxRemoteFooter("WIDTH");
}

static void fxRemoteEnsureConnected() {
  if (!RTALFxLink::connected()) {
    RTALFxLink::requestState();
    rtalUiShowMessage("FX LINK OFFLINE", 900);
  }
}

void stepFxRemote(int8_t delta) {
  if (!RTALFxLink::connected()) { fxRemoteEnsureConnected(); return; }
  const int dir = delta > 0 ? 1 : -1;
  switch (rtalUi.screen) {
    case RtalScreenId::FxRemoteDelay:
      switch (rtalUi.selected & 3) {
        case 0: RTALFxLink::setBool(RTALFxProtocol::P_DELAY_ENABLE, !fxRemoteBool(RTALFxProtocol::P_DELAY_ENABLE,true)); break;
        case 1: RTALFxLink::setBool(RTALFxProtocol::P_DELAY_FREEZE, !fxRemoteBool(RTALFxProtocol::P_DELAY_FREEZE,false)); break;
        case 2: { float v=fxRemoteFloat(RTALFxProtocol::P_DELAY_LEVEL,0.25f)+0.02f*dir; RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_LEVEL,constrain(v,0.0f,1.0f)); break; }
        case 3: { float v=fxRemoteFloat(RTALFxProtocol::P_DELAY_FEEDBACK,0.35f)+0.02f*dir; RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_FEEDBACK,constrain(v,0.0f,0.92f)); break; }
      } break;
    case RtalScreenId::FxRemoteTiming: {
      const int32_t mode=fxRemoteIntValue(RTALFxProtocol::P_DELAY_MODE,0);
      if (rtalUi.selected==0) RTALFxLink::setInt(RTALFxProtocol::P_DELAY_MODE,mode?0:1);
      else if (rtalUi.selected==1) {
        if(mode){int32_t v=constrain((int)fxRemoteIntValue(RTALFxProtocol::P_DELAY_LEFT_DIV,7)+dir,0,11);RTALFxLink::setInt(RTALFxProtocol::P_DELAY_LEFT_DIV,v);}
        else {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_LEFT_MS,250.0f)+10.0f*dir,1.0f,1000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_LEFT_MS,v);}
      } else if (rtalUi.selected==2) {
        if(mode){int32_t v=constrain((int)fxRemoteIntValue(RTALFxProtocol::P_DELAY_RIGHT_DIV,2)+dir,0,11);RTALFxLink::setInt(RTALFxProtocol::P_DELAY_RIGHT_DIV,v);}
        else {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_RIGHT_MS,375.0f)+10.0f*dir,1.0f,1000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_RIGHT_MS,v);}
      } break; }
    case RtalScreenId::FxRemoteTone:
      switch (rtalUi.selected & 3) {
        case 0: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_LOWPASS_HZ,6000.0f)+250.0f*dir,250.0f,18000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_LOWPASS_HZ,v);break;}
        case 1: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_HIGHPASS_HZ,20.0f)+50.0f*dir,20.0f,4000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_HIGHPASS_HZ,v);break;}
        case 2: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_SATURATION,0.0f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_SATURATION,v);break;}
        case 3: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_CROSSFEED,0.0f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_CROSSFEED,v);break;}
      } break;
    case RtalScreenId::FxRemoteDuck:
      switch (rtalUi.selected & 3) {
        case 0: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_DUCK_AMOUNT,0.0f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_DUCK_AMOUNT,v);break;}
        case 1: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_DUCK_THRESHOLD_DB,-24.0f)+1.0f*dir,-60.0f,0.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_DUCK_THRESHOLD_DB,v);break;}
        case 2: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_DELAY_DUCK_RELEASE_MS,350.0f)+25.0f*dir,50.0f,2000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_DELAY_DUCK_RELEASE_MS,v);break;}
        case 3: break; // diagnostics row, read-only
      } break;
    case RtalScreenId::FxRemoteModSelect: {
      const int32_t effect=constrain((int)fxRemoteIntValue(RTALFxProtocol::P_MOD_EFFECT_SELECT,1),0,3);
      if(rtalUi.selected==0){int32_t v=effect+dir;if(v<0)v=3;if(v>3)v=0;RTALFxLink::selectModEffect((uint8_t)v);}
      else if(rtalUi.selected==1){const uint8_t mp=fxSelectedMixParam(effect);if(mp){float v=constrain(fxRemoteFloat(mp,0.35f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(mp,v);}}
      else if(rtalUi.selected==2){if(fxRemoteBool(RTALFxProtocol::P_MOD_SYNC,false)){int32_t v=constrain((int)fxRemoteIntValue(RTALFxProtocol::P_MOD_DIVISION,3)+dir,0,11);RTALFxLink::setInt(RTALFxProtocol::P_MOD_DIVISION,v);}else{float v=fxRemoteFloat(RTALFxProtocol::P_MOD_RATE_HZ,0.25f);const float step=(v<1.0f)?0.05f:0.25f;v=constrain(v+step*dir,0.02f,10.0f);RTALFxLink::setFloat(RTALFxProtocol::P_MOD_RATE_HZ,v);}}
      else if(rtalUi.selected==3){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_MOD_DEPTH,0.50f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_MOD_DEPTH,v);}
      break;}
    case RtalScreenId::FxRemoteModCore: {
      if (rtalUi.selected==0) {
        int32_t v=fxRemoteIntValue(RTALFxProtocol::P_MOD_LFO_SHAPE,0); RTALFxLink::setInt(RTALFxProtocol::P_MOD_LFO_SHAPE,v?0:1);
      } else if (rtalUi.selected==1) {
        bool v=fxRemoteBool(RTALFxProtocol::P_MOD_SYNC,false); RTALFxLink::setBool(RTALFxProtocol::P_MOD_SYNC,!v);
      } else if (rtalUi.selected==2) {
        if (fxRemoteBool(RTALFxProtocol::P_MOD_SYNC,false)) {
          int32_t v=constrain((int)fxRemoteIntValue(RTALFxProtocol::P_MOD_DIVISION,3)+dir,0,11); RTALFxLink::setInt(RTALFxProtocol::P_MOD_DIVISION,v);
        } else {
          float v=fxRemoteFloat(RTALFxProtocol::P_MOD_RATE_HZ,0.25f);
          const float step=(v<1.0f)?0.05f:0.25f; v=constrain(v+step*dir,0.02f,10.0f); RTALFxLink::setFloat(RTALFxProtocol::P_MOD_RATE_HZ,v);
        }
      } else if (rtalUi.selected==3) {
        float v=constrain(fxRemoteFloat(RTALFxProtocol::P_MOD_DEPTH,0.50f)+0.02f*dir,0.0f,1.0f); RTALFxLink::setFloat(RTALFxProtocol::P_MOD_DEPTH,v);
      }
      break; }
    case RtalScreenId::FxRemoteModStereo:
      if (rtalUi.selected==0) {
        float v=constrain(fxRemoteFloat(RTALFxProtocol::P_MOD_STEREO_PHASE_DEG,90.0f)+5.0f*dir,0.0f,180.0f); RTALFxLink::setFloat(RTALFxProtocol::P_MOD_STEREO_PHASE_DEG,v);
      } else if (rtalUi.selected==1) {
        float v=constrain(fxRemoteFloat(RTALFxProtocol::P_MOD_SMOOTHING_MS,50.0f)+5.0f*dir,5.0f,500.0f); RTALFxLink::setFloat(RTALFxProtocol::P_MOD_SMOOTHING_MS,v);
      }
      break;
    case RtalScreenId::FxRemoteChorus:
      switch (rtalUi.selected & 3) {
        case 0: RTALFxLink::setModEffectEnabled(1, !fxRemoteBool(RTALFxProtocol::P_CHORUS_ENABLE,true)); break;
        case 1: { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_CHORUS_MIX,0.35f)+0.02f*dir,0.0f,1.0f); RTALFxLink::setFloat(RTALFxProtocol::P_CHORUS_MIX,v); break; }
        case 2: { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_CHORUS_DELAY_MS,16.0f)+0.5f*dir,8.0f,24.0f); RTALFxLink::setFloat(RTALFxProtocol::P_CHORUS_DELAY_MS,v); break; }
        case 3: { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_CHORUS_TONE_HZ,12000.0f)+500.0f*dir,2000.0f,18000.0f); RTALFxLink::setFloat(RTALFxProtocol::P_CHORUS_TONE_HZ,v); break; }
      }
      break;
    case RtalScreenId::FxRemoteFlanger:
      switch (rtalUi.selected & 3) {
        case 0: RTALFxLink::setModEffectEnabled(2, !fxRemoteBool(RTALFxProtocol::P_FLANGER_ENABLE,false)); break;
        case 1: { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_FLANGER_MIX,0.35f)+0.02f*dir,0.0f,1.0f); RTALFxLink::setFloat(RTALFxProtocol::P_FLANGER_MIX,v); break; }
        case 2: { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_FLANGER_DELAY_MS,2.5f)+0.10f*dir,0.20f,12.0f); RTALFxLink::setFloat(RTALFxProtocol::P_FLANGER_DELAY_MS,v); break; }
        case 3: { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_FLANGER_FEEDBACK,0.35f)+0.05f*dir,-0.95f,0.95f); RTALFxLink::setFloat(RTALFxProtocol::P_FLANGER_FEEDBACK,v); break; }
      }
      break;
    case RtalScreenId::FxRemoteFlangerTone:
      if (rtalUi.selected==0) { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_FLANGER_HPF_HZ,80.0f)+20.0f*dir,20.0f,2000.0f); RTALFxLink::setFloat(RTALFxProtocol::P_FLANGER_HPF_HZ,v); }
      else if (rtalUi.selected==1) { float v=constrain(fxRemoteFloat(RTALFxProtocol::P_FLANGER_SATURATION,0.12f)+0.02f*dir,0.0f,1.0f); RTALFxLink::setFloat(RTALFxProtocol::P_FLANGER_SATURATION,v); }
      break;
    case RtalScreenId::FxRemotePhaser:
      switch (rtalUi.selected & 3) {
        case 0: RTALFxLink::setModEffectEnabled(3, !fxRemoteBool(RTALFxProtocol::P_PHASER_ENABLE,false)); break;
        case 1: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_PHASER_MIX,0.35f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_PHASER_MIX,v);break;}
        case 2: {int32_t st=fxRemoteIntValue(RTALFxProtocol::P_PHASER_STAGES,4);int idx=(st<=2)?0:1;idx=constrain(idx+dir,0,1);static const uint8_t vals[2]={2,4};RTALFxLink::setInt(RTALFxProtocol::P_PHASER_STAGES,vals[idx]);break;}
        case 3: {float v=constrain(fxRemoteFloat(RTALFxProtocol::P_PHASER_FEEDBACK,0.30f)+0.05f*dir,-0.95f,0.95f);RTALFxLink::setFloat(RTALFxProtocol::P_PHASER_FEEDBACK,v);break;}
      } break;
    case RtalScreenId::FxRemotePhaserTone:
      if(rtalUi.selected==0){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_PHASER_CENTER_HZ,900.0f)+50.0f*dir,200.0f,4000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_PHASER_CENTER_HZ,v);}
      else if(rtalUi.selected==1){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_PHASER_HPF_HZ,80.0f)+20.0f*dir,20.0f,2000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_PHASER_HPF_HZ,v);}
      else if(rtalUi.selected==2){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_PHASER_SATURATION,0.08f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_PHASER_SATURATION,v);}
      break;
    case RtalScreenId::FxRemoteReverb:
      if(rtalUi.selected==0) RTALFxLink::setBool(RTALFxProtocol::P_REVERB_ENABLE,!fxRemoteBool(RTALFxProtocol::P_REVERB_ENABLE,false));
      else if(rtalUi.selected==1){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_REVERB_MIX,0.28f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_REVERB_MIX,v);}
      else if(rtalUi.selected==2){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_REVERB_SIZE,0.62f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_REVERB_SIZE,v);}
      else if(rtalUi.selected==3){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_REVERB_DECAY,0.58f)+0.02f*dir,0.0f,1.0f);RTALFxLink::setFloat(RTALFxProtocol::P_REVERB_DECAY,v);}
      break;
    case RtalScreenId::FxRemoteReverbTone:
      if(rtalUi.selected==0){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_REVERB_DAMPING_HZ,7200.0f)+200.0f*dir,1200.0f,16000.0f);RTALFxLink::setFloat(RTALFxProtocol::P_REVERB_DAMPING_HZ,v);}
      else if(rtalUi.selected==1){float v=constrain(fxRemoteFloat(RTALFxProtocol::P_REVERB_PREDELAY_MS,18.0f)+2.0f*dir,0.0f,200.0f);RTALFxLink::setFloat(RTALFxProtocol::P_REVERB_PREDELAY_MS,v);}
      break;
    case RtalScreenId::FxRemoteWidth:
      if (rtalUi.selected==0) RTALFxLink::setBool(RTALFxProtocol::P_WIDTH_ENABLE,!fxRemoteBool(RTALFxProtocol::P_WIDTH_ENABLE,true));
      else if (rtalUi.selected==1) {
        float v=constrain(fxRemoteFloat(RTALFxProtocol::P_WIDTH_AMOUNT,1.0f)+0.05f*dir,0.0f,2.0f);
        RTALFxLink::setFloat(RTALFxProtocol::P_WIDTH_AMOUNT,v);
      }
      break;
    default: break;
  }
}

void drawParameterScreen(const RtalScreenDescriptor& screen) {
  drawStatus();
  if (multiPerformanceContextActive() && screen.id == RtalScreenId::PlayArpeggiator) {
    char arpTitle[16];
    snprintf(arpTitle, sizeof(arpTitle), "ARP PART %u", rtalUi.selectedMultiPart + 1);
    drawTitle("PLAY", arpTitle);
  } else if (multiPerformanceContextActive() && screen.id == RtalScreenId::PlaySequencer) {
    char seqTitle[16]; snprintf(seqTitle, sizeof(seqTitle), "SEQ PART %u", rtalUi.selectedMultiPart + 1);
    drawTitle("PLAY", seqTitle);
  } else if (multiPerformanceContextActive() && screen.id == RtalScreenId::PlaySeqEdit) {
    char seqTitle[16]; snprintf(seqTitle, sizeof(seqTitle), "SEQ EDIT P%u", rtalUi.selectedMultiPart + 1);
    drawTitle("PLAY", seqTitle);
  } else {
    drawTitle(screen.section, screen.title);
  }
  if (screen.id == RtalScreenId::SoundOsc2) {
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(7, 26);
    u8g2.print("Source: OSC1 Wavetable");
  }
  if (screen.id == RtalScreenId::SoundMixer) { drawMixer(); return; }
  uint8_t count = screen.paramCount;
  if (rtalUi.selected >= count) rtalUi.selected = 0;
  uint8_t first = rtalUi.selected;
  uint8_t second = (first + 1) % count;

  if (screen.id == RtalScreenId::SoundOsc2) {
    // OSC2 is a companion oscillator.  Keep the source explanation separate
    // and use compact labels so the two editable rows remain visually calm.
    drawOsc2Parameter(screen.params[first], 37, true);
    if (count > 1) drawOsc2Parameter(screen.params[second], 48, false);
  } else {
    drawParameter(screen.params[first], 30, true);
    if (count > 1) drawParameter(screen.params[second], 44, false);
  }

  if (count == 1) {
    u8g2.setFont(u8g2_font_5x8_tf);
    const char* context = "";
    if (screen.id == RtalScreenId::SoundNoise) context = "Adds white noise";
    if (screen.id == RtalScreenId::SoundSub) context = "One octave below";
    if (*context) { u8g2.setCursor(7, 45); u8g2.print(context); }
  }
  if (isEditScreen(screen.id)) drawEditSignalFlow(screen.flowStage);
  else if (isModScreen(screen.id)) drawModFooter(screen.flowStage);
  else if (screen.id == RtalScreenId::FxMain) drawFxFooter();
  else if (screen.id == RtalScreenId::PlayPerformance ||
           screen.id == RtalScreenId::PlayArpeggiator ||
           screen.id == RtalScreenId::PlaySequencer ||
           screen.id == RtalScreenId::PlaySeqEdit) drawPlayFooter(screen.flowStage);
  else drawSignalFlow(screen.flowStage);
}

void drawSystemRow(const char* name, const char* value, uint8_t baselineY, bool selected) {
  u8g2.setFont(u8g2_font_5x8_tf);
  if (selected) {
    u8g2.setCursor(RTALUILayout::SYSTEM_CURSOR_X, baselineY);
    u8g2.print('>');
  }
  u8g2.setCursor(RTALUILayout::SYSTEM_LABEL_X, baselineY);
  u8g2.print(name);

  // System values are intentionally left-aligned in one fixed column.
  // Right alignment made short and long values appear visually scattered.
  drawTruncated(value,
                RTALUILayout::SYSTEM_VALUE_X,
                baselineY,
                RTALUILayout::SYSTEM_VALUE_MAX_CHARS);
}

// Forward declaration required by Arduino IDE 1.8.x before the Multi editor uses it.
void drawSystemFooter(const char* text);

void drawMultiStatus() {
  u8g2.setFont(u8g2_font_5x8_tf);
  const uint8_t p = rtalUi.selectedMultiPart < RTAL_MULTI_PART_COUNT ? rtalUi.selectedMultiPart : 0;
  const MultiPart &part = multiParts[p];
  char left[24];
  snprintf(left, sizeof(left), "P%u %c%03u %.6s", p + 1,
           part.presetNumber < 30 ? 'F' : 'U', part.presetNumber, part.program.name);
  drawTruncated(left, 1, RTALUILayout::STATUS_BASELINE, 16);

  char right[16];
  snprintf(right, sizeof(right), "CH%02u V%u", part.midiChannel, activeVoiceCountForPart(p));
  int x = 127 - u8g2.getStrWidth(right);
  if (x < 87) x = 87;
  u8g2.setCursor(x, RTALUILayout::STATUS_BASELINE);
  u8g2.print(right);
  u8g2.drawHLine(0, RTALUILayout::STATUS_DIVIDER_Y, 128);
}

void drawMultiOverview() {
  drawMultiStatus();
  drawTitle("PLAY", "MULTI");
  u8g2.setFont(u8g2_font_5x8_tf);
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    // Compact 8-pixel grid keeps all four parts above the footer divider.
    const uint8_t y = 24 + p * 8;
    char line[28];
    const MultiPart &part = multiParts[p];
    const char bank = part.presetNumber < 30 ? 'F' : 'U';
    snprintf(line, sizeof(line), "%cP%u CH%02u %c%03u %uV %s",
             p == rtalUi.selectedMultiPart ? '>' : ' ',
             p + 1, part.midiChannel, bank, part.presetNumber,
             activeVoiceCountForPart(p),
             !part.enabled ? "OFF" : (part.mute ? "MUTE" : ""));
    u8g2.setCursor(0, y); u8g2.print(line);
  }
  drawPlayFooter(2);
}

enum MultiEditorField : uint8_t {
  MULTI_EDIT_PRESET = 0, MULTI_EDIT_CHANNEL, MULTI_EDIT_VOLUME,
  MULTI_EDIT_TRANSPOSE, MULTI_EDIT_MUTE, MULTI_EDIT_ENABLE,
  MULTI_EDIT_RESERVE, MULTI_EDIT_ARP_HOLD, MULTI_EDIT_ARP_GATE, MULTI_EDIT_COUNT
};

const char* multiEditorFieldName(uint8_t field) {
  static const char* const names[MULTI_EDIT_COUNT] = {
    "Preset", "MIDI Ch", "Volume", "Transpose", "Mute", "Enable", "Reserve", "Hold", "Arp Gate"
  };
  return names[field < MULTI_EDIT_COUNT ? field : 0];
}

void multiEditorValue(char* out, size_t outLen, uint8_t field, const MultiPart &part) {
  switch (field) {
    case MULTI_EDIT_PRESET: snprintf(out, outLen, "%c%03u %.6s", part.presetNumber < 30 ? 'F' : 'U', part.presetNumber, part.program.name); break;
    case MULTI_EDIT_CHANNEL: snprintf(out, outLen, "%u", part.midiChannel); break;
    case MULTI_EDIT_VOLUME: snprintf(out, outLen, "%u", part.volume); break;
    case MULTI_EDIT_TRANSPOSE: snprintf(out, outLen, "%+d", part.transpose); break;
    case MULTI_EDIT_MUTE: snprintf(out, outLen, "%s", part.mute ? "ON" : "OFF"); break;
    case MULTI_EDIT_ENABLE: snprintf(out, outLen, "%s", part.enabled ? "ON" : "OFF"); break;
    case MULTI_EDIT_RESERVE: snprintf(out, outLen, "%u", part.voiceReserve); break;
    case MULTI_EDIT_ARP_HOLD: snprintf(out, outLen, "%s", part.program.param[P_ARP_HOLD] >= 64 ? "ON" : "OFF"); break;
    case MULTI_EDIT_ARP_GATE: snprintf(out, outLen, "%u%%", part.arpGatePct); break;
    default: snprintf(out, outLen, "--"); break;
  }
}

void drawMultiEditorRow(const char* name, const char* value, uint8_t baselineY, bool selected) {
  u8g2.setFont(u8g2_font_5x8_tf);
  if (selected) {
    u8g2.setCursor(0, baselineY);
    u8g2.print('>');
  }
  u8g2.setCursor(7, baselineY);
  u8g2.print(name);

  char clipped[16];
  if (!value) value = "";
  strncpy(clipped, value, 12);
  clipped[12] = 0;
  int x = 127 - u8g2.getStrWidth(clipped);
  if (x < 65) x = 65;
  u8g2.setCursor(x, baselineY);
  u8g2.print(clipped);
}

void drawMultiPartEditor() {
  drawMultiStatus();
  char title[16]; snprintf(title, sizeof(title), "PART %u", rtalUi.selectedMultiPart + 1);
  drawTitle("MULTI", title);
  const uint8_t selectedField = rtalUi.selected % MULTI_EDIT_COUNT;

  // Three rows leave a clean one-pixel gap above the footer divider.
  uint8_t first = selectedField > 2 ? selectedField - 2 : 0;
  if (first > MULTI_EDIT_COUNT - 3) first = MULTI_EDIT_COUNT - 3;
  MultiPart &part = multiParts[rtalUi.selectedMultiPart];
  for (uint8_t row = 0; row < 3; ++row) {
    const uint8_t field = first + row;
    char value[24]; multiEditorValue(value, sizeof(value), field, part);
    drawMultiEditorRow(multiEditorFieldName(field), value,
                       RTALUILayout::ROW1_BASELINE + row * 9,
                       field == selectedField);
  }

  // FIX4: quick part navigation directly inside the part editor.
  // SOUND = previous part, EDIT = next part. The selected editor field stays unchanged.
  if (selectedField == MULTI_EDIT_PRESET) drawSystemFooter("SND< P EDIT> LOAD");
  else drawSystemFooter("SND< P EDIT> PUSH");
}

void drawMultiStorage() {
  drawMultiStatus();
  drawTitle("MULTI", "SAVE / LOAD");
  u8g2.setFont(u8g2_font_5x8_tf);

  char slotText[12];
  snprintf(slotText, sizeof(slotText), "M%03u", currentMultiSlot);
  drawSystemRow("Slot", slotText, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);

  const bool exists = multiSetupExists(currentMultiSlot);
  drawSystemRow("State", exists ? "SAVED" : "EMPTY", RTALUILayout::ROW2_BASELINE, false);
  drawSystemRow("Name", currentMultiName, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 1);

  if (rtalUi.selected == 1) {
    uint8_t cursor = rtalUi.multiNameCursor;
    if (cursor >= PROGRAM_NAME_LEN - 1) cursor = PROGRAM_NAME_LEN - 2;
    const uint8_t x = RTALUILayout::SYSTEM_VALUE_X + cursor * 6;
    if (x < 126) u8g2.drawHLine(x, RTALUILayout::ROW3_BASELINE + 2, 5);
    drawSystemFooter("TURN CHAR SND< EDIT>");
  } else {
    drawSystemFooter("TURN SLOT PUSH NAME");
  }
}

static void stepMultiNameChar(int8_t delta) {
  static const char chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
  const uint8_t maxPos = PROGRAM_NAME_LEN - 2;
  if (rtalUi.multiNameCursor > maxPos) rtalUi.multiNameCursor = maxPos;
  const uint8_t pos = rtalUi.multiNameCursor;

  // Ensure the editable position exists; spaces are valid inside a Multi name.
  size_t len = strnlen(currentMultiName, PROGRAM_NAME_LEN - 1);
  while (len <= pos && len < PROGRAM_NAME_LEN - 1) currentMultiName[len++] = ' ';
  currentMultiName[len] = 0;

  char c = currentMultiName[pos];
  int idx = 0;
  for (int i = 0; chars[i]; ++i) if (chars[i] == c) { idx = i; break; }
  const int count = (int)strlen(chars);
  idx += delta > 0 ? 1 : -1;
  if (idx < 0) idx = count - 1;
  if (idx >= count) idx = 0;
  currentMultiName[pos] = chars[idx];
}

static void normalizeMultiNameBeforeSave() {
  int end = (int)strnlen(currentMultiName, PROGRAM_NAME_LEN - 1) - 1;
  while (end >= 0 && currentMultiName[end] == ' ') currentMultiName[end--] = 0;
  if (!currentMultiName[0]) snprintf(currentMultiName, PROGRAM_NAME_LEN, "MULTI %03u", currentMultiSlot);
}

uint8_t totalVoiceReserve() {
  uint8_t total = 0;
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) total += multiParts[p].voiceReserve;
  return total;
}

void stepMultiPartEditor(int8_t delta) {
  MultiPart &part = multiParts[rtalUi.selectedMultiPart];
  const int step = delta > 0 ? 1 : -1;
  switch (rtalUi.selected % MULTI_EDIT_COUNT) {
    case MULTI_EDIT_PRESET: {
      int n = (int)part.presetNumber + step;
      if (n < 0) n = 127; if (n > 127) n = 0;
      Program tmp;
      if (readProgramForMulti((uint8_t)n, tmp)) {
        applyPresetToMultiPart(rtalUi.selectedMultiPart, (uint8_t)n, tmp, "PRESET CHANGE");
      } else rtalUiShowMessage("EMPTY SLOT", 700);
      break;
    }
    case MULTI_EDIT_CHANNEL: {
      int n = (int)part.midiChannel + step;
      if (n < 1) n = 16; if (n > 16) n = 1;
      killMultiPartForReason(rtalUi.selectedMultiPart, "CHANNEL CHANGE");
      part.midiChannel = (uint8_t)n;
      break;
    }
    case MULTI_EDIT_VOLUME: part.volume = constrain((int)part.volume + step, 0, 127); break;
    case MULTI_EDIT_TRANSPOSE: part.transpose = constrain((int)part.transpose + step, -24, 24); break;
    case MULTI_EDIT_MUTE:
      part.mute = !part.mute;
      if (part.mute) silenceMultiPartForPerformance(rtalUi.selectedMultiPart, "MUTE");
      break;
    case MULTI_EDIT_ENABLE:
      part.enabled = !part.enabled;
      if (!part.enabled) silenceMultiPartForPerformance(rtalUi.selectedMultiPart, "DISABLE");
      break;
    case MULTI_EDIT_RESERVE: {
      int n = (int)part.voiceReserve + step;
      n = constrain(n, 0, NUM_VOICES);
      const uint8_t other = totalVoiceReserve() - part.voiceReserve;
      if (other + n <= NUM_VOICES) part.voiceReserve = (uint8_t)n;
      else {
        char msg[20];
        snprintf(msg, sizeof(msg), "RESERVE MAX %u", (unsigned)NUM_VOICES);
        rtalUiShowMessage(msg, 800);
      }
      break;
    }
    case MULTI_EDIT_ARP_HOLD: {
      const uint8_t next = part.program.param[P_ARP_HOLD] >= 64 ? 0 : 127;
      multiArpSetParam(rtalUi.selectedMultiPart, P_ARP_HOLD, next);
      break;
    }
    case MULTI_EDIT_ARP_GATE: {
      int n = (int)part.arpGatePct + (step * 5);
      n = constrain(n, 10, 100);
      multiArpSetGatePercent(rtalUi.selectedMultiPart, (uint8_t)n);
      break;
    }
  }
}

void drawSystemFooter(const char* text) {
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, text, false);
}

void drawSystemSectionFooter(uint8_t activeIndex) {
  static const char* const labels[] = { "INF", "AUD", "DIA", "STO", "ABT" };
  static const uint8_t positions[] = { 1, 25, 50, 76, 103 };
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  for (uint8_t i = 0; i < 5; ++i) {
    RTALFooterFont::drawText(u8g2, positions[i], RTALUILayout::FOOTER_TEXT_Y, labels[i], false);
    if (i == activeIndex) {
      const uint8_t width = RTALFooterFont::textWidth(labels[i]);
      u8g2.drawHLine(positions[i], RTALUILayout::FOOTER_UNDERLINE_Y, width);
    }
  }
}

RtalScreenId nextSystemReadOnlyScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::SystemInfo:        return RtalScreenId::SystemAudio;
    case RtalScreenId::SystemAudio:       return RtalScreenId::SystemDiagnostics;
    case RtalScreenId::SystemDiagnostics: return RtalScreenId::SystemStorage;
    case RtalScreenId::SystemStorage:     return RtalScreenId::SystemAbout;
    default:                              return RtalScreenId::SystemInfo;
  }
}

bool isSystemReadOnlyScreen(RtalScreenId id) {
  return id == RtalScreenId::SystemInfo ||
         id == RtalScreenId::SystemAudio ||
         id == RtalScreenId::SystemDiagnostics ||
         id == RtalScreenId::SystemStorage ||
         id == RtalScreenId::SystemAbout;
}


void drawCcLearnPage() {
  drawStatus();
  drawTitle("MIDI", "CC LEARN");

  const bool waiting = ccLearnMode;
  const bool assigned = !waiting && ccLearnResultState == 1;
  const uint8_t param = waiting ? learnParam : ccLearnResultParam;

  char currentCc[12];
  uint8_t mappedCc = 255;
  if (param < PARAM_COUNT) {
    if (multiPerformanceContextActive() && rtalUi.selectedMultiPart < RTAL_MULTI_PART_COUNT)
      mappedCc = multiParts[rtalUi.selectedMultiPart].program.ccMap[param];
    else
      mappedCc = currentProgram.ccMap[param];
  }
  if (assigned) mappedCc = ccLearnResultCc;
  if (mappedCc <= 127) snprintf(currentCc, sizeof(currentCc), "CC%u", mappedCc);
  else snprintf(currentCc, sizeof(currentCc), "NONE");

  drawSystemRow("Parameter", param < PARAM_COUNT ? paramNames[param] : "--",
                RTALUILayout::ROW1_BASELINE, false);
  drawSystemRow(assigned ? "Assigned" : "Current CC", currentCc,
                RTALUILayout::ROW2_BASELINE, false);
  drawSystemRow("Mode", midiCcMode ? "LEARN" : "BANKED",
                RTALUILayout::ROW3_BASELINE, false);

  const char* state = waiting ? "MOVE CONTROL" :
                      (ccLearnResultState == 1 ? "STORED" : "CANCELLED");
  drawSystemRow("Status", state, RTALUILayout::ROW4_BASELINE, false);
  drawSystemFooter(waiting ? "CC-LEARN-WAITING" : "CC-LEARN-RESULT");
}

void drawMidiSystem() {
  drawStatus();
  drawTitle("SYSTEM", "MIDI");
  if (rtalUi.selected > 3) rtalUi.selected = 0;
  char value[20];

  snprintf(value, sizeof(value), "%u", midiChannel);
  drawSystemRow("Receive Ch", value, RTALUILayout::ROW1_BASELINE, rtalUi.selected == 0);
  drawSystemRow("CC Mode", midiCcMode ? "LEARN" : "BANKED", RTALUILayout::ROW2_BASELINE, rtalUi.selected == 1);
  snprintf(value, sizeof(value), "BANK%u", midiCcBank);
  drawSystemRow("CC Bank", value, RTALUILayout::ROW3_BASELINE, rtalUi.selected == 2);
  drawSystemRow("Clock", getClockSource() ? "MIDI" : "INTERNAL", RTALUILayout::ROW4_BASELINE, rtalUi.selected == 3);
  drawSystemFooter("MIDI-CHANNEL-CC-CLOCK");
}

void drawSystemInfo() {
  drawStatus();
  drawTitle("SYSTEM", "INFO");
  drawSystemRow("Firmware", "1.3", RTALUILayout::ROW1_BASELINE, false);
  drawSystemRow("Build", FW_VERSION, RTALUILayout::ROW2_BASELINE, false);
  drawSystemRow("Profiler", RTAL_PROFILER_ENABLED ? "ON" : "OFF", RTALUILayout::ROW3_BASELINE, false);
  drawSystemRow("Status", sdCardOk ? "READY" : "NO SD", RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(0);
}

void drawSystemAudio() {
  drawStatus();
  drawTitle("SYSTEM", "AUDIO");
  char value[20];
  snprintf(value, sizeof(value), "%u/%u", (unsigned)activeVoicesLast, (unsigned)NUM_VOICES);
  drawSystemRow("Voices", value, RTALUILayout::ROW1_BASELINE, false);
  const uint32_t budgetUs = (uint32_t)(((uint64_t)AUDIO_BLOCK * 1000000ULL) / SAMPLE_RATE);
  snprintf(value, sizeof(value), "%lu/%lu us", (unsigned long)audioDspLastMicros, (unsigned long)budgetUs);
  drawSystemRow("DSP/Budget", value, RTALUILayout::ROW2_BASELINE, false);
  snprintf(value, sizeof(value), "%lu us", (unsigned long)audioDspMaxMicros);
  drawSystemRow("DSP Max", value, RTALUILayout::ROW3_BASELINE, false);
  snprintf(value, sizeof(value), "%lu", (unsigned long)audioDspOverruns);
  drawSystemRow("Overruns", value, RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(1);
}

void drawSystemDiagnostics() {
  drawStatus();
  drawTitle("SYSTEM", "DIAGNOSTICS");
  char value[20];
  snprintf(value, sizeof(value), "%lu", (unsigned long)midiCcQueueDrops);
  drawSystemRow("CC Drops", value, RTALUILayout::ROW1_BASELINE, false);
  snprintf(value, sizeof(value), "%lu", (unsigned long)midiReadBurstMax);
  drawSystemRow("MIDI Burst", value, RTALUILayout::ROW2_BASELINE, false);
  if (getClockSource() == 1 && midiClockValid)
    snprintf(value, sizeof(value), "%lu us", (unsigned long)midiClockJitterMaxUs);
  else
    snprintf(value, sizeof(value), "--");
  drawSystemRow("Clock Jit Max", value, RTALUILayout::ROW3_BASELINE, false);
  snprintf(value, sizeof(value), "%lu/%lu", (unsigned long)audioQueueDrops, (unsigned long)audioQueueMaxDepth);
  drawSystemRow("Q Drop/Max", value, RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(2);
}

void drawSystemStorage() {
  drawStatus();
  drawTitle("SYSTEM", "STORAGE");
  char value[20];
  drawSystemRow("SD Card", sdCardOk ? "OK" : "NOT FOUND", RTALUILayout::ROW1_BASELINE, false);
  snprintf(value, sizeof(value), "%c%c%c%c",
           multiSdWtCacheReady[0] ? 'R' : '-', multiSdWtCacheReady[1] ? 'R' : '-',
           multiSdWtCacheReady[2] ? 'R' : '-', multiSdWtCacheReady[3] ? 'R' : '-');
  drawSystemRow("WT Cache P1-4", value, RTALUILayout::ROW2_BASELINE, false);
  snprintf(value, sizeof(value), "%lu/%lu", (unsigned long)multiSdWtCacheLoadCount, (unsigned long)multiSdWtCacheLoadFailCount);
  drawSystemRow("WT Load/Fail", value, RTALUILayout::ROW3_BASELINE, false);
  snprintf(value, sizeof(value), "%u/%u KB", (unsigned)(minFreeHeapSeen / 1024U), (unsigned)(minFreePsramSeen / 1024U));
  drawSystemRow("Min Heap/PS", value, RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(3);
}

void drawSystemAbout() {
  drawStatus();
  drawTitle("SYSTEM", "ABOUT");
  drawSystemRow("Instrument", "WELLENBAD", RTALUILayout::ROW1_BASELINE, false);
  drawSystemRow("Firmware", "1.4", RTALUILayout::ROW2_BASELINE, false);
  drawSystemRow("UI", "RTAL UI 1.0", RTALUILayout::ROW3_BASELINE, false);
  drawSystemRow("Build", FW_VERSION, RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(4);
}

void normalizeCurrentSelection() {
  if (rtalUi.screen == RtalScreenId::PlayMultiOverview) {
    if (rtalUi.selectedMultiPart >= RTAL_MULTI_PART_COUNT) rtalUi.selectedMultiPart = 0;
    return;
  }
  if (rtalUi.screen == RtalScreenId::PlayMultiPart) {
    if (rtalUi.selected >= MULTI_EDIT_COUNT) rtalUi.selected = 0;
    if (rtalUi.selectedMultiPart >= RTAL_MULTI_PART_COUNT) rtalUi.selectedMultiPart = 0;
    return;
  }
  if (rtalUi.screen == RtalScreenId::PlayMultiStorage) {
    if (rtalUi.selected > 1) rtalUi.selected = 0;
    if (rtalUi.multiNameCursor >= PROGRAM_NAME_LEN - 1) rtalUi.multiNameCursor = 0;
    return;
  }
  if (isFxRemoteScreen(rtalUi.screen)) {
    if (rtalUi.selected >= 4) rtalUi.selected = 0;
    return;
  }
  if (rtalUi.screen == RtalScreenId::SystemMidi) {
    if (rtalUi.selected >= 4) rtalUi.selected = 0;
    return;
  }
  if (rtalUi.screen == RtalScreenId::ModMorph) {
    if (rtalUi.selected >= RTAL_MORPH_ITEM_COUNT) rtalUi.selected = 0;
    return;
  }
  const RtalScreenDescriptor* screen = descriptorFor(rtalUi.screen);
  if (screen && screen->paramCount > 0 && rtalUi.selected >= screen->paramCount) {
    rtalUi.selected = 0;
  }
}

bool isAllowedUiHelperParam(uint8_t id) {
  return id == UI_PARAM_SEQ_TABLE_MODE ||
         id == UI_PARAM_SEQ_STEP ||
         id == UI_PARAM_SEQ_VALUE;
}

void stepMidiSystem(int8_t delta) {
  switch (rtalUi.selected) {
    case 0: {
      int value = (int)midiChannel + (delta > 0 ? 1 : -1);
      if (value < 1) value = 16;
      if (value > 16) value = 1;
      // A011_FIX2: keep normal PROGRAM Part 1 MIDI routing in sync with
      // the global SYSTEM MIDI channel. Stop existing voices first to avoid
      // a stuck note if the corresponding Note-Off arrives on the old channel.
      if (!multiPerformanceContextActive() && (uint8_t)value != midiChannel) {
        allNotesOffForPart(0, true);
      }
      midiChannel = (uint8_t)value;
      if (!multiPerformanceContextActive()) {
        multiParts[0].midiChannel = midiChannel;
      }
      prefs.putUChar("midiCh", midiChannel);
      saveSystemConfigToSD();
      break;
    }
    case 1:
      setMidiCcModeGlobal(delta > 0 ? 1 : 0, true);
      break;
    case 2: {
      int value = (int)midiCcBank + (delta > 0 ? 1 : -1);
      if (value < 0) value = 2;
      if (value > 2) value = 0;
      midiCcBank = (uint8_t)value;
      prefs.putUChar("ccBank", midiCcBank);
      saveSystemConfigToSD();
      break;
    }
    case 3:
      setClockSourceGlobal(delta > 0 ? 1 : 0, true);
      break;
  }
}

} // namespace

void rtalUiBegin() {
  rtalUi.active = RTAL_UI_ENGINE_DEFAULT_ACTIVE != 0;
  rtalUi.screen = RtalScreenId::PlayHome;
  rtalUi.lastSound = RtalScreenId::SoundOsc1;
  rtalUi.lastEdit = RtalScreenId::EditFilter;
  rtalUi.lastMod = RtalScreenId::ModLfo;
  rtalUi.lastPlay = RtalScreenId::PlayPerformance;
  rtalUi.lastFx = RtalScreenId::FxMain;
  rtalUi.lastSystem = RtalScreenId::SystemMidi;
  rtalUi.selected = 0;
}

void rtalUiRunAudit() {
  uint8_t mapped[PARAM_COUNT] = {0};
  uint16_t invalidParamRefs = 0;
  uint16_t emptyEditablePages = 0;
  uint16_t duplicateRefs = 0;

  for (const auto &screen : rtalScreens) {
    if (screen.paramCount > 0 && screen.params == nullptr) {
      emptyEditablePages++;
      continue;
    }
    for (uint8_t i = 0; i < screen.paramCount; ++i) {
      const uint8_t id = screen.params[i];
      if (id < PARAM_COUNT) {
        if (mapped[id] > 0) duplicateRefs++;
        mapped[id]++;
      } else if (!isAllowedUiHelperParam(id)) {
        invalidParamRefs++;
      }
    }
  }

  uint16_t mappedUnique = 0;
  uint16_t missing = 0;
  for (uint16_t id = 0; id < PARAM_COUNT; ++id) {
    if (mapped[id]) mappedUnique++;
    else missing++;
  }

  Serial.println(F("--------------------------------"));
  Serial.println(F("RTAL UI AUDIT v1.3"));
  Serial.print(F("Parameters:       ")); Serial.println(PARAM_COUNT);
  Serial.print(F("Mapped unique:    ")); Serial.println(mappedUnique);
  Serial.print(F("Missing:          ")); Serial.println(missing);
  Serial.print(F("Duplicate refs:   ")); Serial.println(duplicateRefs);
  Serial.print(F("Invalid refs:     ")); Serial.println(invalidParamRefs);
  Serial.print(F("Invalid pages:    ")); Serial.println(emptyEditablePages);

  if (missing) {
    Serial.println(F("Missing parameter IDs:"));
    for (uint16_t id = 0; id < PARAM_COUNT; ++id) {
      if (!mapped[id]) {
        Serial.print(F("  ")); Serial.print(id); Serial.print(F(" "));
        Serial.println(paramNames[id]);
      }
    }
  }

  // P_PAN_SPREAD is intentionally available on SOUND/OUTPUT and PLAY/PERFORMANCE.
  const bool duplicatePolicyOk = duplicateRefs == 1 && mapped[P_PAN_SPREAD] == 2;
  const bool ok = missing == 0 && invalidParamRefs == 0 &&
                  emptyEditablePages == 0 && duplicatePolicyOk;
  Serial.print(F("Result:           ")); Serial.println(ok ? F("OK") : F("CHECK"));
  Serial.println(F("--------------------------------"));
}

uint8_t rtalUiSelectedMultiPart() {
  return rtalUi.selectedMultiPart < RTAL_MULTI_PART_COUNT ? rtalUi.selectedMultiPart : 0;
}
void rtalUiSetSelectedMultiPart(uint8_t partIndex) {
  if (partIndex < RTAL_MULTI_PART_COUNT) rtalUi.selectedMultiPart = partIndex;
}

bool rtalUiIsActive() { return rtalUi.active; }

void rtalUiToggle() {
  rtalUi.active = !rtalUi.active;
  rtalUi.screen = RtalScreenId::PlayHome;
  rtalUi.selected = 0;
  rtalUiShowMessage(rtalUi.active ? "RTAL UI ENGINE" : "LEGACY UI", 900);
}

void rtalUiShowMessage(const char* text, uint16_t durationMs) {
  snprintf(rtalUi.message, sizeof(rtalUi.message), "%s", text ? text : "");
  rtalUi.messageUntil = millis() + durationMs;
}

void rtalUiDraw() {
  if (!rtalUi.active) return;
  normalizeCurrentSelection();
  u8g2.clearBuffer();
  const bool ccLearnResultVisible = ccLearnResultState != 0 &&
                                    (int32_t)(ccLearnResultUntil - millis()) > 0;
  if (ccLearnMode || ccLearnResultVisible) drawCcLearnPage();
  else if (rtalUi.screen == RtalScreenId::PlayHome) drawHome();
  else if (rtalUi.screen == RtalScreenId::PlayMultiOverview) drawMultiOverview();
  else if (rtalUi.screen == RtalScreenId::PlayMultiPart) drawMultiPartEditor();
  else if (rtalUi.screen == RtalScreenId::PlayMultiStorage) drawMultiStorage();
  else if (rtalUi.screen == RtalScreenId::FxRemoteDelay) drawFxRemoteDelay();
  else if (rtalUi.screen == RtalScreenId::FxRemoteTiming) drawFxRemoteTiming();
  else if (rtalUi.screen == RtalScreenId::FxRemoteTone) drawFxRemoteTone();
  else if (rtalUi.screen == RtalScreenId::FxRemoteDuck) drawFxRemoteDuck();
  else if (rtalUi.screen == RtalScreenId::FxRemoteModSelect) drawFxRemoteModSelect();
  else if (rtalUi.screen == RtalScreenId::FxRemoteModCore) drawFxRemoteModCore();
  else if (rtalUi.screen == RtalScreenId::FxRemoteModStereo) drawFxRemoteModStereo();
  else if (rtalUi.screen == RtalScreenId::FxRemoteChorus) drawFxRemoteChorus();
  else if (rtalUi.screen == RtalScreenId::FxRemoteFlanger) drawFxRemoteFlanger();
  else if (rtalUi.screen == RtalScreenId::FxRemoteFlangerTone) drawFxRemoteFlangerTone();
  else if (rtalUi.screen == RtalScreenId::FxRemotePhaser) drawFxRemotePhaser();
  else if (rtalUi.screen == RtalScreenId::FxRemotePhaserTone) drawFxRemotePhaserTone();
  else if (rtalUi.screen == RtalScreenId::FxRemoteReverb) drawFxRemoteReverb();
  else if (rtalUi.screen == RtalScreenId::FxRemoteReverbTone) drawFxRemoteReverbTone();
  else if (rtalUi.screen == RtalScreenId::FxRemoteWidth) drawFxRemoteWidth();
  else if (rtalUi.screen == RtalScreenId::SystemMidi) drawMidiSystem();
  else if (rtalUi.screen == RtalScreenId::SystemInfo) drawSystemInfo();
  else if (rtalUi.screen == RtalScreenId::SystemAudio) drawSystemAudio();
  else if (rtalUi.screen == RtalScreenId::SystemDiagnostics) drawSystemDiagnostics();
  else if (rtalUi.screen == RtalScreenId::SystemStorage) drawSystemStorage();
  else if (rtalUi.screen == RtalScreenId::SystemAbout) drawSystemAbout();
  else if (rtalUi.screen == RtalScreenId::ModMorph) drawMorphPage();
  else if (rtalUi.screen == RtalScreenId::SoundWaveMon) drawRtalWaveMonitor();
  else if (rtalUi.screen == RtalScreenId::PlaySeqShow) drawRtalSeqShow();
  else {
    const auto* screen = descriptorFor(rtalUi.screen);
    if (screen) drawParameterScreen(*screen);
    else drawHome();
  }
  if (!ccLearnMode && !ccLearnResultVisible && millis() < rtalUi.messageUntil) {
    // HIS-001: messages are never drawn as inverse full-width text.
    u8g2.setFont(u8g2_font_5x8_tf);
    uint8_t w = u8g2.getStrWidth(rtalUi.message);
    uint8_t x = (128 - w) / 2;
    uint8_t lineX = x > 2 ? x - 2 : 0;
    uint8_t lineWidth = w + 4;
    if ((uint16_t)lineX + lineWidth > 128) lineWidth = 128 - lineX;
    u8g2.drawHLine(lineX, 26, lineWidth);
    u8g2.setCursor(x, 35); u8g2.print(rtalUi.message);
    u8g2.drawHLine(lineX, 38, lineWidth);
  }
  u8g2.sendBuffer();
}

bool rtalUiHandleButton(uint8_t buttonIndex, bool longPress) {
  if (!rtalUi.active) return false;
  switch (buttonIndex) {
    case 1: // SOUND
      if (!longPress && rtalUi.screen == RtalScreenId::PlayMultiStorage && rtalUi.selected == 1) {
        rtalUi.multiNameCursor = (rtalUi.multiNameCursor == 0) ? (PROGRAM_NAME_LEN - 2) : (rtalUi.multiNameCursor - 1);
        return true;
      }
      if (!longPress && (rtalUi.screen == RtalScreenId::PlayMultiPart ||
                         (multiPerformanceContextActive() && (rtalUi.screen == RtalScreenId::PlayArpeggiator ||
                                                rtalUi.screen == RtalScreenId::PlaySequencer ||
                                                rtalUi.screen == RtalScreenId::PlaySeqEdit ||
                                                rtalUi.screen == RtalScreenId::PlaySeqShow)))) {
        // v1.3.06: previous Multi part in PART editor or four-part ARP page.
        rtalUi.selectedMultiPart = (rtalUi.selectedMultiPart == 0)
                                 ? (RTAL_MULTI_PART_COUNT - 1)
                                 : (rtalUi.selectedMultiPart - 1);
        return true;
      }
      if (longPress) rtalUi.screen = RtalScreenId::SoundOsc1;
      else if (!isSoundScreen(rtalUi.screen)) rtalUi.screen = rtalUi.lastSound;
      else rtalUi.screen = nextSoundScreen(rtalUi.screen);
      rtalUi.lastSound = rtalUi.screen;
      rtalUi.selected = 0;
      if (rtalUi.screen != RtalScreenId::SoundWaveMon) rtalUi.waveMonitorTableSelect = false;
      return true;

    case 2: // EDIT
      if (!longPress && rtalUi.screen == RtalScreenId::PlayMultiStorage && rtalUi.selected == 1) {
        rtalUi.multiNameCursor = (rtalUi.multiNameCursor + 1) % (PROGRAM_NAME_LEN - 1);
        return true;
      }
      if (!longPress && (rtalUi.screen == RtalScreenId::PlayMultiPart ||
                         (multiPerformanceContextActive() && (rtalUi.screen == RtalScreenId::PlayArpeggiator ||
                                                rtalUi.screen == RtalScreenId::PlaySequencer ||
                                                rtalUi.screen == RtalScreenId::PlaySeqEdit ||
                                                rtalUi.screen == RtalScreenId::PlaySeqShow)))) {
        // v1.3.06: next Multi part in PART editor or four-part ARP page.
        rtalUi.selectedMultiPart = (rtalUi.selectedMultiPart + 1) % RTAL_MULTI_PART_COUNT;
        return true;
      }
      if (longPress) {
        // FIX28: Compare is a sound-design action and therefore belongs
        // directly to the physical EDIT key in the new UI.
        if (!compareAvailable) {
          rtalUiShowMessage("COMPARE UNAVAILABLE", 1000);
          return true;
        }
        toggleCompareMode();
        rtalUiShowMessage(compareMode ? "COMPARE: ORIGINAL"
                                      : "COMPARE: EDITED", 1200);
        return true;
      }
      if (!isEditScreen(rtalUi.screen)) rtalUi.screen = rtalUi.lastEdit;
      else rtalUi.screen = nextEditScreen(rtalUi.screen);
      rtalUi.lastEdit = rtalUi.screen;
      rtalUi.selected = 0;
      return true;

    case 3: // MOD
      if (longPress) rtalUi.screen = RtalScreenId::ModLfo;
      else if (!isModScreen(rtalUi.screen)) rtalUi.screen = rtalUi.lastMod;
      else rtalUi.screen = nextModScreen(rtalUi.screen);
      rtalUi.lastMod = rtalUi.screen;
      rtalUi.selected = 0;
      return true;

    case 4: // FX
      if (longPress) {
        rtalUi.screen = RtalScreenId::FxMain;
      } else if (rtalUi.screen == RtalScreenId::FxMain) {
        rtalUi.screen = RtalScreenId::FxRemoteDelay;
        RTALFxLink::requestState();
      } else if (isFxRemoteScreen(rtalUi.screen)) {
        rtalUi.screen = nextFxRemoteScreen(rtalUi.screen);
        if (isFxRemoteScreen(rtalUi.screen)) RTALFxLink::requestState();
      } else {
        rtalUi.screen = rtalUi.lastFx;
      }
      rtalUi.lastFx = rtalUi.screen;
      rtalUi.selected = 0;
      return true;

    case 5: // STORE
      if (!longPress && rtalUi.screen == RtalScreenId::PlayMultiStorage) {
        normalizeMultiNameBeforeSave();
        if (saveMultiSetupToSD(currentMultiSlot)) rtalUiShowMessage("MULTI SAVED", 900);
        else rtalUiShowMessage("MULTI SAVE FAIL", 1100);
        return true;
      }
      if (!longPress && multiPerformanceContextActive() &&
          (rtalUi.screen == RtalScreenId::PlayMultiOverview ||
           rtalUi.screen == RtalScreenId::PlayMultiPart)) {
        rtalUi.screen = RtalScreenId::PlayMultiStorage;
        rtalUi.selected = 0;
        return true;
      }
      if (longPress) {
        rtalUi.screen = isSystemReadOnlyScreen(rtalUi.screen)
                      ? nextSystemReadOnlyScreen(rtalUi.screen)
                      : RtalScreenId::SystemInfo;
        rtalUi.lastSystem = rtalUi.screen;
        rtalUi.selected = 0;
      } else openUserSaveMode();
      return true;
    case 6: // LOAD
      if (!longPress && rtalUi.screen == RtalScreenId::PlayMultiStorage) {
        if (loadMultiSetupFromSD(currentMultiSlot)) rtalUiShowMessage("MULTI LOADED", 900);
        else rtalUiShowMessage("EMPTY / BAD", 1000);
        return true;
      }
      if (longPress) {
        rtalUi.screen = RtalScreenId::SystemMidi;
        rtalUi.lastSystem = rtalUi.screen;
        rtalUi.selected = 0;
      } else if (rtalUi.screen == RtalScreenId::PlayMultiOverview) {
        // Overview LOAD is Multi Load. Enter PART first to browse a part preset.
        rtalUi.screen = RtalScreenId::PlayMultiStorage;
        rtalUi.selected = 0;
      } else if (rtalUi.screen == RtalScreenId::PlayMultiPart) {
        // Explicit MULTI/PART context: LOAD replaces only this Part.
        openPresetBrowserForPart(rtalUi.selectedMultiPart);
      } else {
        // FIX4: HOME/PERFORMANCE and the regular sound/edit pages use the
        // normal F/U program browser. Loading there intentionally leaves the
        // saved-Multi performance state and returns to Part-1 poly mode.
        openPresetBrowser();
      }
      return true;
    case 7: // PLAY
      if (longPress) {
        const PerformanceMode target = performanceMultiActive() ? PerformanceMode::SINGLE : PerformanceMode::MULTI;
        const bool ok = switchPerformanceMode(target);
        if (ok) {
          rtalUi.screen = target == PerformanceMode::MULTI ? RtalScreenId::PlayMultiOverview : RtalScreenId::PlayHome;
          rtalUiShowMessage(target == PerformanceMode::MULTI ? "MODE MULTI" : "MODE SINGLE", 1000);
        } else {
          rtalUi.screen = RtalScreenId::PlayHome;
          rtalUiShowMessage("MULTI EMPTY", 1100);
        }
      }
      else if (!isPlayScreen(rtalUi.screen)) rtalUi.screen = rtalUi.lastPlay;
      else rtalUi.screen = nextPlayScreen(rtalUi.screen);
      if (rtalUi.screen != RtalScreenId::PlayHome) rtalUi.lastPlay = rtalUi.screen;
      rtalUi.selected = 0;
      return true;
    case 8: // BACK / HOME
      // A015 FIX1: short T8 keeps the existing BACK behaviour; long T8 is
      // the dedicated HOME shortcut moved here from the former long-PLAY.
      if (longPress) {
        rtalUi.screen = RtalScreenId::PlayHome;
        rtalUi.selected = 0;
        return true;
      }
      if (rtalUi.screen == RtalScreenId::PlayMultiStorage) {
        rtalUi.screen = RtalScreenId::PlayMultiOverview; rtalUi.selected = 0; return true;
      }
      rtalUi.screen = RtalScreenId::PlayHome; rtalUi.selected = 0; return true;
    default:
      if (!longPress) rtalUiShowMessage("PHASE 2", 700);
      return true;
  }
}

bool rtalUiHandleEncoder(int8_t delta) {
  if (!rtalUi.active) return false;
  if (rtalUi.screen == RtalScreenId::SoundWaveMon) {
    if (rtalUi.waveMonitorTableSelect) {
      stepSelectedWavetable(delta > 0 ? 1 : -1);
    } else {
      int value = (int)getAParam(P_WAVE_POS) + (delta > 0 ? 2 : -2);
      setParam(P_WAVE_POS, constrain(value, 0, 127));
    }
    return true;
  }
  if (isFxRemoteScreen(rtalUi.screen)) {
    stepFxRemote(delta);
    return true;
  }
  if (rtalUi.screen == RtalScreenId::SystemMidi) {
    stepMidiSystem(delta);
    return true;
  }
  if (isSystemReadOnlyScreen(rtalUi.screen)) return true;
  if (rtalUi.screen == RtalScreenId::PlaySeqShow) return true;
  if (rtalUi.screen == RtalScreenId::PlayMultiStorage) {
    if (rtalUi.selected == 1) {
      stepMultiNameChar(delta);
    } else {
      int slot = (int)currentMultiSlot + (delta > 0 ? 1 : -1);
      if (slot < 0) slot = RTAL_MULTI_SLOT_COUNT - 1;
      if (slot >= RTAL_MULTI_SLOT_COUNT) slot = 0;
      currentMultiSlot = (uint8_t)slot;
      if (!readMultiSetupNameFromSD(currentMultiSlot, currentMultiName, PROGRAM_NAME_LEN))
        snprintf(currentMultiName, PROGRAM_NAME_LEN, "MULTI %03u", currentMultiSlot);
    }
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlayMultiOverview) {
    int p = (int)rtalUi.selectedMultiPart + (delta > 0 ? 1 : -1);
    if (p < 0) p = RTAL_MULTI_PART_COUNT - 1;
    if (p >= RTAL_MULTI_PART_COUNT) p = 0;
    rtalUi.selectedMultiPart = (uint8_t)p;
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlayMultiPart) {
    stepMultiPartEditor(delta);
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlayHome) {
    int next = (currentProgramNumber + (delta > 0 ? 1 : -1)) & 127;
    safeLoadProgram(next);
    return true;
  }
  if (rtalUi.screen == RtalScreenId::ModMorph) {
    switch (rtalUi.selected % RTAL_MORPH_ITEM_COUNT) {
      case RTAL_MORPH_ITEM_AMOUNT:
        stepUiParam(P_MORPH_AMOUNT, delta > 0 ? 1 : -1);
        break;
      case RTAL_MORPH_ITEM_SOURCE:
        stepMorphSource(delta > 0 ? 1 : -1);
        break;
      case RTAL_MORPH_ITEM_CAPTURE_A:
      case RTAL_MORPH_ITEM_CAPTURE_B:
      case RTAL_MORPH_ITEM_RANDOMIZE:
        triggerMorphUiAction(rtalUi.selected % RTAL_MORPH_ITEM_COUNT);
        break;
    }
    return true;
  }
  const auto* screen = descriptorFor(rtalUi.screen);
  if (!screen || screen->paramCount == 0) return true;
  uint8_t p = screen->params[rtalUi.selected % screen->paramCount];
  if (multiPerformanceContextActive() && rtalUi.screen == RtalScreenId::PlayArpeggiator &&
      (p == P_ARP_MODE || p == P_ARP_RATE || p == P_ARP_OCTAVES || p == P_ARP_HOLD)) {
    uint8_t oldValue = multiArpParam(rtalUi.selectedMultiPart, p);
    int value = oldValue;
    if (p == P_ARP_HOLD) value = delta > 0 ? 127 : 0;
    else value += (delta > 0 ? 1 : -1);
    if (p == P_ARP_MODE) value = constrain(value, 0, 4);
    else if (p == P_ARP_RATE) value = constrain(value, 0, 5);
    else if (p == P_ARP_OCTAVES) value = constrain(value, 1, 4);
    multiArpSetParam(rtalUi.selectedMultiPart, p, (uint8_t)value);
    return true;
  }
  if (multiPerformanceContextActive() &&
      (rtalUi.screen == RtalScreenId::PlaySequencer || rtalUi.screen == RtalScreenId::PlaySeqEdit)) {
    if (p == P_SEQ_MODE || p == P_SEQ_RATE || p == P_SEQ_STEPS || p == P_SEQ_TARGET || p == P_SEQ_DEPTH) {
      int value = (int)multiSeqParam(rtalUi.selectedMultiPart, p) + (delta > 0 ? 1 : -1);
      if (p == P_SEQ_MODE) value = constrain(value, 0, 3);
      else if (p == P_SEQ_RATE) value = constrain(value, 0, 5);
      else if (p == P_SEQ_STEPS) value = constrain(value, 1, MODSEQ_STEPS);
      else value = constrain(value, 0, 127);
      multiSeqSetParam(rtalUi.selectedMultiPart, p, (uint8_t)value);
      if (p == P_SEQ_STEPS && modSeqEditStep >= (uint8_t)value) modSeqEditStep = (uint8_t)value - 1;
      return true;
    }
    if (p == UI_PARAM_SEQ_TABLE_MODE) {
      multiSeqSetTableMode(rtalUi.selectedMultiPart, delta > 0 ? 1 : 0);
      return true;
    }
    if (p == UI_PARAM_SEQ_STEP) {
      int steps = multiSeqParam(rtalUi.selectedMultiPart, P_SEQ_STEPS);
      if (steps < 1) steps = 1; if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;
      int st = (int)modSeqEditStep + (delta > 0 ? 1 : -1);
      if (st < 0) st = 0; if (st >= steps) st = steps - 1;
      modSeqEditStep = (uint8_t)st;
      return true;
    }
    if (p == UI_PARAM_SEQ_VALUE) {
      int value = (int)multiSeqValueForPart(rtalUi.selectedMultiPart, modSeqEditStep) + (delta > 0 ? 1 : -1);
      multiSeqSetValue(rtalUi.selectedMultiPart, modSeqEditStep, (uint8_t)constrain(value, 0, 127));
      return true;
    }
  }
  // v1.3.12 FIX2: Tempo is a global master-clock value. While external MIDI
  // clock is selected it is read-only in the ARP UI; encoder turns must not
  // modify the hidden internal tempo parameter.
  if (p == P_TAP_TEMPO && getClockSource() != 0) {
    return true;
  }

  if (p == P_RANDOMIZE) {
    // Trigger once on either encoder direction. The control task performs the
    // musical randomization and resets P_RANDOMIZE to zero afterwards.
    setParam(P_RANDOMIZE, 127);
    rtalUiShowMessage("RANDOMIZED", 900);
  } else {
    if (multiPerformanceContextActive() && p < PARAM_COUNT) {
      const uint8_t part = rtalContextPart();
      int value = (int)multiParam(part, p);
      if (p == P_CLOCK_SOURCE) {
        setClockSourceGlobal(delta > 0 ? 1 : 0, true);
      } else if (p == P_TAP_TEMPO) {
        value = constrain(value + (delta > 0 ? 1 : -1), 0, 127);
        setParam(p, (uint8_t)value); // global by design
      } else if (p == P_WAVETABLE) {
        value = constrain(value + (delta > 0 ? 1 : -1), 0, WT_LAST_SLOT);
        setMultiPartParam(part, p, (uint8_t)value);
      } else if (isSwitchParam(p)) {
        setMultiPartParam(part, p, delta > 0 ? 127 : 0);
      } else {
        int maxValue = 127;
        if (p == P_LFO_TARGET) maxValue = 4;
        else if (p == P_LFO_SHAPE) maxValue = 5;
        else if (p == P_PLAY_MODE) maxValue = 3;
        else if (p == P_ARP_MODE) maxValue = 4;
        else if (p == P_ARP_RATE) maxValue = 5;
        else if (p == P_ARP_OCTAVES) { value = constrain(value + (delta > 0 ? 1 : -1), 1, 4); setMultiPartParam(part, p, (uint8_t)value); return true; }
        else if (p == P_SEQ_MODE) maxValue = 3;
        else if (p == P_SEQ_RATE) maxValue = 5;
        else if (p == P_SEQ_STEPS) { value = constrain(value + (delta > 0 ? 1 : -1), 1, MODSEQ_STEPS); setMultiPartParam(part, p, (uint8_t)value); return true; }
        value = constrain(value + (delta > 0 ? 1 : -1), 0, maxValue);
        setMultiPartParam(part, p, (uint8_t)value);
      }
    } else {
      stepUiParam(p, delta > 0 ? 1 : -1);
    }
  }
  return true;
}

bool rtalUiHandleEncoderPress(bool longPress) {
  if (!rtalUi.active) return false;
  if (longPress) {
    if (ccLearnMode) {
      cancelCcLearn();
      return true;
    }
    if (rtalUi.screen == RtalScreenId::SoundWaveMon) {
      rtalUi.waveMonitorTableSelect = false;
      rtalUiShowMessage("WAVE SCAN", 700);
    }
    else if (rtalUi.screen == RtalScreenId::PlayHome) {
      // Same semantics as LOAD on HOME: open the normal F/U browser.
      openPresetBrowser();
    }
    else if (isFxRemoteScreen(rtalUi.screen)) {
      RTALFxLink::requestState();
      rtalUiShowMessage("FX STATE REQUEST", 700);
    }
    else if (rtalUi.screen == RtalScreenId::ModMorph) {
      if ((rtalUi.selected % RTAL_MORPH_ITEM_COUNT) == RTAL_MORPH_ITEM_AMOUNT) {
        startCcLearn(P_MORPH_AMOUNT);
      } else {
        rtalUiShowMessage("NO CC LEARN", 700);
      }
    } else {
      const auto* screen = descriptorFor(rtalUi.screen);
      if (screen && screen->paramCount) {
        uint8_t param = screen->params[rtalUi.selected % screen->paramCount];
        if (param == UI_PARAM_SEQ_TABLE_MODE ||
            param == UI_PARAM_SEQ_STEP ||
            param == UI_PARAM_SEQ_VALUE) {
          rtalUiShowMessage("NO CC LEARN", 700);
        } else {
          startCcLearn(param);
        }
      }
    }
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlayMultiStorage) { rtalUi.selected ^= 1; return true; }
  if (rtalUi.screen == RtalScreenId::PlayMultiOverview) {
    rtalUi.screen = RtalScreenId::PlayMultiPart;
    rtalUi.selected = 0;
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlayMultiPart) {
    rtalUi.selected = (rtalUi.selected + 1) % MULTI_EDIT_COUNT;
    return true;
  }
  if (rtalUi.screen == RtalScreenId::SoundWaveMon) {
    rtalUi.waveMonitorTableSelect = !rtalUi.waveMonitorTableSelect;
    rtalUiShowMessage(rtalUi.waveMonitorTableSelect ? "TABLE SELECT" : "WAVE SCAN", 700);
    return true;
  }
  if (isFxRemoteScreen(rtalUi.screen)) {
    // Build0040: FX MOD/STEREO has only two editable rows.
    // LFO Left/Right are live monitor values and must never receive the cursor.
    const uint8_t editableCount =
        (rtalUi.screen == RtalScreenId::FxRemoteModStereo ||
         rtalUi.screen == RtalScreenId::FxRemoteFlangerTone ||
         rtalUi.screen == RtalScreenId::FxRemoteWidth ||
         rtalUi.screen == RtalScreenId::FxRemoteReverbTone) ? 2 :
        (rtalUi.screen == RtalScreenId::FxRemotePhaserTone) ? 3 : 4;
    rtalUi.selected = (rtalUi.selected + 1) % editableCount;
    return true;
  }
  if (rtalUi.screen == RtalScreenId::SystemMidi) {
    rtalUi.selected = (rtalUi.selected + 1) % 4;
    return true;
  }
  if (isSystemReadOnlyScreen(rtalUi.screen)) {
    rtalUi.screen = nextSystemReadOnlyScreen(rtalUi.screen);
    rtalUi.lastSystem = rtalUi.screen;
    rtalUi.selected = 0;
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlayHome) return true;
  if (rtalUi.screen == RtalScreenId::ModMorph) {
    rtalUi.selected = (rtalUi.selected + 1) % RTAL_MORPH_ITEM_COUNT;
    return true;
  }
  if (rtalUi.screen == RtalScreenId::PlaySeqShow) return true;
  const auto* screen = descriptorFor(rtalUi.screen);
  if (screen && screen->paramCount) rtalUi.selected = (rtalUi.selected + 1) % screen->paramCount;
  return true;
}

#endif
