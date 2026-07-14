// ================================================================
// Audio FX
// ================================================================
int32_t dc_x1_l = 0, dc_y1_l = 0, dc_x1_r = 0, dc_y1_r = 0;
uint16_t chorusWrite = 0;
uint16_t chorusPhase = 0;

static inline int16_t dcBlockL(int16_t x) {
  int32_t y = x - dc_x1_l + ((dc_y1_l * 32600) >> 15);
  dc_x1_l = x; dc_y1_l = y;
  return constrain(y, -32768, 32767);
}
static inline int16_t dcBlockR(int16_t x) {
  int32_t y = x - dc_x1_r + ((dc_y1_r * 32600) >> 15);
  dc_x1_r = x; dc_y1_r = y;
  return constrain(y, -32768, 32767);
}

static inline void processChorus(int16_t &l, int16_t &r) {
  uint8_t amount = getAParamSeq(P_CHORUS);
  if (amount == 0) {
    chorusBufL[chorusWrite] = l;
    chorusBufR[chorusWrite] = r;
    chorusWrite = (chorusWrite + 1) & CHORUS_MASK;
    return;
  }

  // Chorus V2: three lightweight triangle-LFO taps.
  // No sin/cos in the audio loop; only integer delays and feedback-free mix.
  chorusPhase += 13;
  int16_t tri1 = triangleLfo(chorusPhase);
  int16_t tri2 = triangleLfo(chorusPhase + 21845);
  int16_t tri3 = triangleLfo(chorusPhase + 43690);

  uint16_t d1 = 16 + (((tri1 + 32768) * 18) >> 16);
  uint16_t d2 = 29 + (((tri2 + 32768) * 24) >> 16);
  uint16_t d3 = 43 + (((tri3 + 32768) * 28) >> 16);

  int32_t dl = chorusBufL[(chorusWrite - d1) & CHORUS_MASK];
  dl += chorusBufR[(chorusWrite - d2 - 7) & CHORUS_MASK];
  dl += chorusBufL[(chorusWrite - d3 - 13) & CHORUS_MASK];

  int32_t dr = chorusBufR[(chorusWrite - d1 - 11) & CHORUS_MASK];
  dr += chorusBufL[(chorusWrite - d2 - 19) & CHORUS_MASK];
  dr += chorusBufR[(chorusWrite - d3 - 3) & CHORUS_MASK];

  chorusBufL[chorusWrite] = l;
  chorusBufR[chorusWrite] = r;
  chorusWrite = (chorusWrite + 1) & CHORUS_MASK;

  l = constrain((int32_t)l + ((dl * amount) >> 10), -32768, 32767);
  r = constrain((int32_t)r + ((dr * amount) >> 10), -32768, 32767);
}

// Integer master soft limiter. It keeps normal levels untouched, compresses peaks
// above about -2 dBFS and counts hard emergency clips for debugging.
static inline int16_t masterLimiter(int32_t x) {
  const int32_t knee = 26000;
  const int32_t ceiling = 32760;
  if (x > knee) {
    limiterHits++;
    x = knee + ((x - knee) >> 2);
  } else if (x < -knee) {
    limiterHits++;
    x = -knee + ((x + knee) >> 2);
  }
  if (x > ceiling) { hardClipHits++; x = ceiling; }
  if (x < -ceiling) { hardClipHits++; x = -ceiling; }
  return (int16_t)x;
}
