#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALStereoChorusParameters {
    bool enabled;
    float mix;
    float baseDelayMs;
    float toneHz;
};

struct RTALStereoChorusStatistics {
    uint64_t processedFrames;
    uint32_t processedBlocks;
    float peakWetLeft;
    float peakWetRight;
    size_t allocatedBytes;
    bool usingPsram;
};

class RTALStereoChorus {
public:
    static RTALStatus begin(uint32_t sampleRate);
    static void reset();
    static void beginBlock(size_t frames);
    static void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight);

    static void setEnabled(bool v);
    static void setMix(float v);
    static void setBaseDelayMs(float v);
    static void setToneHz(float v);
    static RTALStereoChorusParameters parameters();
    static RTALStereoChorusStatistics statistics(bool resetWindow);
    static void printReport();

private:
    static float clamp(float v,float lo,float hi);
    static float readHermite(const float* buffer,uint32_t writeIndex,float delaySamples);
    static float readLinear(const float* buffer,uint32_t writeIndex,float delaySamples);
    static float toneCoefficient(float hz);
    static void updateSmoothedParameters();
    static void flushLocalStatistics();

    static portMUX_TYPE mux_;
    static RTALStereoChorusParameters target_, blockTarget_, current_;
    static RTALStereoChorusStatistics statistics_;
    static float* bufferLeft_;
    static float* bufferRight_;
    static uint32_t bufferSamples_, writeIndex_, sampleRate_;
    static float toneStateLeft_, toneStateRight_, toneCoeff_;
    static float sampleRatePerMs_;
    static float mixStep_, delayStep_;
    static float mixNorm_, mixNormStep_;
    static uint64_t localProcessedFrames_;
    static float localPeakWetLeft_, localPeakWetRight_;
    static uint16_t localProcessedBlocks_;
    static uint8_t statsDecim_;
    static uint8_t controlDecim_;
};
