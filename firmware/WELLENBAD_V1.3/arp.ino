// ================================================================
// Arpeggiator
// ================================================================
void arpAddNote(uint8_t note, uint8_t vel) {
  for (int i = 0; i < arpCount; i++) if (arpNotes[i] == note) return;
  if (arpCount >= ARP_MAX_NOTES) return;

  const bool wasEmpty = (arpCount == 0);

  int pos = arpCount;
  while (pos > 0 && arpNotes[pos - 1] > note) {
    arpNotes[pos] = arpNotes[pos - 1]; arpVel[pos] = arpVel[pos - 1]; pos--;
  }
  arpNotes[pos] = note; arpVel[pos] = vel; arpCount++;

  // v1.2: Natural start behaviour for the internal clock.
  // The first note of a newly started arpeggio sounds immediately; subsequent
  // notes follow the selected rate from this exact start time. With external
  // MIDI Clock no immediate note is generated here, so the first step remains
  // aligned to the incoming MIDI clock grid.
  if (wasEmpty && getClockSource() == 0) {
    arpIndex = 0;
    arpDir = 1;
    arpLastStep = millis();
    arpStepNow();
  }
}

void arpRemoveNote(uint8_t note) {
  if (getAParam(P_ARP_HOLD) > 63) return;
  for (int i = 0; i < arpCount; i++) if (arpNotes[i] == note) {
    for (int j = i; j < arpCount - 1; j++) { arpNotes[j] = arpNotes[j + 1]; arpVel[j] = arpVel[j + 1]; }
    arpCount--; return;
  }
}

uint32_t arpStepMs() {
  uint8_t rate = getAParam(P_ARP_RATE);
  uint32_t quarter = 60000UL / bpm;
  switch (rate) {
    case 0: return quarter;
    case 1: return quarter / 2;
    case 2: return quarter / 3;
    case 3: return quarter / 4;
    case 4: return quarter / 6;
    case 5: return quarter / 8;
    default: return quarter / 4;
  }
}

uint8_t arpMidiDivider() {
  switch (getAParam(P_ARP_RATE)) {
    case 0: return 24;
    case 1: return 12;
    case 2: return 8;
    case 3: return 6;
    case 4: return 4;
    case 5: return 3;
    default: return 6;
  }
}

// ================================================================
// Timing Engine
// ================================================================
// MIDI Clock is 24 PPQN.
// One shared table is used for internal and external sequencer timing.
// This prevents INT and MIDI clock modes from drifting into different
// musical rates.
static inline uint8_t timingSeqRateIndex() {
  uint8_t r = getAParam(P_SEQ_RATE);
  if (r > 5) r = 5;
  return r;
}

static inline uint8_t timingSeqMidiDividerForRate(uint8_t r) {
  static const uint8_t divs[6] = {
    12,  // rate 0: 1/8
     6,  // rate 1: 1/16
     4,  // rate 2: 1/16T
     3,  // rate 3: 1/32
     2,  // rate 4: 1/32T
     1   // rate 5: 1/64
  };
  if (r > 5) r = 5;
  return divs[r];
}

static inline uint8_t timingSeqMidiDivider() {
  return timingSeqMidiDividerForRate(timingSeqRateIndex());
}

static inline uint32_t timingSeqStepMs(uint16_t bpmValue) {
  if (bpmValue < 20) bpmValue = 20;
  uint32_t quarter = 60000UL / bpmValue;
  uint8_t div = timingSeqMidiDivider();

  // div is in MIDI-clock ticks at 24 PPQN.
  // Internal step length must therefore be quarter * div / 24.
  uint32_t ms = (quarter * (uint32_t)div + 12UL) / 24UL;
  if (ms < 1) ms = 1;
  return ms;
}

uint8_t modSeqMidiDivider() {
  // External MIDI Clock = 24 PPQN.
  // Empirically matched to the internal sequencer speed.
  switch (getAParam(P_SEQ_RATE)) {
    case 0: return 12;
    case 1: return 6;
    case 2: return 4;
    case 3: return 3;
    case 4: return 2;
    case 5: return 1;
    default: return 3;
  }
}

uint8_t arpSelectNote() {
  uint8_t mode = getAParam(P_ARP_MODE);
  uint8_t octaves = max((uint8_t)1, getAParam(P_ARP_OCTAVES));
  if (octaves > 4) octaves = 4;
  uint8_t baseIndex = 0, octave = 0;
  if (mode == 4) { baseIndex = random(arpCount); octave = random(octaves); }
  else { baseIndex = arpIndex % arpCount; octave = (arpIndex / arpCount) % octaves; }
  uint8_t note = arpNotes[baseIndex] + octave * 12;
  if (note > 127) note = 127;
  return note;
}

void arpAdvance() {
  uint8_t mode = getAParam(P_ARP_MODE);
  uint8_t octaves = max((uint8_t)1, getAParam(P_ARP_OCTAVES));
  if (octaves > 4) octaves = 4;
  int maxSteps = arpCount * octaves;
  if (maxSteps <= 0) return;

  if (mode == 1) { arpIndex++; if (arpIndex >= maxSteps) arpIndex = 0; }
  else if (mode == 2) { arpIndex--; if (arpIndex < 0) arpIndex = maxSteps - 1; }
  else if (mode == 3) { arpIndex += arpDir; if (arpIndex >= maxSteps - 1) arpDir = -1; if (arpIndex <= 0) arpDir = 1; }
  else if (mode == 4) { arpIndex++; }
}

void arpStepNow() {
  if (getAParam(P_ARP_MODE) == 0) return;
  if (arpCount == 0) { arpAllNotesOff(); return; }
  arpAllNotesOff();
  uint8_t note = arpSelectNote();
  uint8_t vel = arpVel[arpIndex % arpCount];

  queueAudioEvent(AE_NOTE_ON, note, vel);
  currentArpNote = note;
  arpAdvance();
}

void updateMidiClockMonitor() {
  if (!midiClockRunning) return;
  if ((micros() - lastClockMicros) > 500000UL) {
    midiClockRunning = false;
    midiClockValid = false;
    midiClockTimeouts++;
    arpAllNotesOff();
    allNotesOff();
  }
}

uint16_t getMidiBpmForDisplay() {
  uint32_t now = millis();

  // OLED display only: update slowly and use +/-1 BPM hysteresis.
  // This does not affect MIDI clock timing for ARP/SEQ.
  if (now - midiBpmDisplayLastMs >= 500) {
    uint32_t x100 = midiBpmSmoothed_x100;

    if (x100 < 2000) x100 = 2000;
    if (x100 > 30000) x100 = 30000;

    // Hysteresis in x100 domain:
    // Keep the displayed value while the real BPM is still inside
    // displayed BPM +/- 1.00 BPM. This prevents 99/100 flicker.
    uint32_t center = (uint32_t)midiBpmDisplayStable * 100UL;
    if (x100 > center + 150UL) {
      midiBpmDisplayStable = (uint16_t)((x100 + 50UL) / 100UL);
    } else if (x100 + 150UL < center) {
      midiBpmDisplayStable = (uint16_t)((x100 + 50UL) / 100UL);
    }

    midiBpmDisplayLastMs = now;
  }

  return midiBpmDisplayStable;
}

void tapTempo() {
  uint32_t now = millis();
  if (lastTapTime > 0) {
    uint32_t diff = now - lastTapTime;
    if (diff > 250 && diff < 2000) {
      bpm = 60000UL / diff;
      bpm = constrain(bpm, 40, 240);
      setParam(P_TAP_TEMPO, map(bpm, 40, 240, 0, 127));
    }
  }
  lastTapTime = now;
}

static inline uint8_t switchValue(bool on) {
  return on ? 127 : 0;
}


static inline bool switchIsOn(uint8_t value) {
  return value >= 64;
}


void arpAllNotesOff() {
  if (currentArpNote != 255) {
    // Directly stop generated note without feeding arp remove logic.
    queueAudioEvent(AE_NOTE_OFF, currentArpNote, 0);
    currentArpNote = 255;
  }
}
// ================================================================
// v1.3.06 - Four-Part Multitimbral Arpeggiator
// ================================================================
uint8_t multiArpParam(uint8_t partIndex, uint8_t paramId) {
  if (partIndex >= RTAL_MULTI_PART_COUNT || paramId >= PARAM_COUNT) return 0;
  return multiParts[partIndex].program.param[paramId];
}

static inline uint8_t multiArpRateDivider(uint8_t partIndex);
static inline uint32_t multiArpStepMs(uint8_t partIndex);

uint8_t multiArpGatePercent(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return 80;
  uint8_t gate = multiParts[partIndex].arpGatePct;
  if (gate < 10) gate = 10;
  if (gate > 100) gate = 100;
  return gate;
}

void multiArpSetGatePercent(uint8_t partIndex, uint8_t value) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  value = constrain(value, 10, 100);
  multiParts[partIndex].arpGatePct = value;
}

static inline uint32_t multiArpCurrentStepUs(uint8_t partIndex) {
  if (getClockSource() == 1) {
    const uint32_t tickUs = max((uint32_t)1000UL, (uint32_t)clockPeriodMicros);
    return tickUs * (uint32_t)multiArpRateDivider(partIndex);
  }
  return multiArpStepMs(partIndex) * 1000UL;
}

static inline void multiArpArmGate(uint8_t partIndex, uint32_t nowUs) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  MultiArpState &st = multiArp[partIndex];
  const uint8_t gate = multiArpGatePercent(partIndex);
  if (gate >= 100 || st.currentNote == 255) {
    st.gatePending = false;
    st.gateOffUs = 0;
    return;
  }
  const uint32_t stepUs = multiArpCurrentStepUs(partIndex);
  uint32_t gateUs = (stepUs * (uint32_t)gate) / 100UL;
  if (gateUs < 1000UL) gateUs = 1000UL;
  st.gateOffUs = nowUs + gateUs;
  st.gatePending = true;
}

static inline void multiArpServiceGates(uint32_t nowUs) {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    MultiArpState &st = multiArp[p];
    if (!st.gatePending || st.currentNote == 255) continue;
    if ((int32_t)(nowUs - st.gateOffUs) < 0) continue;
    queueAudioEvent(AE_ARP_NOTE_OFF, st.currentNote, 0, p, multiParts[p].midiChannel);
    st.currentNote = 255;
    st.gatePending = false;
    st.gateOffUs = 0;
    ++st.gateOffCount;
  }
}

static inline uint8_t multiArpRateDivider(uint8_t partIndex) {
  switch (multiArpParam(partIndex, P_ARP_RATE)) {
    case 0: return 24; // 1/4
    case 1: return 12; // 1/8
    case 2: return 8;  // 1/8T
    case 3: return 6;  // 1/16
    case 4: return 4;  // 1/16T
    case 5: return 3;  // 1/32
    default: return 6;
  }
}

static inline uint32_t multiArpStepMs(uint8_t partIndex) {
  uint32_t quarter = 60000UL / max((uint16_t)40, bpm);
  switch (multiArpParam(partIndex, P_ARP_RATE)) {
    case 0: return quarter;
    case 1: return max(1UL, quarter / 2UL);
    case 2: return max(1UL, quarter / 3UL);
    case 3: return max(1UL, quarter / 4UL);
    case 4: return max(1UL, quarter / 6UL);
    case 5: return max(1UL, quarter / 8UL);
    default: return max(1UL, quarter / 4UL);
  }
}

void multiArpResetPart(uint8_t partIndex, bool clearInputNotes) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  MultiArpState &st = multiArp[partIndex];
  if (st.currentNote != 255) {
    queueAudioEvent(AE_ARP_NOTE_OFF, st.currentNote, 0, partIndex, multiParts[partIndex].midiChannel);
    st.currentNote = 255;
  }
  if (clearInputNotes) st.count = 0;
  st.index = 0;
  st.dir = 1;
  st.nextInternalStepUs = 0;
  st.gateOffUs = 0;
  st.gatePending = false;
  st.midiClockCounter = 0;
}

void multiArpResetAll(bool clearInputNotes) {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) multiArpResetPart(p, clearInputNotes);
}

void multiArpAddNote(uint8_t partIndex, uint8_t note, uint8_t vel) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  MultiArpState &st = multiArp[partIndex];
  for (uint8_t i = 0; i < st.count; ++i) if (st.notes[i] == note) return;
  if (st.count >= ARP_MAX_NOTES) return;

  const bool wasEmpty = st.count == 0;
  int pos = st.count;
  while (pos > 0 && st.notes[pos - 1] > note) {
    st.notes[pos] = st.notes[pos - 1];
    st.velocity[pos] = st.velocity[pos - 1];
    --pos;
  }
  st.notes[pos] = note;
  st.velocity[pos] = vel;
  ++st.count;

  // INT clock: first note sounds immediately, then joins the shared global grid.
  if (wasEmpty && getClockSource() == 0 && multiArpParam(partIndex, P_ARP_MODE) != 0) {
    const uint32_t stepUs = multiArpStepMs(partIndex) * 1000UL;
    const uint32_t nowUs = micros();
    multiArpStepNow(partIndex);
    // FIX3: deadline-based scheduler. The immediate first note starts the arp,
    // and the next step is anchored to an absolute microsecond deadline.
    st.nextInternalStepUs = nowUs + stepUs;
  }
}

void multiArpRemoveNote(uint8_t partIndex, uint8_t note) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  if (multiArpParam(partIndex, P_ARP_HOLD) >= 64) return;
  MultiArpState &st = multiArp[partIndex];
  for (uint8_t i = 0; i < st.count; ++i) {
    if (st.notes[i] != note) continue;
    for (uint8_t j = i; j + 1 < st.count; ++j) {
      st.notes[j] = st.notes[j + 1];
      st.velocity[j] = st.velocity[j + 1];
    }
    --st.count;
    if (st.index >= (int8_t)st.count) st.index = 0;
    if (st.count == 0) multiArpResetPart(partIndex, false);
    return;
  }
}

static uint8_t multiArpSelectNote(uint8_t partIndex, uint8_t &velocityOut) {
  MultiArpState &st = multiArp[partIndex];
  uint8_t mode = multiArpParam(partIndex, P_ARP_MODE);
  uint8_t octaves = multiArpParam(partIndex, P_ARP_OCTAVES);
  if (octaves < 1) octaves = 1;
  if (octaves > 4) octaves = 4;
  uint8_t baseIndex = 0, octave = 0;
  if (mode == 4) {
    baseIndex = random(st.count);
    octave = random(octaves);
  } else {
    const int positiveIndex = st.index < 0 ? 0 : st.index;
    baseIndex = positiveIndex % st.count;
    octave = (positiveIndex / st.count) % octaves;
  }
  uint16_t n = (uint16_t)st.notes[baseIndex] + (uint16_t)octave * 12U;
  if (n > 127) n = 127;
  velocityOut = st.velocity[baseIndex];
  return (uint8_t)n;
}

static void multiArpAdvance(uint8_t partIndex) {
  MultiArpState &st = multiArp[partIndex];
  uint8_t mode = multiArpParam(partIndex, P_ARP_MODE);
  uint8_t octaves = multiArpParam(partIndex, P_ARP_OCTAVES);
  if (octaves < 1) octaves = 1;
  if (octaves > 4) octaves = 4;
  int maxSteps = st.count * octaves;
  if (maxSteps <= 0) return;

  if (mode == 1) {
    ++st.index; if (st.index >= maxSteps) st.index = 0;
  } else if (mode == 2) {
    --st.index; if (st.index < 0) st.index = maxSteps - 1;
  } else if (mode == 3) {
    st.index += st.dir;
    if (st.index >= maxSteps - 1) { st.index = maxSteps - 1; st.dir = -1; }
    if (st.index <= 0) { st.index = 0; st.dir = 1; }
  } else if (mode == 4) {
    ++st.index;
  }
}

void multiArpStepNow(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  if (!multiParts[partIndex].enabled || multiParts[partIndex].mute) return;
  if (multiArpParam(partIndex, P_ARP_MODE) == 0) return;
  MultiArpState &st = multiArp[partIndex];
  if (st.count == 0) { multiArpResetPart(partIndex, false); return; }

  if (st.currentNote != 255) {
    queueAudioEvent(AE_ARP_NOTE_OFF, st.currentNote, 0, partIndex, multiParts[partIndex].midiChannel);
    st.currentNote = 255;
  }
  st.gatePending = false;
  st.gateOffUs = 0;

  uint8_t velocity = 100;
  const uint8_t note = multiArpSelectNote(partIndex, velocity);
  queueAudioEvent(AE_ARP_NOTE_ON, note, velocity, partIndex, multiParts[partIndex].midiChannel);
  st.currentNote = note;
  multiArpArmGate(partIndex, micros());
  multiArpAdvance(partIndex);
}

void multiArpClockTick() {
  // Called for every external 24-PPQN clock tick. Counters run for all parts,
  // even when no note is held, so arpeggiators that enter later join the same grid.
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    MultiArpState &st = multiArp[p];
    ++st.midiClockCounter;
    const uint8_t div = multiArpRateDivider(p);
    if (st.midiClockCounter >= div) {
      st.midiClockCounter = 0;
      multiArpStepNow(p);
    }
  }
}

void multiArpStartSync() {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    MultiArpState &st = multiArp[p];
    st.midiClockCounter = 0;
    st.index = 0;
    st.dir = 1;
    if (st.currentNote != 255) {
      queueAudioEvent(AE_ARP_NOTE_OFF, st.currentNote, 0, p, multiParts[p].midiChannel);
      st.currentNote = 255;
    }
    st.gatePending = false;
    st.gateOffUs = 0;
  }
}

void multiArpStopSync() {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    MultiArpState &st = multiArp[p];
    st.midiClockCounter = 0;
    if (st.currentNote != 255) {
      queueAudioEvent(AE_ARP_NOTE_OFF, st.currentNote, 0, p, multiParts[p].midiChannel);
      st.currentNote = 255;
    }
    st.gatePending = false;
    st.gateOffUs = 0;
  }
}

void multiArpContinueSync() {
  // Preserve index; only the shared external clock resumes.
}

void multiArpSetParam(uint8_t partIndex, uint8_t paramId, uint8_t value) {
  if (partIndex >= RTAL_MULTI_PART_COUNT || paramId >= PARAM_COUNT) return;
  MultiPart &part = multiParts[partIndex];
  const uint8_t oldValue = part.program.param[paramId];

  if (paramId == P_ARP_MODE && value > 4) value = 4;
  if (paramId == P_ARP_RATE && value > 5) value = 5;
  if (paramId == P_ARP_OCTAVES) {
    if (value < 1) value = 1;
    if (value > 4) value = 4;
  }
  if (paramId == P_ARP_HOLD) value = value >= 64 ? 127 : 0;
  part.program.param[paramId] = value;

  if (paramId == P_ARP_MODE && value == 0) {
    multiArpResetPart(partIndex, true);
  } else if (paramId == P_ARP_HOLD && oldValue >= 64 && value < 64) {
    // Same robust behaviour as the v1.2 single arp: leaving HOLD clears latches.
    multiArpResetPart(partIndex, true);
  } else if (paramId == P_ARP_RATE) {
    // Rejoin shared grid immediately at the new division.
    multiArp[partIndex].midiClockCounter = 0;
    // Re-anchor only the edited arp rate. Other sound/UI parameter changes do
    // not touch timing phase.
    multiArp[partIndex].nextInternalStepUs = micros() + multiArpStepMs(partIndex) * 1000UL;
  }
}

void multiArpDiagnosticsService() {
#if !RTAL_MULTI_ARP_TAIL_DIAGNOSTICS
  return;
#else
  if (!multiModeActive()) return;
  static uint32_t lastMs = 0;
  static uint32_t lastGateOff[RTAL_MULTI_PART_COUNT] = {0,0,0,0};
  static uint32_t lastTailReuse[RTAL_MULTI_PART_COUNT] = {0,0,0,0};
  static uint32_t lastSmooth[RTAL_MULTI_PART_COUNT] = {0,0,0,0};
  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastMs) < 3000UL) return;
  lastMs = nowMs;

  bool changed = false;
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    if (multiArp[p].gateOffCount != lastGateOff[p] ||
        multiArp[p].tailReuseCount != lastTailReuse[p] ||
        multiArp[p].smoothRetriggerCount != lastSmooth[p]) {
      changed = true; break;
    }
  }
  if (!changed) return;

  Serial.print(F("ARP TAIL | "));
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    Serial.print('P'); Serial.print(p + 1);
    Serial.print(F(" G")); Serial.print(multiArpGatePercent(p));
    Serial.print(F(" OFF")); Serial.print(multiArp[p].gateOffCount);
    Serial.print(F(" HARD")); Serial.print(multiArp[p].tailReuseCount);
    Serial.print(F(" SMOOTH")); Serial.print(multiArp[p].smoothRetriggerCount);
    Serial.print(F(" R")); Serial.print(releaseVoiceCountForPart(p));
    if (p + 1 < RTAL_MULTI_PART_COUNT) Serial.print(F(" | "));
    lastGateOff[p] = multiArp[p].gateOffCount;
    lastTailReuse[p] = multiArp[p].tailReuseCount;
    lastSmooth[p] = multiArp[p].smoothRetriggerCount;
  }
  Serial.println();
#endif
}

void updateArp() {
  if (multiModeActive()) {
    // FIX1: gate-off deadlines are serviced in the high-priority MIDI task for
    // both internal and external clock. Gate timing therefore remains isolated
    // from OLED redraws and parameter editing.
    const uint32_t nowUs = micros();
    multiArpServiceGates(nowUs);
    if (getClockSource() == 1) return;

    // FIX3: timing runs from the high-priority MIDI task and uses absolute
    // microsecond deadlines. It is therefore isolated from OLED redraws, SD
    // housekeeping and encoder/parameter editing in ControlTask.
    for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
      MultiArpState &st = multiArp[p];
      if (multiArpParam(p, P_ARP_MODE) == 0 || st.count == 0) {
        st.nextInternalStepUs = 0;
        continue;
      }

      const uint32_t stepUs = multiArpStepMs(p) * 1000UL;
      if (st.nextInternalStepUs == 0) {
        st.nextInternalStepUs = nowUs + stepUs;
        continue;
      }

      if ((int32_t)(nowUs - st.nextInternalStepUs) >= 0) {
        const uint32_t lateUs = nowUs - st.nextInternalStepUs;
        multiArpStepNow(p);

        // Preserve the musical grid after normal scheduler latency. If the
        // task was delayed by more than two complete steps, do not emit a
        // burst of catch-up notes; re-anchor cleanly from now.
        if (lateUs > stepUs * 2UL) st.nextInternalStepUs = nowUs + stepUs;
        else st.nextInternalStepUs += stepUs;
      }
    }
    return;
  }

  if (getAParam(P_ARP_MODE) == 0) return;
  if (getClockSource() == 1) return;

  uint32_t now = millis();
  uint32_t stepMs = arpStepMs();
  if (now - arpLastStep < stepMs) return;
  arpLastStep += stepMs;
  if (now - arpLastStep > stepMs * 2UL) arpLastStep = now;
  arpStepNow();
}

