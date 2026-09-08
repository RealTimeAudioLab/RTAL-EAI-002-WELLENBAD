#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALStereoFlangerParameters {
    bool enabled;
    float mix;
    float baseDelayMs;
    float feedback;       // bipolar -0.95 .. +0.95
    float feedbackHpfHz;  // DC/bass cleanup in feedback path
    float saturation;     // 0..1, inexpensive cubic soft saturation
};

struct RTALStereoFlangerStatistics {
    uint64_t processedFrames;
    uint32_t processedBlocks;
    float peakWetLeft;
    float peakWetRight;
    size_t allocatedBytes;
    bool usingPsram;
};

class RTALStereoFlanger {
public:
    static RTALStatus begin(uint32_t sampleRate);
    static void reset();
    static void beginBlock(size_t frames);
    static void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight);

    static void setEnabled(bool v);
    static void setMix(float v);
    static void setBaseDelayMs(float v);
    static void setFeedback(float v);
    static void setFeedbackHpfHz(float v);
    static void setSaturation(float v);
    static RTALStereoFlangerParameters parameters();
    static RTALStereoFlangerStatistics statistics(bool resetWindow);
    static void printReport();

private:
    static float clamp(float v,float lo,float hi);
    static float readHermite(const float* buffer,uint32_t writeIndex,float delaySamples);
    static float softSaturate(float x,float amount);
    static void flushLocalStatistics();

    static portMUX_TYPE mux_;
    static RTALStereoFlangerParameters target_, blockTarget_, current_;
    static RTALStereoFlangerStatistics statistics_;
    static float* bufferLeft_;
    static float* bufferRight_;
    static uint32_t bufferSamples_, writeIndex_, sampleRate_;
    static float sampleRatePerMs_;
    static float mixStep_, delayStep_, feedbackStep_, saturationStep_;
    static float mixNorm_, mixNormStep_;
    static float hpfCoeff_;
    static float hpfInLeft_, hpfInRight_, hpfOutLeft_, hpfOutRight_;
    static uint64_t localProcessedFrames_;
    static float localPeakWetLeft_, localPeakWetRight_;
};
