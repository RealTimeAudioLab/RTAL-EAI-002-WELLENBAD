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
// B0063F NOTE REL FIX1: recover from a lost/stale SINGLE NoteOff.
// MidiTask updates arpHeldInputNotes before it queues AE_NOTE_OFF, so this
// list is an independent authoritative view of physically held MIDI keys.
// Only ARP-OFF SINGLE voices are reconciled; MULTI/ARP output and normal
// release tails are deliberately untouched. Cost is negligible (once/block).
static inline void reconcileSingleHeldNotes() {
  if (multiModeActive()) return;
  if (getAParam(P_ARP_MODE) != 0) return;
  if (sustainPedal) return;

  for (int i = 0; i < NUM_VOICES; ++i) {
    Voice &v = voices[i];
    if (!v.active || v.partIndex != RTAL_MULTI_PART_NONE) continue;
    if (v.ampStage == ENV_RELEASE || v.ampStage == ENV_OFF) continue;
    if (!arpInputNoteHeld(v.note)) {
      envNoteOff(v);
      v.sustained = false;
    }
  }
}

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

static constexpr int16_t RTAL_MULTI_FIXED_HEADROOM_Q15 = 9400;  // FIX4: fixed 8-voice-safe headroom

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
    reconcileSingleHeldNotes();
    updateStuckVoiceGuard();

    // A001: Active voice count is stable for the duration of this block.
    uint8_t activeCount = 0;
    for (int v = 0; v < NUM_VOICES; v++) {
      if (voices[v].active) activeCount++;
    }
    audioPolyLoad = activeCount;
    activeVoicesLast = activeCount;

    // WB0068 CDC AUDIO QUIET1: while ANY voice is sounding, the remote link is
    // strictly RX-only. Do not allow USB CDC TX, mirror copies or resync bursts
    // to compete with the audio engine. Release the guard only after all voices
    // have been inactive for 500 ms. Editor commands remain live on Core 0.
    static uint32_t remoteAudioQuietSinceMs = 0;
    if (activeCount > 0U) {
      remoteAudioQuietSinceMs = 0;
      RTALRemote::setRealtimeGuard(true);
    } else {
      const uint32_t rtNow = millis();
      if (remoteAudioQuietSinceMs == 0U) remoteAudioQuietSinceMs = rtNow;
      RTALRemote::setRealtimeGuard((uint32_t)(rtNow - remoteAudioQuietSinceMs) < 500U);
    }

    // v1.3.03 FIX2: determine operating mode before selecting the block gain.
    const bool blockMultiMode = multiModeActive();

    // A005b: values that cannot change meaningfully inside one 128-sample
    // block are evaluated once instead of 128 times.
#if RTAL_DYNAMIC_HEADROOM
    // FIX4: MULTI mode uses one fixed eight-voice-safe gain.
    // This prevents a newly added part from lowering already sounding parts.
    const int16_t blockHeadroomScale = blockMultiMode
        ? RTAL_MULTI_FIXED_HEADROOM_Q15
        : voiceHeadroomScale(activeCount);
#endif
    const bool blockFastPoly = (RTAL_ENABLE_FAST_POLY && activeCount >= RTAL_POLY_FAST_THRESHOLD);
    const bool blockProgramMuted = (millis() < programChangeMuteUntil);
#if RTAL_INTERNAL_CHORUS
  #if RTAL_DISABLE_CHORUS_HIGH_POLY
    const bool blockChorusAllowed = (activeCount < RTAL_POLY_FAST_THRESHOLD);
  #else
    const bool blockChorusAllowed = true;
  #endif
#else
    const bool blockChorusAllowed = false;
#endif

    // v1.3.03 FIX1: Per-part programs are static during an audio block and
    // currently use no sample-by-sample smoothing. Build their four snapshots
    // once per 128-sample block, not four times for every sample. The previous
    // implementation performed 512 snapshot builds plus repeated voice scans
    // per block and exhausted the sixth-voice timing margin.
    AudioSnapshot multiSnapshots[RTAL_MULTI_PART_COUNT];
    uint32_t multiRuntimeGenerations[RTAL_MULTI_PART_COUNT] = {0, 0, 0, 0};
    uint8_t blockGlobalChorusAmount = 0;
    bool partActive[RTAL_MULTI_PART_COUNT] = {false, false, false, false};

    if (blockMultiMode) {
      for (uint8_t v = 0; v < NUM_VOICES; ++v) {
        if (voices[v].active && voices[v].partIndex < RTAL_MULTI_PART_COUNT) {
          partActive[voices[v].partIndex] = true;
        }
      }
      for (uint8_t p = 0; p < RTAL_MULTI_PART_COUNT; ++p) {
        // WB0065 RT RESERVE1: an inactive part cannot be rendered anywhere in
        // this block. Do not copy its Program, run its sequencer projection,
        // build ~40 snapshot fields or update its runtime generation. A newly
        // queued note is already materialized by processAudioEvents() above,
        // therefore partActive[] is valid for the current block.
        if (!partActive[p]) continue;

        // WB0069 MULTI RT RESERVE2: avoid copying the complete Program when
        // the per-part modulation sequencer is OFF. This is the common case
        // and removes one block-rate Program memcpy/stack write per active part.
        // Only build a temporary Program when the sequencer can actually alter
        // a parameter. The resulting AudioSnapshot is bit-identical otherwise.
        if (multiParts[p].program.param[P_SEQ_MODE] == 0) {
          readAudioSnapshotFromProgram(multiParts[p].program, multiParts[p], multiSnapshots[p]);
        } else {
          Program effectiveProgram = multiParts[p].program;
          applyMultiSeqToProgram(p, effectiveProgram);
          readAudioSnapshotFromProgram(effectiveProgram, multiParts[p], multiSnapshots[p]);
        }
        multiRuntimeGenerations[p] = updateMultiVoiceRuntimeGeneration(p, multiSnapshots[p]);
#if RTAL_INTERNAL_CHORUS
        if (multiSnapshots[p].chorus > blockGlobalChorusAmount) {
          blockGlobalChorusAmount = multiSnapshots[p].chorus;
        }
#endif
      }
    }

    // WB0063 OPT3: SINGLE-mode parameter snapshot is control-rate, not sample-rate.
    // The old path rebuilt ~40 parameters and derived filter/controller terms
    // for every sample (44.1k snapshots/s), even though audible controls are
    // already smoothed. Rebuild once every four samples instead. To preserve
    // the original smoothing time constants, execute four smoother steps at
    // each snapshot tick. Maximum parameter hold time is 3 samples (~68 us).
    AudioSnapshot audioSnapshot;
    uint32_t sampleRuntimeGeneration = 0;
    uint8_t singleGlobalChorusAmount = blockGlobalChorusAmount;

    for (int i = 0; i < AUDIO_BLOCK; i++) {
      uint8_t globalChorusAmount = blockGlobalChorusAmount;
      if (!blockMultiMode) {
        if ((i & 3) == 0) {
          updateSmoothParams();
          updateSmoothParams();
          updateSmoothParams();
          updateSmoothParams();
          readAudioSnapshot(audioSnapshot);
          sampleRuntimeGeneration = updateVoiceRuntimeGeneration(audioSnapshot);
          singleGlobalChorusAmount = audioSnapshot.chorus;
        }
        globalChorusAmount = singleGlobalChorusAmount;
      }

      int32_t mixL = 0;
      int32_t mixR = 0;

      uint32_t voiceProfileStart = 0;
      if (DSPProfiler.sampleAudioSections()) voiceProfileStart = DSPProfiler.beginSection();
      for (int v = 0; v < NUM_VOICES; v++) {
        // Avoid stereo/pan work for inactive voices.
        if (!voices[v].active) continue;

        const AudioSnapshot *voiceSnapshot = &audioSnapshot;
        uint32_t voiceGeneration = sampleRuntimeGeneration;
        if (blockMultiMode && voices[v].partIndex < RTAL_MULTI_PART_COUNT) {
          voiceSnapshot = &multiSnapshots[voices[v].partIndex];
          voiceGeneration = multiRuntimeGenerations[voices[v].partIndex];
        }
        StereoSample s = renderVoiceStereo(voices[v], *voiceSnapshot, blockFastPoly, voiceGeneration);
        mixL += s.l;
        mixR += s.r;
      }
      if (DSPProfiler.sampleAudioSections()) {
        DSPProfiler.endAudioSection(RTALProfiler::SECTION_VOICE, voiceProfileStart);
      }

      uint32_t mixProfileStart = 0;
      if (DSPProfiler.sampleAudioSections()) mixProfileStart = DSPProfiler.beginSection();
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
#if RTAL_INTERNAL_CHORUS
        if (blockChorusAllowed) {
          processChorus(outL, outR, globalChorusAmount);
        }
#else
        (void)blockChorusAllowed;
        (void)globalChorusAmount;
#endif
        if (DSPProfiler.sampleAudioSections()) {
          DSPProfiler.endAudioSection(RTALProfiler::SECTION_FX, fxProfileStart);
        }
        outL = softLimiterPreDac(outL);
        outR = softLimiterPreDac(outR);
      }

      buffer[i * 2 + 0] = ((int32_t)outL) << 16;
      buffer[i * 2 + 1] = ((int32_t)outR) << 16;
      if (DSPProfiler.sampleAudioSections()) DSPProfiler.endAudioSection(RTALProfiler::SECTION_MIX, mixProfileStart);
    }

    size_t written = 0;
    // Finish profiling before the blocking DMA write. This measures the actual
    // DSP service time and not the time spent waiting for an I2S buffer.
    const bool perfSampledBlock = DSPProfiler.sampleAudioSections();
    DSPProfiler.endBlock(activeCount);

    // Detailed profiler blocks contain extra instrumentation when explicitly enabled.
    // Keep the always-on UI deadline statistics clean by excluding those sparse
    // profiler blocks (1/64). Serial profiler output still reports them.

    // v1.3.13: independent always-on DSP deadline monitor.  This intentionally
    // excludes the blocking I2S DMA write and costs only one micros() call per
    // 128-sample block.
    const uint32_t dspElapsed = micros() - blockStart;
    audioDspLastMicros = dspElapsed;
    const uint32_t dspBudgetUs = (uint32_t)(((uint64_t)AUDIO_BLOCK * 1000000ULL) / SAMPLE_RATE);
    if (!perfSampledBlock) {
      if (dspElapsed > audioDspMaxMicros) audioDspMaxMicros = dspElapsed;
      if (dspElapsed > dspBudgetUs) audioDspOverruns++;
    }

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
