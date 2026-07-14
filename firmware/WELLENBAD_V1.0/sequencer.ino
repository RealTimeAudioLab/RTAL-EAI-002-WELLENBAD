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
  modSeqDir = 1;
  modSeqLastStep = millis();
}


void setSeqTableModeGlobal(uint8_t v, bool store) {
  modSeqTableMode = v ? 1 : 0;
  if (store) prefs.putUChar("seqTblMode", modSeqTableMode);
}
