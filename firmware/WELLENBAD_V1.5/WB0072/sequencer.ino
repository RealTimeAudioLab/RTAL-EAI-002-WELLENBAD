static inline bool modSeqTargetIsSafe(uint8_t t) {
  if (t >= PARAM_COUNT) return false;
  if (t == P_SEQ_MODE || t == P_SEQ_RATE || t == P_SEQ_STEPS || t == P_SEQ_TARGET || t == P_SEQ_DEPTH) return false;
  if (t == P_RANDOMIZE) return false;
  return true;
}
uint32_t modSeqStepMs() {
  // Internal sequencer timing.
  // Keep the proven original millis-based table for stable internal clock.
  uint8_t rate = getAParam(P_SEQ_RATE);
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


uint8_t modSeqTargetParam() {
  uint8_t t = getAParam(P_SEQ_TARGET);
  if (!modSeqTargetIsSafe(t)) return P_WAVE_POS;
  return t;
}


void updateModSeqAudioMirror() {
  // FAST PATH:
  // Sequencer modulation is prepared outside the per-sample audio loop.
  // audioParamsSeq receives the temporary effective value, while
  // currentProgram.param[] and audioParams.p[] remain unchanged.
  // Do not move getSeqModFor()/clamp style checks into oscRead()/filter,
  // because that reduced polyphony from 6 to about 3 voices.
  uint8_t oldTarget = modSeqMirrorLastTarget;
  uint8_t target = modSeqCurrentTarget;
  int8_t offset = modSeqOffset;

  if (!modSeqTargetIsSafe(target)) {
    target = P_WAVE_POS;
    modSeqCurrentTarget = target;
  }

  portENTER_CRITICAL(&paramMux);

  // Restore previous target back to its unmodulated base value.
  if (oldTarget < PARAM_COUNT) {
    audioParamsSeq.p[oldTarget] = audioParams.p[oldTarget];
  }

  // Apply temporary sequencer offset only to the active target mirror.
  // currentProgram.param[] and audioParams.p[] remain untouched.
  if (target < PARAM_COUNT) {
    if (modSeqAbsoluteTableActive && target == P_WAVETABLE) {
      audioParamsSeq.p[target] = modSeqAbsoluteTableValue;
      modSeqMirrorLastTarget = target;
      portEXIT_CRITICAL(&paramMux);
      return;
    }

    int16_t mv = (int16_t)audioParams.p[target] + (int16_t)offset;
    if (mv < 0) mv = 0;
    if (mv > 127) mv = 127;
    audioParamsSeq.p[target] = (uint8_t)mv;
    modSeqMirrorLastTarget = target;
  }

  portEXIT_CRITICAL(&paramMux);
}


void modSeqStepNow() {
  uint8_t mode = getAParam(P_SEQ_MODE);
  if (mode == 0) {
    modSeqOffset = 0;
    modSeqAbsoluteTableActive = false;
    updateModSeqAudioMirror();
    return;
  }

  uint8_t steps = getAParam(P_SEQ_STEPS);
  if (steps < 1) steps = 1;
  if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;

  modSeqCurrentTarget = modSeqTargetParam();

  uint8_t stepIndex = modSeqIndex;
  if (stepIndex >= steps) stepIndex = 0;
  // Record the step that is applied now, before modSeqIndex advances to the
  // next step. This keeps the graphical show page correct for forward,
  // pendulum and random modes alike.
  modSeqDisplayStep = stepIndex;

  if (modSeqCurrentTarget == P_WAVETABLE) {
    // Wavetable sequencing:
    // ABS: Seq Value is the real wavetable slot number.
    // REL: Seq Value is added to the current base wavetable.
    uint16_t table;
    if (modSeqTableMode == 0) {
      // ABS: Seq Value is the direct wavetable slot number.
      table = modSeqValues[stepIndex];
    } else {
      // REL: Seq Value is an offset added to the current base wavetable.
      table = (uint16_t)getAParam(P_WAVETABLE) + (uint16_t)modSeqValues[stepIndex];
    }
    if (table >= WT_VISIBLE_SLOTS) table = WT_LAST_SLOT;

    modSeqAbsoluteTableValue = (uint8_t)table;
    modSeqAbsoluteTableActive = true;
    modSeqOffset = 0;
    updateModSeqAudioMirror();
  } else {
    modSeqAbsoluteTableActive = false;

    int32_t centered = (int32_t)modSeqValues[stepIndex] - 64;
    int32_t depth = getAParam(P_SEQ_DEPTH);

    // Non-destructive: store only a temporary bipolar offset.
    // The target parameter itself is never overwritten with setParam().
    int32_t offset = (centered * depth) >> 6;

    if (offset < -127) offset = -127;
    if (offset > 127) offset = 127;
    modSeqOffset = (int8_t)offset;
    updateModSeqAudioMirror();
  }

  switch (mode) {
    case 1: // FORWARD
      modSeqIndex++;
      if (modSeqIndex >= steps) modSeqIndex = 0;
      break;

    case 2: // PENDULUM
      if (steps <= 1) {
        modSeqIndex = 0;
        modSeqDir = 1;
      } else {
        int16_t next = (int16_t)modSeqIndex + modSeqDir;
        if (next >= steps) {
          modSeqDir = -1;
          next = steps - 2;
        } else if (next < 0) {
          modSeqDir = 1;
          next = 1;
        }
        modSeqIndex = (uint8_t)next;
      }
      break;

    case 3: // RANDOM
      modSeqIndex = random(steps);
      break;

    default:
      modSeqIndex = 0;
      modSeqOffset = 0;
      break;
  }
}


void updateModSequencer() {
  if (multiModeActive()) return;
  if (getAParam(P_SEQ_MODE) == 0) return;

  // Bei externer Clock läuft der Mod-Sequencer synchron über MIDI Clock.
  if (getClockSource() == 1) return;

  uint32_t now = millis();
  uint32_t stepMs = modSeqStepMs();
  if (now - modSeqLastStep < stepMs) return;

  // Keep the sequence rhythmically stable even if the control task is delayed.
  // Do not reset to now every time, because that accumulates jitter.
  modSeqLastStep += stepMs;

  // If the task was blocked for a long time, resync cleanly instead of
  // firing a burst of old steps.
  if (now - modSeqLastStep > stepMs * 2UL) {
    modSeqLastStep = now;
  }

  modSeqStepNow();
}


void resetModSequencer() {
  modSeqIndex = 0;
  modSeqDisplayStep = 0;
  modSeqDir = 1;
  modSeqLastStep = millis();
}


void setSeqTableModeGlobal(uint8_t v, bool store) {
  modSeqTableMode = v ? 1 : 0;
  if (store) prefs.putUChar("seqTblMode", modSeqTableMode);
}

// ================================================================
// v1.3.09 - Four-Part Modulation Sequencer
// ================================================================
static inline uint8_t multiSeqRateDividerForPart(uint8_t partIndex) {
  switch (multiSeqParam(partIndex, P_SEQ_RATE)) {
    case 0: return 12;
    case 1: return 6;
    case 2: return 4;
    case 3: return 3;
    case 4: return 2;
    case 5: return 1;
    default: return 3;
  }
}

static inline uint32_t multiSeqStepUsForPart(uint8_t partIndex) {
  uint8_t rate = multiSeqParam(partIndex, P_SEQ_RATE);
  uint32_t quarterUs = 60000000UL / (bpm < 20 ? 20 : bpm);
  switch (rate) {
    case 0: return quarterUs;
    case 1: return quarterUs / 2UL;
    case 2: return quarterUs / 3UL;
    case 3: return quarterUs / 4UL;
    case 4: return quarterUs / 6UL;
    case 5: return quarterUs / 8UL;
    default: return quarterUs / 4UL;
  }
}

uint8_t multiSeqParam(uint8_t partIndex, uint8_t id) {
  if (partIndex >= RTAL_MULTI_PART_COUNT || id >= PARAM_COUNT) return 0;
  return multiParts[partIndex].program.param[id];
}

uint8_t multiSeqTableModeForPart(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return 0;
  return multiParts[partIndex].program.seqTableMode ? 1 : 0;
}

uint8_t multiSeqValueForPart(uint8_t partIndex, uint8_t step) {
  if (partIndex >= RTAL_MULTI_PART_COUNT || step >= PROGRAM_SEQ_STEPS) return 0;
  return multiParts[partIndex].program.seqValues[step];
}

uint8_t multiSeqDisplayStepForPart(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return 0;
  return multiSeq[partIndex].displayStep;
}

void resetMultiSeqPart(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  MultiSeqState &st = multiSeq[partIndex];
  st.index = 0;
  st.displayStep = 0;
  st.dir = 1;
  st.midiClockCounter = 0;
  st.nextInternalStepUs = 0;
  st.offset = 0;
  st.currentTarget = P_WAVE_POS;
  st.absoluteTableActive = false;
  st.absoluteTableValue = 0;
}

void multiSeqSetParam(uint8_t partIndex, uint8_t id, uint8_t value) {
  if (partIndex >= RTAL_MULTI_PART_COUNT || id >= PARAM_COUNT) return;
  if (id == P_SEQ_MODE && value > 3) value = 3;
  if (id == P_SEQ_RATE && value > 5) value = 5;
  if (id == P_SEQ_STEPS) {
    if (value < 1) value = 1;
    if (value > MODSEQ_STEPS) value = MODSEQ_STEPS;
  }
  if (id == P_SEQ_TARGET && (!modSeqTargetIsSafe(value))) value = P_WAVE_POS;

  multiParts[partIndex].program.param[id] = value;

  MultiSeqState &st = multiSeq[partIndex];
  if (id == P_SEQ_RATE) {
    st.midiClockCounter = 0;
    st.nextInternalStepUs = micros() + multiSeqStepUsForPart(partIndex);
  }
  if (id == P_SEQ_MODE || id == P_SEQ_STEPS || id == P_SEQ_TARGET || id == P_SEQ_DEPTH) {
    st.index = 0;
    st.displayStep = 0;
    st.dir = 1;
    st.offset = 0;
    st.absoluteTableActive = false;
    st.currentTarget = modSeqTargetIsSafe(multiParts[partIndex].program.param[P_SEQ_TARGET])
                     ? multiParts[partIndex].program.param[P_SEQ_TARGET] : P_WAVE_POS;
    st.nextInternalStepUs = 0;
  }
}

void multiSeqSetTableMode(uint8_t partIndex, uint8_t value) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  multiParts[partIndex].program.seqTableMode = value ? 1 : 0;
  multiSeq[partIndex].absoluteTableActive = false;
}

void multiSeqSetValue(uint8_t partIndex, uint8_t step, uint8_t value) {
  if (partIndex >= RTAL_MULTI_PART_COUNT || step >= PROGRAM_SEQ_STEPS) return;
  multiParts[partIndex].program.seqValues[step] = value > 127 ? 127 : value;
}

void multiSeqStepNow(uint8_t partIndex) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  MultiPart &part = multiParts[partIndex];
  MultiSeqState &st = multiSeq[partIndex];
  uint8_t mode = part.program.param[P_SEQ_MODE];
  if (mode == 0 || !part.enabled || part.mute) {
    st.offset = 0;
    st.absoluteTableActive = false;
    return;
  }

  uint8_t steps = part.program.param[P_SEQ_STEPS];
  if (steps < 1) steps = 1;
  if (steps > MODSEQ_STEPS) steps = MODSEQ_STEPS;

  uint8_t target = part.program.param[P_SEQ_TARGET];
  if (!modSeqTargetIsSafe(target)) target = P_WAVE_POS;
  st.currentTarget = target;

  uint8_t stepIndex = st.index;
  if (stepIndex >= steps) stepIndex = 0;
  st.displayStep = stepIndex;
  const uint8_t stepValue = part.program.seqValues[stepIndex];

  if (target == P_WAVETABLE) {
    uint16_t table = stepValue;
    if (part.program.seqTableMode) table = (uint16_t)part.program.param[P_WAVETABLE] + stepValue;
    if (table >= WT_VISIBLE_SLOTS) table = WT_LAST_SLOT;
    st.absoluteTableValue = (uint8_t)table;
    st.absoluteTableActive = true;
    st.offset = 0;
  } else {
    st.absoluteTableActive = false;
    int32_t centered = (int32_t)stepValue - 64;
    int32_t depth = part.program.param[P_SEQ_DEPTH];
    int32_t offset = (centered * depth) >> 6;
    if (offset < -127) offset = -127;
    if (offset > 127) offset = 127;
    st.offset = (int8_t)offset;
  }

  switch (mode) {
    case 1:
      st.index++;
      if (st.index >= steps) st.index = 0;
      break;
    case 2:
      if (steps <= 1) { st.index = 0; st.dir = 1; }
      else {
        int16_t next = (int16_t)st.index + st.dir;
        if (next >= steps) { st.dir = -1; next = steps - 2; }
        else if (next < 0) { st.dir = 1; next = 1; }
        st.index = (uint8_t)next;
      }
      break;
    case 3:
      st.index = random(steps);
      break;
    default:
      st.index = 0;
      st.offset = 0;
      st.absoluteTableActive = false;
      break;
  }
}

void multiSeqClockTick() {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    MultiSeqState &st = multiSeq[p];
    ++st.midiClockCounter;
    const uint8_t div = multiSeqRateDividerForPart(p);
    if (st.midiClockCounter >= div) {
      st.midiClockCounter = 0;
      multiSeqStepNow(p);
    }
  }
}

void multiSeqStartSync() {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    resetMultiSeqPart(p);
  }
}

void multiSeqStopSync() {
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    multiSeq[p].midiClockCounter = 0;
    multiSeq[p].nextInternalStepUs = 0;
    multiSeq[p].offset = 0;
    multiSeq[p].absoluteTableActive = false;
  }
}

void multiSeqContinueSync() {
  // Keep phase/index; external clock resumes from the next incoming tick.
}

void updateMultiModSequencers() {
  if (!multiModeActive() || getClockSource() == 1) return;
  const uint32_t nowUs = micros();
  for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
    MultiSeqState &st = multiSeq[p];
    if (!multiParts[p].enabled || multiParts[p].mute || multiSeqParam(p, P_SEQ_MODE) == 0) {
      st.nextInternalStepUs = 0;
      st.offset = 0;
      st.absoluteTableActive = false;
      continue;
    }
    const uint32_t stepUs = multiSeqStepUsForPart(p);
    if (st.nextInternalStepUs == 0) {
      st.nextInternalStepUs = nowUs + stepUs;
      continue;
    }
    if ((int32_t)(nowUs - st.nextInternalStepUs) >= 0) {
      const uint32_t lateUs = nowUs - st.nextInternalStepUs;
      multiSeqStepNow(p);
      if (lateUs > stepUs * 2UL) st.nextInternalStepUs = nowUs + stepUs;
      else st.nextInternalStepUs += stepUs;
    }
  }
}

void applyMultiSeqToProgram(uint8_t partIndex, Program &effective) {
  if (partIndex >= RTAL_MULTI_PART_COUNT) return;
  const MultiSeqState &st = multiSeq[partIndex];
  if (effective.param[P_SEQ_MODE] == 0) return;
  uint8_t target = st.currentTarget;
  if (!modSeqTargetIsSafe(target) || target >= PARAM_COUNT) return;

  if (target == P_WAVETABLE && st.absoluteTableActive) {
    effective.param[target] = st.absoluteTableValue;
    return;
  }
  int16_t v = (int16_t)effective.param[target] + (int16_t)st.offset;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  effective.param[target] = (uint8_t)v;
}
