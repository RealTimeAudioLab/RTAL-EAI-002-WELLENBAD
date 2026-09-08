#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

enum class RTALAudioClockState : uint8_t
{
    Waiting = 0,
    Present,
    Lost
};


struct RTALActivePeakEvent
{
    uint32_t audioBlock;
    uint32_t activeUs;
    uint32_t processingUs;
    uint32_t preUs;
    uint32_t txUs;
    uint32_t otherUs;
};

struct RTALAudioStatistics
{
    uint32_t audioBlocks;
    uint32_t failedAudioBlocks;
    uint32_t probeFullBlocks;
    uint32_t probePartialReads;
    uint32_t rejectedTimingBlocks;
    uint32_t clockPresentTransitions;
    uint32_t clockLostTransitions;
    uint32_t lastProcessingUs;
    uint32_t maximumProcessingUs;
    uint64_t accumulatedProcessingUs;

    // Build0046d/0046e realtime deadline diagnostics.
    uint32_t processingDeadlineMisses;
    uint32_t maximumProcessingOverrunUs;
    uint32_t activeDeadlineMisses;
    uint32_t lastActiveUs;
    uint32_t maximumActiveUs;
    uint64_t accumulatedActiveUs;


    // Build0046f complete-audio-path peak forensics (windowed on report).
    uint32_t activePeakGt2600;
    uint32_t activePeakGt2700;
    uint32_t activePeakGt2800;
    uint32_t activePeakGtBudget;
    uint32_t activePeakGt3000;
    uint32_t activePeakGt3200;
    uint32_t activePeakEventCount;
    uint32_t activePeakEventTotal;
    RTALActivePeakEvent activePeakEvents[12];

    // Raw RX data is inspected immediately after a complete i2s_read(), before
    // analyzer, DC blocker or DSP processing.
    uint32_t rawFreezeEvents;
    uint32_t rawFreezeCurrentFrames;
    uint32_t rawFreezeMaximumFrames;
    uint64_t rawIdenticalFrames;
    int32_t rawFreezeLeft;
    int32_t rawFreezeRight;

    uint32_t rawRepeatedBlocks;
    uint32_t rawRepeatEvents;
    uint32_t rawRepeatCurrentBlocks;
    uint32_t rawRepeatMaximumBlocks;

    int32_t peakAbsolute;
    RTALAudioClockState clockState;
};

class RTALAudioEngine
{
public:
    static RTALStatus begin();
    static RTALAudioStatistics statistics();
    static uint32_t blockBudgetUs();
    static const char* clockStateName(RTALAudioClockState state);

private:
    static void audioTask(void* parameter);
    static void updatePeak(const int32_t* samples, size_t sampleCount);
    static void analyzeRawInput(const int32_t* samples, size_t frames);
    static void setClockState(RTALAudioClockState state);
    static bool intervalIsPlausible(uint32_t intervalUs);

    static TaskHandle_t taskHandle_;
    static int8_t registryId_;
    static portMUX_TYPE statisticsMux_;
    static RTALAudioStatistics statistics_;
};
