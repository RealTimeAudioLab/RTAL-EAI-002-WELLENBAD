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


void audioPartAllNotesOff(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  for (int i = 0; i < NUM_VOICES; ++i) {
    Voice &v = voices[i];
    if (!v.active || v.partIndex != partIndex) continue;
    v.sustained = false;
    envNoteOff(v);
  }
}

void audioPartAllSoundOff(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  for (int i = 0; i < NUM_VOICES; ++i) {
    Voice &v = voices[i];
    if (!v.active || v.partIndex != partIndex) continue;
    v.active = false;
    v.sustained = false;
    v.ampStage = ENV_OFF;
    v.filtStage = ENV_OFF;
    v.waveStage = ENV_OFF;
    v.ampEnv = 0;
    v.filtEnv = 0;
    v.waveEnv = 0;
    v.partIndex = RTAL_MULTI_PART_NONE;
    v.midiChannel = 0;
  }
}

void killMultiPartForReason(uint8_t partIndex, const char *reason) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  multiArpResetPart(partIndex, true);
  resetMultiSeqPart(partIndex);
  partAllSoundOffCount++;
  queueAudioEvent(AE_PART_ALL_SOUND_OFF, 0, 0, partIndex,
                  multiParts[partIndex].midiChannel);
  Serial.print(F("MULTI PART KILL | P")); Serial.print(partIndex + 1);
  Serial.print(F(" | ")); Serial.println(reason ? reason : "INTERNAL");
}

void allNotesOffForPart(uint8_t partIndex, bool immediate) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  if (multiModeActive()) multiArpResetPart(partIndex, true);
  if (immediate) {
    partAllSoundOffCount++;
    queueAudioEvent(AE_PART_ALL_SOUND_OFF, 0, 0, partIndex,
                    multiParts[partIndex].midiChannel);
    Serial.print("MULTI CC120 ALL SOUND OFF | P"); Serial.println(partIndex + 1);
  } else {
    partAllNotesOffCount++;
    queueAudioEvent(AE_PART_ALL_NOTES_OFF, 0, 0, partIndex,
                    multiParts[partIndex].midiChannel);
    Serial.print("MULTI CC123 ALL NOTES OFF | P"); Serial.println(partIndex + 1);
  }
}

void allNotesOff() {
  panicCount++;
  // Core 0 state reset. The actual voice reset happens on Core 1.
  channelAftertouch = 0;
  modWheel = 0;
  expressionLevel = 127;
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    multiParts[p].controllers.pitchBend = 0;
    multiParts[p].controllers.modWheel = 0;
    multiParts[p].controllers.aftertouch = 0;
    multiParts[p].controllers.expression = 127;
    multiParts[p].controllers.sustainPedal = false;
  }
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
        audioHandleNoteOn(ev.note, ev.velocity, ev.partIndex, ev.midiChannel);
        break;
      case AE_NOTE_OFF:
        audioHandleNoteOff(ev.note, ev.partIndex, ev.midiChannel);
        break;
      case AE_ALL_NOTES_OFF:
        audioAllNotesOff();
        break;
      case AE_SUSTAIN_RELEASE:
        audioSustainRelease();
        break;
      case AE_PART_ALL_NOTES_OFF:
        audioPartAllNotesOff(ev.partIndex);
        break;
      case AE_PART_ALL_SOUND_OFF:
        audioPartAllSoundOff(ev.partIndex);
        break;
      case AE_PART_SUSTAIN_RELEASE:
        audioPartSustainRelease(ev.partIndex);
        break;
      case AE_ARP_NOTE_ON:
        audioHandleArpNoteOn(ev.note, ev.velocity, ev.partIndex, ev.midiChannel);
        break;
      case AE_ARP_NOTE_OFF:
        audioHandleNoteOff(ev.note, ev.partIndex, ev.midiChannel);
        break;
    }
  }
}


void handleNoteOn(byte ch, byte note, byte vel) {
  if (vel == 0) { handleNoteOff(ch, note, vel); return; }

  if (multiModeActive()) {
    bool matched = false;
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (!multiPartMatchesMidiChannel(part, ch)) continue;
      matched = true;
      if (multiArpParam(part, P_ARP_MODE) != 0) {
        multiArpAddNote(part, note, vel);
      } else {
        queueAudioEvent(AE_NOTE_ON, note, vel, part, ch);
      }
    }
    (void)matched;
    return;
  }

  if (ch != midiChannel) return;
  if (getAParam(P_ARP_MODE) != 0) { arpAddNote(note, vel); return; }
  queueAudioEvent(AE_NOTE_ON, note, vel);
}


void handleNoteOff(byte ch, byte note, byte vel) {
  if (multiModeActive()) {
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (!multiPartMatchesMidiChannel(part, ch)) continue;
      if (multiArpParam(part, P_ARP_MODE) != 0) {
        multiArpRemoveNote(part, note);
      } else {
        queueAudioEvent(AE_NOTE_OFF, note, 0, part, ch);
      }
    }
    return;
  }

  if (ch != midiChannel) return;
  if (getAParam(P_ARP_MODE) != 0) { arpRemoveNote(note); return; }
  queueAudioEvent(AE_NOTE_OFF, note, 0);
}


static void processMidiCcEvent(byte ch, byte cc, byte val);

// ================================================================
// v1.3.10 FIX3 - MIDI Clock Priority & CC Decoupling
// The FortySevenEffects parser callback must stay very short. Continuous CC
// streams are queued and processed by ControlTask, while MIDI realtime bytes
// continue to be parsed by the priority-5 MIDI task. Safety CC120/123 remain
// immediate because they are rare and should never wait behind automation.
// ================================================================
void handleCC(byte ch, byte cc, byte val) {
  // Safety commands and the legacy Bank-2 MIDI-thru/action path stay in the
  // parser task. Bank 0/1 synthesis automation is the high-volume path and is
  // deferred below. Keeping Bank 2 here also avoids concurrent MIDI-object TX
  // calls from ControlTask and MidiTask.
  if (cc == 120 || cc == 123 || midiCcBank == 2) {
    processMidiCcEvent(ch, cc, val);
    return;
  }

  if (!midiCcQueue) return;
  DeferredMidiCcEvent ev = { ch, cc, val };
  if (xQueueSend(midiCcQueue, &ev, 0) == pdTRUE) {
    midiCcDeferredCount++;
  } else {
    midiCcQueueDrops++;
  }
}

static void processMidiCcEvent(byte ch, byte cc, byte val) {
  // Safety controllers remain channel/part-specific and take priority.
  if (multiModeActive() && (cc == 120 || cc == 123)) {
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (multiPartMatchesMidiChannel(part, ch)) allNotesOffForPart(part, cc == 120);
    }
    return;
  }

  // Standard performance controllers are always per Part in MULTI mode.
  if (multiModeActive() && (cc == 1 || cc == 7 || cc == 11 || cc == 64)) {
    for (uint8_t partIndex = 0; partIndex < RTAL_MULTI_PART_COUNT; ++partIndex) {
      if (!multiPartMatchesMidiChannel(partIndex, ch)) continue;
      MultiPart &part = multiParts[partIndex];
      if (cc == 1) {
        part.controllers.modWheel = val;
      } else if (cc == 7) {
        part.volume = val;
      } else if (cc == 11) {
        part.controllers.expression = val;
      } else {
        const bool wasDown = part.controllers.sustainPedal;
        const bool isDown = val >= 64;
        part.controllers.sustainPedal = isDown;
        if (wasDown && !isDown) {
          queueAudioEvent(AE_PART_SUSTAIN_RELEASE, 0, 0, partIndex, ch);
        }
      }
    }
    return;
  }

  // CC32 remains a global system bank selector. The configured system receive
  // channel changes the bank; Bank 2 keeps its original all-channel thru logic.
  if (cc == 32) {
    if (ch != midiChannel) {
      if (midiCcBank == 2) MIDI.sendControlChange(cc, val, ch);
      return;
    }

    const uint8_t previousBank = midiCcBank;
    uint8_t selectedBank;
    if (val <= 2) selectedBank = val;
    else if (val < 43) selectedBank = 0;
    else if (val < 86) selectedBank = 1;
    else selectedBank = 2;

    midiCcBank = selectedBank;
    Serial.print("CC BANK: "); Serial.print(midiCcBank);
    Serial.print(" | MODE: "); Serial.println(midiCcMode == 0 ? "BANKED" : "LEARN");

    if (previousBank == 2 || selectedBank == 2) MIDI.sendControlChange(cc, val, ch);
    prefs.putUChar("ccBank", midiCcBank);
    saveSystemConfigToSD();
    return;
  }

  if (midiCcBank == 2) MIDI.sendControlChange(cc, val, ch);

  // Bank 2 remains an exclusive global action/thru bank. This behaviour is
  // unchanged from v1.2/v1.3.06; synthesis CC routing below is only Bank 0/1.
  if (midiCcBank == 2) {
    if (ch != midiChannel) return;
    if (cc == 109 && val >= 64) { saveCurrentPresetToSD(currentProgram.name, currentProgramCategory); return; }
    if (cc == 110 && val >= 64) { loadFirstPresetFromSD(); return; }
    if (cc == 111 && val >= 64) { saveBankToSD("BANK.PBK"); return; }
    if (cc == 112 && val >= 64) { loadFirstBankFromSD(); return; }
    return;
  }

  // ==============================================================
  // v1.3.07 MULTI: BANKED + LEARN are routed by incoming MIDI channel.
  // Every enabled/unmuted Part on that channel participates, which also means
  // layered Parts intentionally receive the same incoming controller stream.
  // ==============================================================
  if (multiModeActive()) {
    bool channelHasPart = false;
    for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
      if (multiPartMatchesMidiChannel(p, ch)) { channelHasPart = true; break; }
    }
    if (!channelHasPart) return;

    // Learning a new assignment writes into each layered Part on the incoming
    // channel. Parts on other channels retain their independent CC maps.
    if (ccLearnMode) {
      uint8_t learnedParts = 0;
      for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
        if (!multiPartMatchesMidiChannel(p, ch)) continue;
        multiParts[p].program.ccMap[learnParam] = cc;
        ++learnedParts;
#if RTAL_MIDI_CC_VERBOSE
        Serial.print("MULTI LEARN P"); Serial.print(p + 1);
        Serial.print(" PARAM "); Serial.print(learnParam);
        Serial.print(" <- CC"); Serial.println(cc);
#endif
      }
      if (learnedParts) {
        ccLearnResultState = 1;
        ccLearnResultParam = learnParam;
        ccLearnResultCc = cc;
        ccLearnResultUntil = millis() + 1800;
        ccLearnMode = false;
      }
      return;
    }

    if (midiCcMode == 0) {
      applyCcBankMapToMultiParts(ch, cc, val);
      return;
    }

    // LEARN mode uses the per-Program map only in Bank 0, exactly as before.
    if (midiCcBank != 0) {
#if RTAL_MIDI_CC_VERBOSE
      Serial.print("BANK1 CC"); Serial.print(cc);
      Serial.println(" ignored in LEARN mode");
#endif
      return;
    }

    bool anyApplied = false;
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (!multiPartMatchesMidiChannel(part, ch)) continue;
      Program &prog = multiParts[part].program;
      for (uint8_t param = 0; param < PARAM_COUNT; ++param) {
        if (prog.ccMap[param] != cc) continue;
        setMultiPartParam(part, param, val);
        anyApplied = true;
#if RTAL_MIDI_CC_VERBOSE
        Serial.print("MULTI LEARN CC"); Serial.print(cc);
        Serial.print(" -> P"); Serial.print(part + 1);
        Serial.print(" PARAM "); Serial.print(param);
        Serial.print(" = "); Serial.println(val);
#endif
        break; // preserve legacy one-parameter-per-Part lookup behaviour
      }
    }
    (void)anyApplied;
    return;
  }

  // ==============================================================
  // SINGLE mode: preserve the established v1.2 behaviour unchanged.
  // ==============================================================
  if (ch != midiChannel) return;

  if (cc == 64) {
    sustainPedal = val >= 64;
    if (!sustainPedal) queueAudioEvent(AE_SUSTAIN_RELEASE);
    return;
  }
  if (cc == 120 || cc == 123) { allNotesOff(); return; }
  if (cc == 1) { modWheel = val; if (morphSource == 1) lastMorphAmount = 255; return; }
  if (cc == 7) { setParam(P_VOLUME, val); return; }
  if (cc == 11) { expressionLevel = val; return; }

  if (ccLearnMode) {
    currentProgram.ccMap[learnParam] = cc;
    ccLearnResultState = 1;
    ccLearnResultParam = learnParam;
    ccLearnResultCc = cc;
    ccLearnResultUntil = millis() + 1800;
    ccLearnMode = false;
    return;
  }

  if (midiCcMode == 0) {
    applyCcBankMap(cc, val);
    return;
  }

  if (midiCcBank != 0) {
#if RTAL_MIDI_CC_VERBOSE
    Serial.print("BANK1 CC"); Serial.print(cc);
    Serial.println(" ignored in LEARN mode");
#endif
    return;
  }

  for (int p = 0; p < PARAM_COUNT; p++) {
    if (currentProgram.ccMap[p] == cc) {
      setParam(p, val);
#if RTAL_MIDI_CC_VERBOSE
      Serial.print("LEARN BANK0 CC"); Serial.print(cc);
      Serial.print(" -> PARAM "); Serial.print(p);
      Serial.print(" = "); Serial.println(val);
#endif
      return;
    }
  }
}


void processDeferredMidiCc() {
  if (!midiCcQueue) return;
  DeferredMidiCcEvent ev;
  uint8_t budget = RTAL_MIDI_CC_SERVICE_BUDGET;
  while (budget-- && xQueueReceive(midiCcQueue, &ev, 0) == pdTRUE) {
    processMidiCcEvent(ev.channel, ev.cc, ev.value);
  }
}

void handlePitchBend(byte ch, int bend) {
  if (multiModeActive()) {
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (multiPartMatchesMidiChannel(part, ch)) multiParts[part].controllers.pitchBend = bend;
    }
    return;
  }
  if (ch != midiChannel) return;
  pitchBend = bend;
}


void handleAfterTouchChannel(byte ch, byte pressure) {
  if (multiModeActive()) {
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (multiPartMatchesMidiChannel(part, ch)) multiParts[part].controllers.aftertouch = pressure;
    }
    return;
  }
  if (ch != midiChannel) return;
  channelAftertouch = pressure;
  if (morphSource == 2) lastMorphAmount = 255;
}


void handleAfterTouchPoly(byte ch, byte note, byte pressure) {
  // WELLENBAD currently has channel-pressure modulation depth, so poly
  // pressure is folded into the owning part's pressure state.
  if (multiModeActive()) {
    for (uint8_t part = 0; part < RTAL_MULTI_PART_COUNT; ++part) {
      if (multiPartMatchesMidiChannel(part, ch)) multiParts[part].controllers.aftertouch = pressure;
    }
    return;
  }
  if (ch != midiChannel) return;
  channelAftertouch = pressure;
  if (morphSource == 2) lastMorphAmount = 255;
}


void handleClock() {
  // FIX3: timestamp and service the LOCAL clock before attempting MIDI THRU.
  // A busy TX path (especially Bank 2 CC thru) must never delay local ARP/SEQ.
  const uint32_t now = micros();

  if (globalClockSource != 0) {
    if (lastClockMicros != 0) {
      uint32_t diff = now - lastClockMicros;
      if (diff > 4000UL && diff < 120000UL) {
        // v1.3.13: measure interval-to-interval deviation before replacing the
        // previous period. This is a diagnostic only and never feeds timing.
        if (clockPeriodMicros >= 4000UL && clockPeriodMicros < 120000UL) {
          const uint32_t jitter = (diff > clockPeriodMicros) ? (diff - clockPeriodMicros) : (clockPeriodMicros - diff);
          if (jitter > midiClockJitterMaxUs) midiClockJitterMaxUs = jitter;
        }
        midiClockTicksSeen++;
        clockPeriodMicros = diff;
        uint32_t bpm100 = (uint32_t)(((uint64_t)60000000ULL * 100ULL) / ((uint64_t)diff * 24ULL));
        if (bpm100 < 2000UL) bpm100 = 2000UL;
        if (bpm100 > 30000UL) bpm100 = 30000UL;
        midiBpm_x100 = bpm100;
        midiBpmSmoothed_x100 += ((int32_t)bpm100 - (int32_t)midiBpmSmoothed_x100) >> 3;
        midiClockValid = true;
      }
    }
    lastClockMicros = now;

    if (getClockSource() == 1 && midiClockRunning) {
      if (multiModeActive()) {
        multiArpClockTick();
        multiSeqClockTick();
      } else {
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
    }
  }

  // MIDI Clock is still always forwarded, independent of CC bank.
  MIDI.sendRealTime(midi::Clock);
}

void handleStart() {
  // FIX3: apply local transport first; MIDI THRU must not gate local timing.
  if (globalClockSource != 0) {
    midiBpmSmoothed_x100 = midiBpm_x100;
    midiClockRunning = true;
    arpClockCounter = 0;
    modSeqClockCounter = 0;
    if (multiModeActive()) {
      multiSeqStartSync();
      multiArpStartSync();
    } else {
      resetModSequencer();
      arpIndex = 0;
      arpDir = 1;
      arpAllNotesOff();
    }
  }
  MIDI.sendRealTime(midi::Start);
}

void handleStop() {
  // FIX3: stop the local engines before forwarding the transport byte.
  if (globalClockSource != 0) {
    midiClockRunning = false;
    arpClockCounter = 0;
    modSeqClockCounter = 0;
    if (multiModeActive()) { multiArpStopSync(); multiSeqStopSync(); }
    else arpAllNotesOff();
    allNotesOff();
  }
  MIDI.sendRealTime(midi::Stop);
}

void handleContinue() {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.
  midiClockRunning = true;
  if (multiModeActive()) { multiArpContinueSync(); multiSeqContinueSync(); }
}

void handleSPP(unsigned int position) {
  if (globalClockSource == 0) return;  // Ignore external MIDI sync in INT clock mode.
 if (multiModeActive()) {
    multiArpStartSync();
    multiSeqStartSync();
  } else {
    arpIndex = 0;
    arpDir = 1;
    resetModSequencer();
  }
}

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
    RTAL_PROFILE_TASK_BEGIN(midiProfileStart);

    // FIX3: aggressively drain already-complete MIDI messages. CC callbacks are
    // now O(1) queue writes, so a dense controller stream cannot monopolize the
    // parser before the next interleaved F8 realtime byte is reached.
    uint32_t burst = 0;
    for (uint32_t i = 0; i < 96; ++i) {
      if (!MIDI.read()) break;
      ++burst;
    }
    if (burst > midiReadBurstMax) midiReadBurstMax = burst;

    RTAL_PROFILE_TASK_END(RTALProfiler::SECTION_MIDI, midiProfileStart);

    updateArp();
    if (multiModeActive()) updateMultiModSequencers();

    // Keep a deterministic 1 ms service cadence for internal timing while the
    // lightweight parser can drain up to 96 complete messages per pass.
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
  if (programNumber == currentProgramNumber) snapshotAudioParamsToCurrentProgram();
  Program *src = &currentProgram;

  if (programNumber != currentProgramNumber) {
    char key[16];
    snprintf(key, sizeof(key), "prog%03d", programNumber);
    size_t len = prefs.getBytesLength(key);
    if (len == sizeof(Program)) {
      prefs.getBytes(key, &tmp, sizeof(Program));
      src = &tmp;
    } else if (len == sizeof(ProgramV3)) {
      ProgramV3 oldp; prefs.getBytes(key, &oldp, sizeof(oldp)); convertProgramV3ToV4(oldp, tmp); src = &tmp;
    } else if (len == sizeof(ProgramV2)) {
      ProgramV2 oldp;
      prefs.getBytes(key, &oldp, sizeof(oldp));
      convertProgramV2ToV3(oldp, tmp);
      src = &tmp;
    } else if (len == sizeof(ProgramV1)) {
      ProgramV1 oldp;
      prefs.getBytes(key, &oldp, sizeof(oldp));
      convertProgramV1ToV3(oldp, tmp);
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
  Program incoming;
  memset(&incoming, 0, sizeof(incoming));

  if (hexLen >= sizeof(Program) * 2) {
    uint8_t *raw = (uint8_t*)&incoming;
    for (uint16_t i = 0; i < sizeof(Program); i++) {
      int8_t hi = fromHexNibble(hexPayload[i * 2]);
      int8_t lo = fromHexNibble(hexPayload[i * 2 + 1]);
      if (hi < 0 || lo < 0) return false;
      raw[i] = (hi << 4) | lo;
    }
  } else if (hexLen >= sizeof(ProgramV3) * 2) {
    ProgramV3 oldp; uint8_t *raw = (uint8_t*)&oldp;
    for (uint16_t i = 0; i < sizeof(oldp); i++) {
      int8_t hi = fromHexNibble(hexPayload[i * 2]); int8_t lo = fromHexNibble(hexPayload[i * 2 + 1]);
      if (hi < 0 || lo < 0) return false; raw[i] = (hi << 4) | lo;
    }
    convertProgramV3ToV4(oldp, incoming);
  } else if (hexLen >= sizeof(ProgramV2) * 2) {
    ProgramV2 oldp;
    uint8_t *raw = (uint8_t*)&oldp;
    for (uint16_t i = 0; i < sizeof(oldp); i++) {
      int8_t hi = fromHexNibble(hexPayload[i * 2]);
      int8_t lo = fromHexNibble(hexPayload[i * 2 + 1]);
      if (hi < 0 || lo < 0) return false;
      raw[i] = (hi << 4) | lo;
    }
    convertProgramV2ToV3(oldp, incoming);
  } else if (hexLen >= sizeof(ProgramV1) * 2) {
    ProgramV1 oldp;
    uint8_t *raw = (uint8_t*)&oldp;
    for (uint16_t i = 0; i < sizeof(oldp); i++) {
      int8_t hi = fromHexNibble(hexPayload[i * 2]);
      int8_t lo = fromHexNibble(hexPayload[i * 2 + 1]);
      if (hi < 0 || lo < 0) return false;
      raw[i] = (hi << 4) | lo;
    }
    convertProgramV1ToV3(oldp, incoming);
  } else {
    return false;
  }

  incoming.name[PROGRAM_NAME_LEN - 1] = 0;
  for (int i = 0; i < PARAM_COUNT; i++) {
    if (incoming.param[i] > 127) incoming.param[i] = 127;
    if (incoming.ccMap[i] > 127) incoming.ccMap[i] = 255;
  }
  normalizeProgramCompatibility(incoming);

  currentProgram = incoming;
  applyProgramSequenceToRuntime(currentProgram);
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
