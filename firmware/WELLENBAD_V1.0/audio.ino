// ================================================================
// I2S and audio task
// ================================================================
void processAudioEvents();

void setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = AUDIO_BLOCK,
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRCK,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

// safety guard: a voice that remains active for an extreme duration is
// gently released. This is deliberately conservative so sustained pads are not
// cut during normal playing, but it prevents live-session stuck notes.
static inline void updateStuckVoiceGuard() {
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck < 500) return;
  lastCheck = now;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].active && voices[i].noteOnMillis != 0 && (now - voices[i].noteOnMillis) > 300000UL) {
      envNoteOff(voices[i]);
      voices[i].sustained = false;
      stuckVoiceResets++;
    }
  }
}

static inline int16_t voiceHeadroomScale(uint8_t voices) {
  // Q15 scale, deliberately conservative from 5 voices upward.
  // This fixes the common "5th note distorts" symptom caused by summed voices clipping.
  static const int16_t scale[9] = {32767, 30000, 22000, 18000, 15000, 13000, 11500, 10300, 9400};
  if (voices > 8) voices = 8;
  return scale[voices];
}

static inline int16_t softLimiterPreDac(int32_t x) {
  // Very gentle integer soft limiter before the DAC.
  // Transparent below full scale, progressively compresses peaks above it.
  const int32_t limit = 32767;
  if (x > 131068) x = 131068;
  if (x < -131068) x = -131068;

  int32_t ax = x >= 0 ? x : -x;
  if (ax <= limit) return (int16_t)x;

  int32_t over = ax - limit;
  int32_t compressed = limit + ((over * limit) / (limit + over));
  if (compressed > limit) compressed = limit;

  return (int16_t)(x < 0 ? -compressed : compressed);
}


void audioTask(void *param) {
  int32_t buffer[AUDIO_BLOCK * 2];
  while (true) {
    uint32_t blockStart = micros();
    processAudioEvents();
    updateStuckVoiceGuard();
    for (int i = 0; i < AUDIO_BLOCK; i++) {
      updateSmoothParams();

      // Count active voices before rendering. This value is used by the oscillator
      // to enter the cheaper high-poly path and by the output scaler for headroom.
      uint8_t activeCount = 0;
      for (int v = 0; v < NUM_VOICES; v++) {
        if (voices[v].active) activeCount++;
      }
      audioPolyLoad = activeCount;

      int32_t mixL = 0, mixR = 0;
      for (int v = 0; v < NUM_VOICES; v++) {
        StereoSample s = renderVoiceStereo(voices[v]);
        mixL += s.l;
        mixR += s.r;
      }
      activeVoicesLast = activeCount;

#if PPG_DYNAMIC_HEADROOM
      int16_t scale = voiceHeadroomScale(activeCount);
      mixL = (mixL * scale) >> 15;
      mixR = (mixR * scale) >> 15;
#else
      mixL = (mixL * 3) >> 2;
      mixR = (mixR * 3) >> 2;
#endif
      // Global output trim: -6 dB
      mixL >>= 1;
      mixR >>= 1;
      int16_t outL = dcBlockL(softLimiterPreDac(mixL));
      int16_t outR = dcBlockR(softLimiterPreDac(mixR));
      if (millis() < programChangeMuteUntil) {
        outL = 0;
        outR = 0;
      } else {
#if PPG_DISABLE_CHORUS_HIGH_POLY
        if (audioPolyLoad < PPG_POLY_FAST_THRESHOLD) {
          processChorus(outL, outR);
        }
#else
        processChorus(outL, outR);
#endif
        outL = softLimiterPreDac(outL);
        outR = softLimiterPreDac(outR);
      }
      buffer[i * 2 + 0] = ((int32_t)outL) << 16;
      buffer[i * 2 + 1] = ((int32_t)outR) << 16;
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &written, portMAX_DELAY);
    if (written != sizeof(buffer)) i2sShortWrites++;
    uint32_t elapsed = micros() - blockStart;
    audioLastBlockMicros = elapsed;
    if (elapsed > audioMaxBlockMicros) audioMaxBlockMicros = elapsed;
    audioBlocksRendered++;
  }
}
