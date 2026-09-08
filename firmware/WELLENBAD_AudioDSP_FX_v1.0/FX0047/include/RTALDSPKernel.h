#pragma once
#include <Arduino.h>
#include "RTALStatus.h"
#include "RTALOutputLimiter.h"

struct RTALDSPKernelParameters
{
    bool fxBypass;
    bool hardBypass;
    float dry;
    float wet;
};


struct RTALDSPPeakEvent
{
    uint32_t blockNumber;
    uint32_t totalUs;
    uint32_t preUs;
    uint32_t delayUs;
    uint32_t modUs;
    uint32_t reverbUs;
    uint32_t postUs;
    uint32_t otherUs;
    uint32_t delaySmoothUs;
    uint32_t delayReadUs;
    uint32_t delayFeedbackUs;
    uint32_t delayDuckUs;
    uint32_t delayWriteUs;
    uint32_t delayStatsUs;
    uint32_t lfoPhase;
    uint8_t modFx;
    bool reverbActive;
};

struct RTALDSPKernelStatistics
{
    uint64_t processedFrames;
    uint32_t processedBlocks;
    uint64_t rmsSamples;

    uint32_t limiterEventsLeft;
    uint32_t limiterEventsRight;
    uint32_t hardClipsLeft;
    uint32_t hardClipsRight;

    float inputPeakLeft;
    float inputPeakRight;
    float outputPeakLeft;
    float outputPeakRight;

    double inputDcLeft;
    double inputDcRight;
    double outputDcLeft;
    double outputDcRight;

    double inputSquareLeft;
    double inputSquareRight;
    double outputSquareLeft;
    double outputSquareRight;

    uint64_t accumulatedBlockUs;
    uint32_t maximumBlockUs;
    uint32_t minimumBlockUs;
    uint32_t deadlineMisses;
    uint32_t maximumOverrunUs;

    // Build0046f peak histogram and compact recent-event ring (5 s window).
    uint32_t peakGt2600;
    uint32_t peakGt2700;
    uint32_t peakGt2800;
    uint32_t peakGtBudget;
    uint32_t peakGt3000;
    uint32_t peakGt3200;
    uint32_t peakEventCount;
    uint32_t peakEventTotal;
    RTALDSPPeakEvent peakEvents[12];

    // Complete kernel time classified by selected MOD FX:
    // 0=OFF, 1=CHORUS, 2=FLANGER, 3=PHASER.
    uint32_t modFxBlocks[4];
    uint64_t modFxAccumulatedBlockUs[4];
    uint32_t modFxMaximumBlockUs[4];
    uint32_t reverbActiveBlocks;

    // Frame-0 sampled stage cost in CPU cycles. No per-sample micros() calls.
    uint64_t sampledDelayCycles;
    uint32_t sampledDelayCount;
    uint64_t sampledModCycles[4];
    uint32_t sampledModCount[4];
    uint64_t sampledReverbCycles;
    uint32_t sampledReverbCount;

    float limiterGainLeft;
    float limiterGainRight;
};

class RTALDSPKernel
{
public:
    static RTALStatus begin();
    static void processBlock(int32_t* interleavedStereo32, size_t frames);

    static void setFxBypass(bool enabled);
    static void setHardBypass(bool enabled);
    static void setDry(float value);
    static void setWet(float value);

    static RTALDSPKernelParameters parameters();
    static RTALDSPKernelStatistics statistics(bool resetWindow);
    static void printReport();

private:
    static float clamp01(float value);
    static int16_t floatToInt16(float sample, uint32_t& clipCounter);
    static float wetLeft(float left, float right);
    static float wetRight(float left, float right);
    static void snapshotTargetParameters();
    static void resetStatisticsUnlocked();
    static float processDcLeft(float input);
    static float processDcRight(float input);
    static bool nearUnity(float value);
    static bool nearZero(float value);

    static portMUX_TYPE mux_;
    static RTALDSPKernelParameters target_;
    static RTALDSPKernelParameters blockTarget_;
    static RTALDSPKernelParameters current_;
    static RTALDSPKernelStatistics statistics_;

    static float dcPreviousInputLeft_;
    static float dcPreviousInputRight_;
    static float dcPreviousOutputLeft_;
    static float dcPreviousOutputRight_;

    static RTALOutputLimiter limiterLeft_;
    static RTALOutputLimiter limiterRight_;
};
