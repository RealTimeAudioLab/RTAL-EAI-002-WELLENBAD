#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALStereoPhaserParameters {
    bool enabled;
    float mix;
    uint8_t stages;       // 2/4 (final HQ modes)
    float feedback;       // bipolar -0.95 .. +0.95
    float feedbackHpfHz;
    float saturation;
    float centerHz;       // tonal center of the allpass bank
};

struct RTALStereoPhaserStatistics {
    uint64_t processedFrames;
    uint32_t processedBlocks;
    float peakWetLeft;
    float peakWetRight;
};

class RTALStereoPhaser {
public:
    static RTALStatus begin(uint32_t sampleRate);
    static void reset();
    static void beginBlock(size_t frames);
    static void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight);
    static void setEnabled(bool v);
    static void setMix(float v);
    static void setStages(uint8_t v);
    static void setFeedback(float v);
    static void setFeedbackHpfHz(float v);
    static void setSaturation(float v);
    static void setCenterHz(float v);
    static RTALStereoPhaserParameters parameters();
    static RTALStereoPhaserStatistics statistics(bool resetWindow);
    static void printReport();
private:
    static float clamp(float v,float lo,float hi);
    static float softSaturate(float x,float amount);
    static float processAllpass(float x,float a,float& z);
    static void flushLocalStatistics();
    static portMUX_TYPE mux_;
    static RTALStereoPhaserParameters target_, blockTarget_, current_;
    static RTALStereoPhaserStatistics statistics_;
    static uint32_t sampleRate_;
    static float mixStep_, feedbackStep_, saturationStep_, centerStep_;
    static float mixNorm_, mixNormStep_;
    static float hpfCoeff_;
    static float hpfInLeft_, hpfInRight_, hpfOutLeft_, hpfOutRight_;
    static float stateLeft_[4], stateRight_[4];
    static float coeffLeft_[4], coeffRight_[4];
    static float coeffStepLeft_[4], coeffStepRight_[4];
    static uint8_t coeffPhase_;
    static constexpr uint8_t kCoeffControlDiv = 4;
    static uint64_t localProcessedFrames_;
    static float localPeakWetLeft_, localPeakWetRight_;
};
