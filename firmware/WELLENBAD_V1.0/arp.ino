// ================================================================
// Arpeggiator
// ================================================================
void arpAddNote(uint8_t note, uint8_t vel) {
  for (int i = 0; i < arpCount; i++) if (arpNotes[i] == note) return;
  if (arpCount >= ARP_MAX_NOTES) return;
  int pos = arpCount;
  while (pos > 0 && arpNotes[pos - 1] > note) {
    arpNotes[pos] = arpNotes[pos - 1]; arpVel[pos] = arpVel[pos - 1]; pos--;
  }
  arpNotes[pos] = note; arpVel[pos] = vel; arpCount++;
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
void updateArp() {
  if (getAParam(P_ARP_MODE) == 0) return;
  if (getClockSource() == 1) return;

  uint32_t now = millis();
  uint32_t stepMs = arpStepMs();
  if (now - arpLastStep < stepMs) return;

  // Stable internal clock accumulator.
  // Advance by the intended interval, not by "now", so small task delays do not
  // become tempo jitter.
  arpLastStep += stepMs;

  // If the control task was blocked for longer, resync cleanly instead of
  // firing a burst of delayed arp steps.
  if (now - arpLastStep > stepMs * 2UL) {
    arpLastStep = now;
  }

  arpStepNow();
}
