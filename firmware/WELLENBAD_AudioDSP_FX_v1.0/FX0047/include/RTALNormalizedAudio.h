#pragma once
#include <Arduino.h>
#include "RTALStatus.h"
#include "RTALDCBlocker.h"

struct RTALNormalizedStatistics
{
    uint64_t processedFrames;
    int16_t inputPeakLeft;
    int16_t inputPeakRight;
    int16_t outputPeakLeft;
    int16_t outputPeakRight;
    int64_t inputSumLeft;
    int64_t inputSumRight;
    int64_t outputSumLeft;
    int64_t outputSumRight;
};

class RTALNormalizedAudio
{
public:
    static RTALStatus begin();
    static void processBlock(int32_t* interleavedStereo32, size_t frames);
    static RTALNormalizedStatistics statistics(bool resetWindow);
    static void printReport();

private:
    static void resetUnlocked();
    static int16_t absolute16(int16_t value);

    static portMUX_TYPE mux_;
    static RTALNormalizedStatistics statistics_;
    static RTALDCBlocker dcLeft_;
    static RTALDCBlocker dcRight_;
};
