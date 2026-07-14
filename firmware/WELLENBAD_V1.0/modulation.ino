static inline int16_t lfoShapeTriangle(uint16_t phase) {
  return triangleLfo(phase);
}

static inline int16_t lfoShapeSine(uint16_t phase) {
  int16_t tri = triangleLfo(phase);
  int32_t x = tri;
  int32_t ax = x < 0 ? -x : x;
  int32_t y = (x * (65536L - ax)) >> 15;
  return (int16_t)constrain(y, -32768, 32767);
}

static inline int16_t lfoShapeSawUp(uint16_t phase) {
  return (int16_t)((int32_t)phase - 32768L);
}

static inline int16_t lfoShapeSawDown(uint16_t phase) {
  return (int16_t)(32767L - (int32_t)phase);
}

static inline int16_t lfoShapeSquare(uint16_t phase) {
  return (phase < 32768) ? 32767 : -32768;
}

void setLfoShapeGlobal(uint8_t v, bool store) {
  if (v > 5) v = 5;
  globalLfoShape = v;
  currentProgram.lfoShape = v;
  // LFO Shape is now stored per program (Program V2).
  (void)store;
}
