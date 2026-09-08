#pragma once
#include <Arduino.h>

struct RTALDSPParameters
{
    bool bypass;
    float dry;
    float wet;
};

struct RTALDSPStatistics
{
    uint64_t processedFrames;
    uint32_t limiterEventsLeft;
    uint32_t limiterEventsRight;
    uint32_t hardClipsLeft;
    uint32_t hardClipsRight;
    float inputPeakLeft;
    float inputPeakRight;
    float outputPeakLeft;
    float outputPeakRight;
    float limiterGainLeft;
    float limiterGainRight;
};
