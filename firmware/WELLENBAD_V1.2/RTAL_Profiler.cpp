#include "RTAL_Profiler.h"

RTALProfiler DSPProfiler;

#if RTAL_PROFILER_ENABLED
void RTALProfiler::begin(uint32_t blockBudgetUs) {
  budgetUs = blockBudgetUs;
  lastPrintMs = millis();
  resetWindow();
}

double RTALProfiler::percent(uint64_t usedUs, uint64_t referenceUs) {
  return referenceUs ? (100.0 * (double)usedUs / (double)referenceUs) : 0.0;
}

void RTALProfiler::endBlock(uint8_t activeVoices) {
  const uint32_t elapsed = micros() - blockStartUs;

  portENTER_CRITICAL(&mux);
  dspTotalUs += elapsed;
  blocks++;
  if (elapsed > dspMaxUs) dspMaxUs = elapsed;
  if (activeVoices > peakVoices) peakVoices = activeVoices;

  // An overrun means the DSP computation exceeded one complete audio-block budget.
  if (budgetUs && elapsed > budgetUs) {
    overruns++;
    // Number of real-time deadlines crossed by this render operation.
    missedBlocks += (elapsed - 1U) / budgetUs;
  }

  // Audio sub-sections are sampled to minimize profiler overhead.
  if (profileThisBlock) {
    Counter &voice = section[SECTION_VOICE];
    voice.totalUs += sampledVoiceUs;
    voice.calls++;
    if (sampledVoiceUs > voice.maxUs) voice.maxUs = sampledVoiceUs;

    Counter &fx = section[SECTION_FX];
    fx.totalUs += sampledFxUs;
    fx.calls++;
    if (sampledFxUs > fx.maxUs) fx.maxUs = sampledFxUs;
  }
  portEXIT_CRITICAL(&mux);
}

void RTALProfiler::addTaskTime(Section which, uint32_t elapsedUs) {
  if (which >= SECTION_COUNT) return;
  portENTER_CRITICAL(&mux);
  Counter &c = section[which];
  c.totalUs += elapsedUs;
  c.calls++;
  if (elapsedUs > c.maxUs) c.maxUs = elapsedUs;
  portEXIT_CRITICAL(&mux);
}

void RTALProfiler::noteShortWrite() {
  // A short DMA write means that a complete rendered block was not handed to I2S.
  portENTER_CRITICAL(&mux);
  missedBlocks++;
  portEXIT_CRITICAL(&mux);
}

void RTALProfiler::printIfDue(uint32_t intervalMs) {
  const uint32_t now = millis();
  if ((uint32_t)(now - lastPrintMs) < intervalMs) return;
  lastPrintMs = now;
  print();
}

void RTALProfiler::print() {
  uint64_t snapDspTotalUs;
  uint64_t snapBlocks;
  uint32_t snapDspMaxUs;
  uint32_t snapOverruns;
  uint32_t snapMissedBlocks;
  uint8_t snapPeakVoices;
  Counter snapSection[SECTION_COUNT];

  // Take and clear one coherent five-second window. The critical section is
  // intentionally short and contains no Serial output.
  portENTER_CRITICAL(&mux);
  snapDspTotalUs = dspTotalUs;
  snapBlocks = blocks;
  snapDspMaxUs = dspMaxUs;
  snapOverruns = overruns;
  snapMissedBlocks = missedBlocks;
  snapPeakVoices = peakVoices;
  for (uint8_t i = 0; i < SECTION_COUNT; i++) snapSection[i] = section[i];

  dspTotalUs = 0;
  blocks = 0;
  dspMaxUs = 0;
  overruns = 0;
  missedBlocks = 0;
  peakVoices = 0;
  for (uint8_t i = 0; i < SECTION_COUNT; i++) section[i] = Counter();
  portEXIT_CRITICAL(&mux);

  const uint64_t avgUs = snapBlocks ? snapDspTotalUs / snapBlocks : 0;
  const uint64_t audioWindowUs = snapBlocks * (uint64_t)budgetUs;
  const uint64_t sampledAudioReferenceUs =
      snapSection[SECTION_VOICE].calls * (uint64_t)budgetUs;

  Serial.println(F("=== RTAL PROFILER ==="));
  Serial.printf("DSP %%       : %6.2f\n", percent(snapDspTotalUs, audioWindowUs));
  Serial.printf("Voice %%     : %6.2f\n", percent(snapSection[SECTION_VOICE].totalUs,
                                                   sampledAudioReferenceUs));
  Serial.printf("MIDI %%      : %6.2f\n", percent(snapSection[SECTION_MIDI].totalUs, audioWindowUs));
  Serial.printf("UI %%        : %6.2f\n", percent(snapSection[SECTION_UI].totalUs, audioWindowUs));
  Serial.printf("SD %%        : %6.2f\n", percent(snapSection[SECTION_SD].totalUs, audioWindowUs));
  Serial.printf("FX %%        : %6.2f\n", percent(snapSection[SECTION_FX].totalUs,
                                                   sampledAudioReferenceUs));

  // "ISR" is retained as the familiar RTAL display name. On this ESP32 build it
  // measures the real-time audio service section before the blocking I2S write,
  // not Espressif's internal I2S interrupt handler.
  Serial.printf("Average ISR  : %6llu us\n", avgUs);
  Serial.printf("Max ISR      : %6lu us\n", snapDspMaxUs);
  Serial.printf("Peak ISR     : %6.2f %%\n", budgetUs ? 100.0 * (double)snapDspMaxUs / budgetUs : 0.0);
  Serial.printf("Peak Voices  : %6u\n", snapPeakVoices);
  Serial.printf("Missed Blocks: %6lu\n", snapMissedBlocks);
  Serial.printf("Overruns     : %6lu\n", snapOverruns);
  Serial.printf("Blocks       : %6llu\n", snapBlocks);
}

void RTALProfiler::resetWindow() {
  portENTER_CRITICAL(&mux);
  dspTotalUs = 0;
  blocks = 0;
  dspMaxUs = 0;
  overruns = 0;
  missedBlocks = 0;
  peakVoices = 0;
  sampledVoiceUs = 0;
  sampledFxUs = 0;
  for (uint8_t i = 0; i < SECTION_COUNT; i++) section[i] = Counter();
  portEXIT_CRITICAL(&mux);
}

#endif
