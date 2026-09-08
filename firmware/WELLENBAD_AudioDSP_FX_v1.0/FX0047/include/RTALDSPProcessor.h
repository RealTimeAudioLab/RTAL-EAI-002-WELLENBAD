#pragma once
#include <Arduino.h>
#include "RTALStatus.h"
#include "RTALDSPTypes.h"
#include "RTALOutputLimiter.h"

class RTALDSPProcessor
{
public:
    static RTALStatus begin();
    static void processBlock(int32_t* interleavedStereo32, size_t frames);
    static void setBypass(bool bypass);
    static void setDry(float dry);
    static void setWet(float wet);
    static RTALDSPParameters parameters();
    static RTALDSPStatistics statistics(bool resetWindow);
    static void printReport();
private:
    static float clamp01(float value);
    static int16_t floatToInt16(float sample, uint32_t& clipCounter);
    static void updateSmoothedParameters();
    static portMUX_TYPE mux_;
    static RTALDSPParameters target_;
    static RTALDSPParameters current_;
    static RTALDSPStatistics statistics_;
    static RTALOutputLimiter limiterLeft_;
    static RTALOutputLimiter limiterRight_;
};
