#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALStereoDelayParameters
{
    bool enabled;
    bool freeze;
    bool pingPong; // compatibility mirror
    float crossfeed;
    float duckAmount;
    float duckThresholdDb;
    float duckReleaseMs;
    float timeLeftMs;
    float timeRightMs;
    float feedback;
    float level;
    float feedbackLowpassHz;
    float feedbackHighpassHz;
    float feedbackSaturation;
};

struct RTALStereoDelayForensics
{
    uint32_t sampledFrames;
    uint32_t totalFrames;
    uint64_t smoothCycles;
    uint64_t readCycles;
    uint64_t feedbackCycles;
    uint64_t duckCycles;
    uint64_t writeCycles;
    uint64_t statsCycles;
};

struct RTALStereoDelayStatistics
{
    uint64_t processedFrames;
    uint32_t processedBlocks;
    float peakWetLeft;
    float peakWetRight;
    float peakFeedbackLeft;
    float peakFeedbackRight;
    size_t allocatedBytes;
    bool usingPsram;
};

class RTALStereoDelay
{
public:
    static RTALStatus begin(uint32_t sampleRate);
    static void reset();

    static void setEnabled(bool enabled);
    static void setFreeze(bool enabled);
    static void setPingPong(bool enabled);
    static void setCrossfeed(float value);
    static void setDuckAmount(float value);
    static void setDuckThresholdDb(float value);
    static void setDuckReleaseMs(float value);
    static void setTimeLeftMs(float value);
    static void setTimeRightMs(float value);
    static void transitionSyncTimes(float leftMs, float rightMs, float crossfadeMs);
    static void setFeedback(float value);
    static void setLevel(float value);
    static void setFeedbackLowpassHz(float value);
    static void setFeedbackHighpassHz(float value);
    static void setFeedbackSaturation(float value);

    static RTALStereoDelayParameters parameters();
    static void beginBlock(size_t frames);
    static void endBlock();
    static void process(float inputLeft, float inputRight,
                        float& wetLeft, float& wetRight);

    static RTALStereoDelayStatistics statistics(bool resetWindow);
    static RTALStereoDelayForensics forensics();
    static RTALStereoDelayForensics forensicWindow(bool resetWindow);
    static void printReport();

private:
    static float clamp(float value, float lo, float hi);
    static float readInterpolated(const float* buffer,
                                  uint32_t writeIndex,
                                  float delaySamples);
    static float lowpassCoefficient(float cutoffHz);
    static float highpassCoefficient(float cutoffHz);
    static float dbToLinear(float db);
    static float saturateFeedback(float value, float amount);
    static void updateSmoothedParameters();
    static void updateFilterCoefficientsIfDue();

    static portMUX_TYPE mux_;
    static RTALStereoDelayParameters target_;
    static RTALStereoDelayParameters blockTarget_;
    static RTALStereoDelayParameters current_;
    static RTALStereoDelayStatistics statistics_;
    static RTALStereoDelayForensics forensics_;
    static RTALStereoDelayForensics forensicWindow_;
    static uint32_t forensicFrameIndex_;

    static float* bufferLeft_;
    static float* bufferRight_;
    static uint32_t bufferSamples_;
    static uint32_t writeIndex_;
    static uint32_t sampleRate_;

    static float feedbackFilterStateLeft_;
    static float feedbackFilterStateRight_;
    static float feedbackLowpassCoefficient_;
    static float feedbackHighpassInputLeft_;
    static float feedbackHighpassInputRight_;
    static float feedbackHighpassStateLeft_;
    static float feedbackHighpassStateRight_;
    static float feedbackHighpassCoefficient_;
    static uint32_t coefficientUpdateCountdown_;
    static float coefficientLowpassHz_;
    static float coefficientHighpassHz_;
    static float duckEnvelope_;
    static float duckGain_;
    static float duckThresholdLinear_;
    static float duckReleaseCoefficient_;
    static float freezeMix_;

    // Build0046l: click-free sync-time transition. Two fixed read taps are
    // crossfaded; FREE/manual delay-time smoothing remains unchanged.
    static bool syncTransitionActive_;
    static float syncOldLeftMs_;
    static float syncOldRightMs_;
    static float syncNewLeftMs_;
    static float syncNewRightMs_;
    static float syncTransitionMix_;
    static float syncTransitionStep_;
};
