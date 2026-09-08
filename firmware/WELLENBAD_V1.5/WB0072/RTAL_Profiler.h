#ifndef RTAL_PROFILER_H
#define RTAL_PROFILER_H

#include <Arduino.h>

// Master switch. Define this before including RTAL_Profiler.h.
// 0 = profiler is compiled out completely.
// 1 = profiler is active.
#ifndef RTAL_PROFILER_ENABLED
#define RTAL_PROFILER_ENABLED 0
#endif

// Audio sub-sections are sampled every RTAL_PROFILE_SAMPLE_DIV blocks to keep
// the measurement overhead well below the work being measured.
#ifndef RTAL_PROFILE_SAMPLE_DIV
#define RTAL_PROFILE_SAMPLE_DIV 16U
#endif

class RTALProfiler {
public:
  enum Section : uint8_t {
    SECTION_VOICE = 0,
    SECTION_MIDI,
    SECTION_UI,
    SECTION_SD,
    SECTION_FX,
    SECTION_OSC,
    SECTION_FILTER,
    SECTION_ENVAMP,
    SECTION_PAN,
    SECTION_MIX,
    SECTION_COUNT
  };

#if RTAL_PROFILER_ENABLED
  void begin(uint32_t blockBudgetUs);

  inline void beginBlock() {
    blockStartUs = micros();
    profileThisBlock = ((blocks & (RTAL_PROFILE_SAMPLE_DIV - 1U)) == 0U);
    sampledVoiceUs = 0;
    sampledFxUs = 0;
    sampledOscUs = 0;
    sampledFilterUs = 0;
    sampledEnvAmpUs = 0;
    sampledPanUs = 0;
    sampledMixUs = 0;
  }

  inline bool sampleAudioSections() const { return profileThisBlock; }
  inline uint32_t beginSection() const { return micros(); }

  inline void endAudioSection(Section section, uint32_t startUs) {
    if (!profileThisBlock) return;
    const uint32_t elapsed = micros() - startUs;
    switch (section) {
      case SECTION_VOICE: sampledVoiceUs += elapsed; break;
      case SECTION_FX: sampledFxUs += elapsed; break;
      case SECTION_OSC: sampledOscUs += elapsed; break;
      case SECTION_FILTER: sampledFilterUs += elapsed; break;
      case SECTION_ENVAMP: sampledEnvAmpUs += elapsed; break;
      case SECTION_PAN: sampledPanUs += elapsed; break;
      case SECTION_MIX: sampledMixUs += elapsed; break;
      default: break;
    }
  }

  void endBlock(uint8_t activeVoices);
  void addTaskTime(Section section, uint32_t elapsedUs);
  void noteShortWrite();
  void print();
  void printIfDue(uint32_t intervalMs = 5000UL);
  void resetWindow();

private:
  struct Counter {
    uint64_t totalUs = 0;
    uint32_t calls = 0;
    uint32_t maxUs = 0;
  };

  uint32_t budgetUs = 0;
  uint32_t blockStartUs = 0;
  uint32_t lastPrintMs = 0;
  bool profileThisBlock = false;

  uint32_t sampledVoiceUs = 0;
  uint32_t sampledFxUs = 0;
  uint32_t sampledOscUs = 0;
  uint32_t sampledFilterUs = 0;
  uint32_t sampledEnvAmpUs = 0;
  uint32_t sampledPanUs = 0;
  uint32_t sampledMixUs = 0;

  uint64_t dspTotalUs = 0;
  uint64_t blocks = 0;
  uint32_t dspMaxUs = 0;
  uint32_t overruns = 0;
  uint32_t missedBlocks = 0;
  uint8_t peakVoices = 0;

  Counter section[SECTION_COUNT];
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  static double percent(uint64_t usedUs, uint64_t referenceUs);
#else
  // Compile-time no-op implementation. All calls are inline and are removed by
  // the optimizer, including micros(), counters and critical sections.
  inline void begin(uint32_t) {}
  inline void beginBlock() {}
  inline bool sampleAudioSections() const { return false; }
  inline uint32_t beginSection() const { return 0; }
  inline void endAudioSection(Section, uint32_t) {}
  inline void endBlock(uint8_t) {}
  inline void addTaskTime(Section, uint32_t) {}
  inline void noteShortWrite() {}
  inline void print() {}
  inline void printIfDue(uint32_t = 5000UL) {}
  inline void resetWindow() {}
#endif
};

extern RTALProfiler DSPProfiler;

#if RTAL_PROFILER_ENABLED
  #define RTAL_PROFILE_TASK_BEGIN(name) const uint32_t name = micros()
  #define RTAL_PROFILE_TASK_END(sectionName, name) \
    DSPProfiler.addTaskTime(sectionName, micros() - (name))
#else
  #define RTAL_PROFILE_TASK_BEGIN(name) do { } while (0)
  #define RTAL_PROFILE_TASK_END(sectionName, name) do { } while (0)
#endif

#endif
