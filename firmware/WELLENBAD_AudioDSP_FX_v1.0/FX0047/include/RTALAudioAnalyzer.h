#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALChannelAnalysis
{
    int32_t minimum;
    int32_t maximum;
    int32_t peakAbsolute;

    int64_t sum;
    long double sumSquares;
    uint64_t sampleCount;

    uint32_t nonZeroSamples;
    uint32_t positiveSamples;
    uint32_t negativeSamples;
    uint32_t zeroSamples;

    uint32_t changedLow8;
    uint32_t changedLow16;
    uint32_t changedLow24;
    uint32_t changedHigh24;

    uint32_t commonZeroLsbs;
    uint32_t commonSignExtensionMsbs;
    uint32_t usedMagnitudeBits;
};

struct RTALAudioAnalysisSnapshot
{
    RTALChannelAnalysis left;
    RTALChannelAnalysis right;

    int32_t firstLeft;
    int32_t firstRight;
    int32_t lastLeft;
    int32_t lastRight;

    uint64_t stereoFrames;
    uint32_t identicalFrames;
    uint32_t differentFrames;
    uint32_t leftOnlyFrames;
    uint32_t rightOnlyFrames;

    int32_t maximumChannelDifference;
};

class RTALAudioAnalyzer
{
public:
    static RTALStatus begin();
    static void processInterleavedStereo32(const int32_t* samples, size_t frames);
    static RTALAudioAnalysisSnapshot snapshot(bool resetWindow);
    static void printReport();

private:
    static void resetUnlocked();
    static void updateChannel(RTALChannelAnalysis& channel, int32_t sample);
    static uint32_t countTrailingZeroBits(uint32_t value);
    static uint32_t magnitudeBits(int32_t value);
    static uint32_t signExtensionBits(int32_t value);

    static portMUX_TYPE mux_;
    static RTALAudioAnalysisSnapshot data_;
};
