#define SYSEX_MANUFACTURER 0x7D
#define SYSEX_ID1 0x50
#define SYSEX_ID2 0x50
#define SYSEX_ID3 0x47
#define SYSEX_REQ_CURRENT 0x01
#define SYSEX_REQ_BANK    0x02
#define SYSEX_DUMP_PROG   0x11
#define SYSEX_RECV_PROG   0x12

void audioAllNotesOff() {
  for (int i = 0; i < NUM_VOICES; i++) {
    voices[i].active = false;
    voices[i].sustained = false;
    voices[i].ampStage = ENV_OFF;
    voices[i].filtStage = ENV_OFF;
    voices[i].waveStage = ENV_OFF;
    voices[i].ampEnv = 0;
    voices[i].filtEnv = 0;
    voices[i].waveEnv = 0;
  }
  heldCount = 0;
  sustainPedal = false;
}


void allNotesOff() {
  panicCount++;
  // Core 0 state reset. The actual voice reset happens on Core 1.
  channelAftertouch = 0;
  modWheel = 0;
  expressionLevel = 127;
  arpCount = 0;
  arpIndex = 0;
  arpDir = 1;
  currentArpNote = 255;
  resetModSequencer();
  queueAudioEvent(AE_ALL_NOTES_OFF);
}


void processAudioEvents() {
  AudioEvent ev;
  while (audioEventQueue && xQueueReceive(audioEventQueue, &ev, 0) == pdTRUE) {
    switch (ev.type) {
      case AE_NOTE_ON:
        audioHandleNoteOn(ev.note, ev.velocity);
        break;
      case AE_NOTE_OFF:
        audioHandleNoteOff(ev.note);
        break;
      case AE_ALL_NOTES_OFF:
        audioAllNotesOff();
        break;
      case AE_SUSTAIN_RELEASE:
        audioSustainRelease();
        break;
    }
  }
}


void handleNoteOn(byte ch, byte note, byte vel) {
  if (ch != midiChannel) return;

  if (vel == 0) { handleNoteOff(ch, note, vel); return; }
  if (getAParam(P_ARP_MODE) != 0) { arpAddNote(note, vel); return; }
  queueAudioEvent(AE_NOTE_ON, note, vel);
}


void handleNoteOff(byte ch, byte note, byte vel) {
  if (ch != midiChannel) return;

  if (getAParam(P_ARP_MODE) != 0) { arpRemoveNote(note); return; }
  queueAudioEvent(AE_NOTE_OFF, note, 0);
}


void handleCC(byte ch, byte cc, byte val) {
  if (ch != midiChannel) return;

  // CC32 selects bank: 0=Bank0, 1=Bank1, 2=Bank2.
  if (cc == 32) {
    if (val <= 2) midiCcBank = val;
    else if (val < 43) midiCcBank = 0;
    else if (val < 86) midiCcBank = 1;
    else midiCcBank = 2;
    prefs.putUChar("ccBank", midiCcBank);
    saveSystemConfigToSD();
    return;
  }

  // CC106 selects CC mode: 0..63=MOPHO, 64..127=LEARN.
  if (cc == 106) {
    midiCcMode = (val >= 64) ? 1 : 0;
    prefs.putUChar("ccMode", midiCcMode);
    saveSystemConfigToSD();
    return;
  }

  // Always active globals.
  if (cc == 64) {
    sustainPedal = val >= 64;
    if (!sustainPedal) queueAudioEvent(AE_SUSTAIN_RELEASE);
    return;
  }
  if (cc == 120 || cc == 123) { allNotesOff(); return; }
  if (cc == 1) { modWheel = val; if (morphSource == 1) lastMorphAmount = 255; return; }
  if (cc == 7) { setParam(P_VOLUME, val); return; }
  if (cc == 11) { expressionLevel = val; return; }

  // CC Learn always writes to per-program ccMap[].
  if (ccLearnMode) {
    currentProgram.ccMap[learnParam] = cc;
    ccLearnMode = false;
    return;
  }

  if (midiCcMode == 0) {
    // MOPHO mode: fixed bank maps. Learned ccMap[] is ignored.
    if (midiCcBank == 2) {
      if (cc == 109 && val >= 64) { saveCurrentPresetToSD(currentProgram.name, currentProgramCategory); return; }
      if (cc == 110 && val >= 64) { loadFirstPresetFromSD(); return; }
      if (cc == 111 && val >= 64) { saveBankToSD("BANK.PBK"); return; }
      if (cc == 112 && val >= 64) { loadFirstBankFromSD(); return; }
      return;
    }
    if (applyCcBankMap(cc, val)) return;
    return;
  }

  // LEARN mode: only per-program learned mappings.
  for (int p = 0; p < PARAM_COUNT; p++) {
    if (currentProgram.ccMap[p] == cc) {
      setParam(p, val);
      return;
    }
  }
}


void handlePitchBend(byte ch, int bend) {
  if (ch != midiChannel) return;
 pitchBend = bend; }


void handleAfterTouchChannel(byte ch, byte pressure) {
  if (ch != midiChannel) return;
 channelAftertouch = pressure; if (morphSource == 2) lastMorphAmount = 255; }


void handleAfterTouchPoly(byte ch, byte note, byte pressure) {
  if (ch != midiChannel) return;
 channelAftertouch = pressure; if (morphSource == 2) lastMorphAmount = 255; }


void handleClock() {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.

  uint32_t now = micros();
  if (lastClockMicros != 0) {
    uint32_t diff = now - lastClockMicros;
    // Valid MIDI clock tick range.
    // 24 PPQN: 300 BPM ~= 8333 us, 30 BPM ~= 83333 us.
    // Wider limits allow jitter but reject garbage.
    if (diff > 4000UL && diff < 120000UL) {
      clockPeriodMicros = diff;
      // 60000000 * 100 does not fit in uint32_t.
      // Use uint64_t or the displayed external BPM is wrong.
      uint32_t bpm100 = (uint32_t)(((uint64_t)60000000ULL * 100ULL) / ((uint64_t)diff * 24ULL));
      // Clamp to useful musical range.
      if (bpm100 < 2000UL) bpm100 = 2000UL;
      if (bpm100 > 30000UL) bpm100 = 30000UL;
      midiBpm_x100 = bpm100;
      // Smooth display only. Do not use this for timing.
      midiBpmSmoothed_x100 += ((int32_t)bpm100 - (int32_t)midiBpmSmoothed_x100) >> 3;
      midiClockValid = true;
    }
  }
  lastClockMicros = now;
  if (getClockSource() != 1) return;
  if (!midiClockRunning) return;
  arpClockCounter++;
  uint8_t div = arpMidiDivider();
  if (arpClockCounter >= div) {
    arpClockCounter = 0;
    arpStepNow();
  }
  modSeqClockCounter++;
  uint8_t sdiv = modSeqMidiDivider();
  if (modSeqClockCounter >= sdiv) {
    modSeqClockCounter = 0;
    modSeqStepNow();
  }
}


void handleStart() {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.

  midiBpmSmoothed_x100 = midiBpm_x100;
  midiClockRunning = true;
  arpClockCounter = 0;
  modSeqClockCounter = 0;
  resetModSequencer();
  arpIndex = 0;
  arpDir = 1;
  arpAllNotesOff();
}


void handleStop() {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.

  midiClockRunning = false;
  arpClockCounter = 0;
  modSeqClockCounter = 0;
  arpAllNotesOff();
  allNotesOff();
}


void handleContinue() {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.
 midiClockRunning = true; }

void handleSPP(unsigned int position) {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.
 arpIndex = 0; arpDir = 1; }

void handlePC(byte ch, byte program) {
  if (ch != midiChannel) return;
  safeLoadProgram(program & 127);
}

void handleSystemExclusive(byte *array, unsigned size) {
  if (array == nullptr || size < 7) return;

  // Some MIDI library versions include F0/F7 in the callback, some provide only payload.
  uint16_t start = 0;
  if (array[0] == 0xF0) start = 1;

  if (start + 5 >= size) return;
  if (array[start + 0] != SYSEX_MANUFACTURER) return;
  if (array[start + 1] != SYSEX_ID1) return;
  if (array[start + 2] != SYSEX_ID2) return;
  if (array[start + 3] != SYSEX_ID3) return;

  uint8_t cmd = array[start + 4];

  if (cmd == SYSEX_REQ_CURRENT) {
    sendCurrentProgramDump();
    return;
  }

  if (cmd == SYSEX_REQ_BANK) {
    sendBankDump();
    return;
  }

  if (cmd == SYSEX_RECV_PROG || cmd == SYSEX_DUMP_PROG) {
    if (start + 6 >= size) return;
    uint8_t programNumber = array[start + 5] & 0x7F;
    uint16_t payloadStart = start + 6;
    uint16_t payloadEnd = size;
    if (payloadEnd > payloadStart && array[payloadEnd - 1] == 0xF7) payloadEnd--;
    receiveProgramDump(&array[payloadStart], payloadEnd - payloadStart, programNumber);
  }
}

void midiTask(void *param) {
  TickType_t lastWake = xTaskGetTickCount();
  while (true) {
    for (int i = 0; i < 32; i++) {
      if (!MIDI.read()) break;
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1));
  }
}

// ================================================================
// SysEx program dump / restore
// Manufacturer ID 0x7D is reserved for educational / non-commercial use.
// Format with boundaries:
// F0 7D 50 50 47 CMD [program] [ASCII hex payload] F7
// CMD 0x01: request current program dump
// CMD 0x02: request full bank dump, one program per SysEx packet
// CMD 0x11: current program dump response
// CMD 0x12: receive/store one program dump
// ================================================================
static inline uint8_t hexNibble(uint8_t v) {
  v &= 0x0F;
  return v < 10 ? ('0' + v) : ('A' + v - 10);
}

static inline int8_t fromHexNibble(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

void sendProgramDump(uint8_t programNumber) {
  Program tmp;
  Program *src = &currentProgram;

  if (programNumber != currentProgramNumber) {
    char key[16];
    snprintf(key, sizeof(key), "prog%03d", programNumber);
    if (prefs.getBytesLength(key) == sizeof(Program)) {
      prefs.getBytes(key, &tmp, sizeof(Program));
      src = &tmp;
    } else {
      tmp = currentProgram;
      snprintf(tmp.name, PROGRAM_NAME_LEN, "INIT %03d", programNumber);
      src = &tmp;
    }
  }

  const uint8_t rawLen = sizeof(Program);
  const uint16_t msgLen = 1 + 4 + 1 + 1 + rawLen * 2 + 1;
  static uint8_t msg[1 + 4 + 1 + 1 + sizeof(Program) * 2 + 1];

  uint16_t k = 0;
  msg[k++] = 0xF0;
  msg[k++] = SYSEX_MANUFACTURER;
  msg[k++] = SYSEX_ID1;
  msg[k++] = SYSEX_ID2;
  msg[k++] = SYSEX_ID3;
  msg[k++] = SYSEX_DUMP_PROG;
  msg[k++] = programNumber & 0x7F;

  const uint8_t *raw = (const uint8_t*)src;
  for (uint16_t i = 0; i < rawLen; i++) {
    msg[k++] = hexNibble(raw[i] >> 4);
    msg[k++] = hexNibble(raw[i]);
  }

  msg[k++] = 0xF7;
  MIDI.sendSysEx(k, msg, true);
}

void sendCurrentProgramDump() {
  sendProgramDump(currentProgramNumber);
}

void sendBankDump() {
  for (uint8_t i = 0; i < NUM_PROGRAMS; i++) {
    sendProgramDump(i);
    delay(8); // Core 0 only; gives slow MIDI DIN enough breathing room.
  }
}

bool receiveProgramDump(const uint8_t *hexPayload, uint16_t hexLen, uint8_t programNumber) {
  if (hexLen < sizeof(Program) * 2) return false;

  Program incoming;
  uint8_t *raw = (uint8_t*)&incoming;

  for (uint16_t i = 0; i < sizeof(Program); i++) {
    int8_t hi = fromHexNibble(hexPayload[i * 2]);
    int8_t lo = fromHexNibble(hexPayload[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    raw[i] = (hi << 4) | lo;
  }

  incoming.name[PROGRAM_NAME_LEN - 1] = 0;

  for (int i = 0; i < PARAM_COUNT; i++) {
    if (incoming.param[i] > 127) incoming.param[i] = 127;
    if (incoming.ccMap[i] > 127) incoming.ccMap[i] = 0;
  }

  currentProgram = incoming;
  currentProgramNumber = programNumber & 0x7F;
  syncProgramToAudio();
  saveProgram(currentProgramNumber);
  return true;
}
void setMidiCcModeGlobal(uint8_t v, bool store) {
  midiCcMode = v ? 1 : 0;
  if (store) {
    prefs.putUChar("ccMode", midiCcMode);
    saveSystemConfigToSD();
    saveSystemConfigToSD();
  }
}


static inline uint8_t getClockSource() {
  return globalClockSource ? 1 : 0;
}
void setClockSourceGlobal(uint8_t v, bool store) {
  // Accept both internal values 0/1 and raw UI/CC values 0/127.
  // 0 = INT, 1 = MIDI, >=64 = MIDI.
  if (v > 1) v = (v >= 64) ? 1 : 0;

  globalClockSource = v;

  portENTER_CRITICAL(&paramMux);
  currentProgram.param[P_CLOCK_SOURCE] = v;
  audioParams.p[P_CLOCK_SOURCE] = v;
  audioParamsSeq.p[P_CLOCK_SOURCE] = v;
  paramsDirty = true;
  portEXIT_CRITICAL(&paramMux);

  if (store) {
    bool ok = saveSystemConfigToSD();

    Serial.print("Clock Src saved to CONFIG.TXT: ");
    Serial.print(v ? "MIDI" : "INT");
    Serial.print(" SD=");
    Serial.println(ok ? "OK" : "FAIL");
  }
}
