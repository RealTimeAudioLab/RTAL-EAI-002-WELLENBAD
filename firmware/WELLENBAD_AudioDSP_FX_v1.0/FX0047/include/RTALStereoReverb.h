#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALStereoReverbParameters {
    bool enabled;
    float mix;          // 0..1
    float size;         // 0..1
    float decay;        // 0..1
    float dampingHz;    // 1200..16000
    float predelayMs;   // 0..200
};

struct RTALStereoReverbStatistics {
    uint64_t processedFrames;
    uint32_t processedBlocks;
    float peakWetLeft;
    float peakWetRight;
};

class RTALStereoReverb {
public:
    static RTALStatus begin(uint32_t sampleRate);
    static void reset();
    static void setEnabled(bool v);
    static void setMix(float v);
    static void setSize(float v);
    static void setDecay(float v);
    static void setDampingHz(float v);
    static void setPredelayMs(float v);
    static RTALStereoReverbParameters parameters();
    static void beginBlock(size_t frames);
    static bool activeForBlock();
    static void process(float inL,float inR,float& outL,float& outR);
    static RTALStereoReverbStatistics statistics(bool resetWindow=false);
    static void printReport();
private:
    static float clamp(float v,float lo,float hi);
    static float* line_[4];
    static float* preL_;
    static float* preR_;
    static uint32_t fdnWrite_;
    static uint32_t preWrite_;
    static uint32_t preSamples_;
    static uint32_t predelayReadOffset_;
    static uint32_t sampleRate_;
    static uint32_t delaySamples_[4];
    static float dampState_[4];
    static float dampCoeff_;
    static float feedbackGain_;
    static float currentMix_,mixStep_;
    static float mixNorm_,mixNormStep_;
    static float currentSize_;
    static float currentDecay_;
    static float currentPredelayMs_;
    static float currentDampingHz_;
    static bool blockEnabled_;
    static portMUX_TYPE mux_;
    static RTALStereoReverbParameters target_, blockTarget_;
    static RTALStereoReverbStatistics statistics_;
    static uint64_t localFrames_;
    static uint16_t localBlocks_;
    static float localPeakL_,localPeakR_;
    static uint8_t statsDecim_;
    static void updateDerived();
    static void flushStats();
};
