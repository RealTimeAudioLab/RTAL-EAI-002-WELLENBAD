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
    DSPProfiler.beginBlock();
    uint32_t blockStart = micros();

    processAudioEvents();
    updateStuckVoiceGuard();

    // A001: Active voice count is stable for the duration of this block.
    uint8_t activeCount = 0;
    for (int v = 0; v < NUM_VOICES; v++) {
      if (voices[v].active) activeCount++;
    }
    audioPolyLoad = activeCount;
    activeVoicesLast = activeCount;

    // A005b: values that cannot change meaningfully inside one 128-sample
    // block are evaluated once instead of 128 times.
#if RTAL_DYNAMIC_HEADROOM
    const int16_t blockHeadroomScale = voiceHeadroomScale(activeCount);
#endif
    const bool blockFastPoly = (RTAL_ENABLE_FAST_POLY && activeCount >= RTAL_POLY_FAST_THRESHOLD);
    const bool blockProgramMuted = (millis() < programChangeMuteUntil);
#if RTAL_DISABLE_CHORUS_HIGH_POLY
    const bool blockChorusAllowed = (activeCount < RTAL_POLY_FAST_THRESHOLD);
#else
    const bool blockChorusAllowed = true;
#endif

    for (int i = 0; i < AUDIO_BLOCK; i++) {
      updateSmoothParams();

      // A002: Read the oscillator parameter snapshot only once per sample,
      // then share it with every active voice. Previously every voice created
      // its own identical snapshot in oscRead().
      AudioSnapshot audioSnapshot;
      readAudioSnapshot(audioSnapshot);
      const uint32_t sampleRuntimeGeneration = updateVoiceRuntimeGeneration(audioSnapshot);

      int32_t mixL = 0;
      int32_t mixR = 0;

      uint32_t voiceProfileStart = 0;
      if (DSPProfiler.sampleAudioSections()) voiceProfileStart = DSPProfiler.beginSection();
      for (int v = 0; v < NUM_VOICES; v++) {
        // Avoid stereo/pan work for inactive voices.
        if (!voices[v].active) continue;

        StereoSample s = renderVoiceStereo(voices[v], audioSnapshot, blockFastPoly, sampleRuntimeGeneration);
        mixL += s.l;
        mixR += s.r;
      }
      if (DSPProfiler.sampleAudioSections()) {
        DSPProfiler.endAudioSection(RTALProfiler::SECTION_VOICE, voiceProfileStart);
      }

#if RTAL_DYNAMIC_HEADROOM
      mixL = (mixL * blockHeadroomScale) >> 15;
      mixR = (mixR * blockHeadroomScale) >> 15;
#else
      mixL = (mixL * 3) >> 2;
      mixR = (mixR * 3) >> 2;
#endif

      // Global output trim: -6 dB
      mixL >>= 1;
      mixR >>= 1;

      int16_t outL = dcBlockL(softLimiterPreDac(mixL));
      int16_t outR = dcBlockR(softLimiterPreDac(mixR));

      if (blockProgramMuted) {
        outL = 0;
        outR = 0;
      } else {
        uint32_t fxProfileStart = 0;
        if (DSPProfiler.sampleAudioSections()) fxProfileStart = DSPProfiler.beginSection();
        if (blockChorusAllowed) {
          processChorus(outL, outR, audioSnapshot.chorus);
        }
        if (DSPProfiler.sampleAudioSections()) {
          DSPProfiler.endAudioSection(RTALProfiler::SECTION_FX, fxProfileStart);
        }
        outL = softLimiterPreDac(outL);
        outR = softLimiterPreDac(outR);
      }

      buffer[i * 2 + 0] = ((int32_t)outL) << 16;
      buffer[i * 2 + 1] = ((int32_t)outR) << 16;
    }

    size_t written = 0;
    // Finish profiling before the blocking DMA write. This measures the actual
    // DSP service time and not the time spent waiting for an I2S buffer.
    DSPProfiler.endBlock(activeCount);

    i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &written, portMAX_DELAY);
    if (written != sizeof(buffer)) {
      i2sShortWrites++;
      DSPProfiler.noteShortWrite();
    }

    uint32_t elapsed = micros() - blockStart;
    audioLastBlockMicros = elapsed;
    if (elapsed > audioMaxBlockMicros) audioMaxBlockMicros = elapsed;
    audioBlocksRendered++;
  }
}
