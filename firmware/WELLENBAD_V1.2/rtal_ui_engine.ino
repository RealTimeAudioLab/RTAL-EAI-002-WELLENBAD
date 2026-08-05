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
  { RtalScreenId::PlayArpeggiator, "PLAY",  "ARPEGGIATOR", rtalParamsArp,       6, 2 },
  { RtalScreenId::PlaySequencer,   "PLAY",  "SEQUENCER",   rtalParamsSeq,       5, 3 },
  { RtalScreenId::PlaySeqEdit,     "PLAY",  "SEQ EDIT",    rtalParamsSeqEdit,   3, 4 },
  { RtalScreenId::PlaySeqShow,     "PLAY",  "SEQ SHOW",    nullptr,             0, 5 }
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
         id == RtalScreenId::PlaySeqShow;
}

RtalScreenId nextPlayScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::PlayHome: return RtalScreenId::PlayPerformance;
    case RtalScreenId::PlayPerformance: return RtalScreenId::PlayArpeggiator;
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

void buildProgramHeaderName(char* out, size_t outSize) {
  if (!out || outSize == 0) return;
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
  if (p == P_LFO_SHAPE) return getAParam(P_LFO_SHAPE);
  if (p == UI_PARAM_SEQ_TABLE_MODE || p == UI_PARAM_SEQ_STEP || p == UI_PARAM_SEQ_VALUE) return uiParamValue(p);
  return getAParam(p);
}

void formatParam(uint8_t p, char* out, size_t outSize) {
  uint8_t value = rtalParamNumericValue(p);
  if (p == P_WAVETABLE) {
    snprintf(out, outSize, "%s", currentWavetableName());
  } else if ((p == P_NOISE_LEVEL || p == P_SUB_LEVEL) && value == 0) {
    snprintf(out, outSize, "OFF");
  } else if (p == P_PAN_SPREAD && value == 0) {
    snprintf(out, outSize, "CENTER");
  } else if (p == P_LFO_TARGET) {
    snprintf(out, outSize, "%s", lfoTargetName(value));
  } else if (p == P_LFO_SHAPE) {
    snprintf(out, outSize, "%s", lfoShapeName(getAParam(P_LFO_SHAPE)));
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
  } else if (p == P_SEQ_MODE) {
    snprintf(out, outSize, "%s", seqModeName(value));
  } else if (p == P_SEQ_RATE) {
    snprintf(out, outSize, "%s", seqRateName(value));
  } else if (p == P_SEQ_TARGET) {
    snprintf(out, outSize, "%s", seqTargetName(value));
  } else if (p == UI_PARAM_SEQ_TABLE_MODE) {
    snprintf(out, outSize, "%s", seqTableModeName(modSeqTableMode));
  } else if (p == UI_PARAM_SEQ_STEP) {
    uint8_t steps = getAParam(P_SEQ_STEPS);
    if (steps < 1) steps = 1;
    if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;
    snprintf(out, outSize, "%u/%u", (unsigned)(modSeqEditStep + 1), (unsigned)steps);
  } else if (p == UI_PARAM_SEQ_VALUE) {
    const uint8_t stepValue = modSeqValues[modSeqEditStep];
    if (seqTargetIsTable()) {
      if (modSeqTableMode == 0) snprintf(out, outSize, "WT%03u", (unsigned)stepValue);
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
    case P_CLOCK_SOURCE:
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
  drawBar(getAParam(p), baselineY);
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
  // FIX33: the footer mirrors the actual PLAY page order. EDIT and SHOW are
  // separate workflow stages, rather than both appearing as generic SEQ.
  static const char* flow = "HOME-PERF-ARP-SEQ-EDIT-SHOW";
  static const uint8_t startChar[] = {0, 5, 10, 14, 18, 23};
  static const uint8_t charCount[] = {4, 4, 3, 3, 4, 4};
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, flow, false);
  if (activeStage < 6) {
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
  drawTruncated(currentWavetableName(), RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW1_BASELINE, 16);

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW2_BASELINE); u8g2.print("Filter");
  snprintf(value, sizeof(value), "%u", getAParam(P_CUTOFF));
  u8g2.setCursor(RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW2_BASELINE); u8g2.print(value);

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW3_BASELINE); u8g2.print("ARP");
  snprintf(value, sizeof(value), "%s %s", arpModeName(getAParam(P_ARP_MODE)), arpRateName(getAParam(P_ARP_RATE)));
  drawTruncated(value, RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW3_BASELINE, 16);

  u8g2.setCursor(RTALUILayout::PERFORMANCE_LABEL_X, RTALUILayout::ROW4_BASELINE); u8g2.print("SEQ");
  snprintf(value, sizeof(value), "%s %s", seqModeName(getAParam(P_SEQ_MODE)), seqRateName(getAParam(P_SEQ_RATE)));
  drawTruncated(value, RTALUILayout::PERFORMANCE_VALUE_X, RTALUILayout::ROW4_BASELINE, 16);

  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, "OSC1-OSC2-MIX-FIL-AMP-FX-OUT", false);
}

void drawMixer() {
  uint8_t mix = getAParam(P_OSC_MIX);
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
  drawTitle("PLAY", "SEQ SHOW");

  uint8_t steps = getAParam(P_SEQ_STEPS);
  if (steps < 1) steps = 1;
  if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;

  const uint8_t modeValue = getAParam(P_SEQ_MODE);
  const uint8_t targetValue = getAParam(P_SEQ_TARGET);
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
  uint8_t current = modSeqDisplayStep;
  if (current >= steps) current = 0;

  for (uint8_t i = 0; i < MODSEQ_STEPS; ++i) {
    const uint8_t x = 3 + i * 7;
    if (i < steps) {
      uint8_t height = 1 + ((uint16_t)modSeqValues[i] * (maxHeight - 1)) / 127U;
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

  drawPlayFooter(5);
}

void drawParameterScreen(const RtalScreenDescriptor& screen) {
  drawStatus();
  drawTitle(screen.section, screen.title);
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

void drawSystemFooter(const char* text) {
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  RTALFooterFont::drawText(u8g2, 1, RTALUILayout::FOOTER_TEXT_Y, text, false);
}

void drawSystemSectionFooter(uint8_t activeIndex) {
  static const char* const labels[] = { "INFO", "AUDIO", "STOR", "ABOUT" };
  static const uint8_t positions[] = { 1, 28, 65, 94 };
  u8g2.drawHLine(0, RTALUILayout::FOOTER_DIVIDER_Y, 128);
  for (uint8_t i = 0; i < 4; ++i) {
    RTALFooterFont::drawText(u8g2, positions[i], RTALUILayout::FOOTER_TEXT_Y, labels[i], false);
    if (i == activeIndex) {
      const uint8_t width = RTALFooterFont::textWidth(labels[i]);
      u8g2.drawHLine(positions[i], RTALUILayout::FOOTER_UNDERLINE_Y, width);
    }
  }
}

RtalScreenId nextSystemReadOnlyScreen(RtalScreenId id) {
  switch (id) {
    case RtalScreenId::SystemInfo:    return RtalScreenId::SystemAudio;
    case RtalScreenId::SystemAudio:   return RtalScreenId::SystemStorage;
    case RtalScreenId::SystemStorage: return RtalScreenId::SystemAbout;
    default:                          return RtalScreenId::SystemInfo;
  }
}

bool isSystemReadOnlyScreen(RtalScreenId id) {
  return id == RtalScreenId::SystemInfo ||
         id == RtalScreenId::SystemAudio ||
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
  uint8_t mappedCc = (param < PARAM_COUNT) ? currentProgram.ccMap[param] : 255;
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
  drawSystemRow("Firmware", "1.2", RTALUILayout::ROW1_BASELINE, false);
  drawSystemRow("Build", "RELEASE", RTALUILayout::ROW2_BASELINE, false);
  drawSystemRow("Profiler", RTAL_PROFILER_ENABLED ? "ON" : "OFF", RTALUILayout::ROW3_BASELINE, false);
  drawSystemRow("Status", sdCardOk ? "READY" : "NO SD", RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(0);
}

void drawSystemAudio() {
  drawStatus();
  drawTitle("SYSTEM", "AUDIO");
  char value[20];
  snprintf(value, sizeof(value), "%u Hz", (unsigned)SAMPLE_RATE);
  drawSystemRow("Sample Rate", value, RTALUILayout::ROW1_BASELINE, false);
  snprintf(value, sizeof(value), "%u/%u", (unsigned)activeVoicesLast, (unsigned)NUM_VOICES);
  drawSystemRow("Voices", value, RTALUILayout::ROW2_BASELINE, false);
  snprintf(value, sizeof(value), "%lu", (unsigned long)voiceSteals);
  drawSystemRow("Voice Steals", value, RTALUILayout::ROW3_BASELINE, false);
  const uint8_t waiting = audioEventQueue ? uxQueueMessagesWaiting(audioEventQueue) : 0;
  snprintf(value, sizeof(value), "%u", (unsigned)waiting);
  drawSystemRow("Queue", value, RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(1);
}

void drawSystemStorage() {
  drawStatus();
  drawTitle("SYSTEM", "STORAGE");
  char value[20];
  drawSystemRow("SD Card", sdCardOk ? "OK" : "NOT FOUND", RTALUILayout::ROW1_BASELINE, false);
  snprintf(value, sizeof(value), "%u", (unsigned)sdWavetableCount);
  drawSystemRow("Wavetables", value, RTALUILayout::ROW2_BASELINE, false);
  snprintf(value, sizeof(value), "%lu", (unsigned long)sdWavetableLoadErrors);
  drawSystemRow("Load Errors", value, RTALUILayout::ROW3_BASELINE, false);
  snprintf(value, sizeof(value), "%u KB", (unsigned)(ESP.getFreePsram() / 1024U));
  drawSystemRow("Free PSRAM", value, RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(2);
}

void drawSystemAbout() {
  drawStatus();
  drawTitle("SYSTEM", "ABOUT");
  drawSystemRow("Instrument", "WELLENBAD", RTALUILayout::ROW1_BASELINE, false);
  drawSystemRow("Firmware", "1.2", RTALUILayout::ROW2_BASELINE, false);
  drawSystemRow("UI", "RTAL UI 1.0", RTALUILayout::ROW3_BASELINE, false);
  drawSystemRow("Build", "RELEASE", RTALUILayout::ROW4_BASELINE, false);
  drawSystemSectionFooter(3);
}

void normalizeCurrentSelection() {
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
      midiChannel = (uint8_t)value;
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
  Serial.println(F("RTAL UI AUDIT v1.2"));
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
  else if (rtalUi.screen == RtalScreenId::SystemMidi) drawMidiSystem();
  else if (rtalUi.screen == RtalScreenId::SystemInfo) drawSystemInfo();
  else if (rtalUi.screen == RtalScreenId::SystemAudio) drawSystemAudio();
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
      if (longPress) rtalUi.screen = RtalScreenId::SoundOsc1;
      else if (!isSoundScreen(rtalUi.screen)) rtalUi.screen = rtalUi.lastSound;
      else rtalUi.screen = nextSoundScreen(rtalUi.screen);
      rtalUi.lastSound = rtalUi.screen;
      rtalUi.selected = 0;
      if (rtalUi.screen != RtalScreenId::SoundWaveMon) rtalUi.waveMonitorTableSelect = false;
      return true;

    case 2: // EDIT
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
      rtalUi.screen = RtalScreenId::FxMain;
      rtalUi.lastFx = rtalUi.screen;
      rtalUi.selected = 0;
      return true;

    case 5: // STORE
      if (longPress) {
        rtalUi.screen = isSystemReadOnlyScreen(rtalUi.screen)
                      ? nextSystemReadOnlyScreen(rtalUi.screen)
                      : RtalScreenId::SystemInfo;
        rtalUi.lastSystem = rtalUi.screen;
        rtalUi.selected = 0;
      } else openUserSaveMode();
      return true;
    case 6: // LOAD
      if (longPress) {
        rtalUi.screen = RtalScreenId::SystemMidi;
        rtalUi.lastSystem = rtalUi.screen;
        rtalUi.selected = 0;
      } else openPresetBrowser();
      return true;
    case 7: // PLAY
      if (longPress) rtalUi.screen = RtalScreenId::PlayHome;
      else if (!isPlayScreen(rtalUi.screen)) rtalUi.screen = rtalUi.lastPlay;
      else rtalUi.screen = nextPlayScreen(rtalUi.screen);
      if (rtalUi.screen != RtalScreenId::PlayHome) rtalUi.lastPlay = rtalUi.screen;
      rtalUi.selected = 0;
      return true;
    case 8: // BACK
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
  if (rtalUi.screen == RtalScreenId::SystemMidi) {
    stepMidiSystem(delta);
    return true;
  }
  if (isSystemReadOnlyScreen(rtalUi.screen)) return true;
  if (rtalUi.screen == RtalScreenId::PlaySeqShow) return true;
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
  if (p == P_RANDOMIZE) {
    // Trigger once on either encoder direction. The control task performs the
    // musical randomization and resets P_RANDOMIZE to zero afterwards.
    setParam(P_RANDOMIZE, 127);
    rtalUiShowMessage("RANDOMIZED", 900);
  } else {
    stepUiParam(p, delta > 0 ? 1 : -1);
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
    else if (rtalUi.screen == RtalScreenId::PlayHome) openPresetBrowser();
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
  if (rtalUi.screen == RtalScreenId::SoundWaveMon) {
    rtalUi.waveMonitorTableSelect = !rtalUi.waveMonitorTableSelect;
    rtalUiShowMessage(rtalUi.waveMonitorTableSelect ? "TABLE SELECT" : "WAVE SCAN", 700);
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
