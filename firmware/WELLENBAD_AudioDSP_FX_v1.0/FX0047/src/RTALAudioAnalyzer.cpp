#include "../include/RTALAudioAnalyzer.h"
#include "../include/RTALLogger.h"
#include "../include/RTALConfig.h"
#include <cstring>
#include <cmath>
#include <limits.h>

portMUX_TYPE RTALAudioAnalyzer::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALAudioAnalysisSnapshot RTALAudioAnalyzer::data_ = {};

RTALStatus RTALAudioAnalyzer::begin()
{
    portENTER_CRITICAL(&mux_);
    resetUnlocked();
    portEXIT_CRITICAL(&mux_);
    return RTALStatus::OK;
}

void RTALAudioAnalyzer::resetUnlocked()
{
    memset(&data_, 0, sizeof(data_));
    data_.left.minimum = INT32_MAX;
    data_.right.minimum = INT32_MAX;
    data_.left.maximum = INT32_MIN;
    data_.right.maximum = INT32_MIN;
    data_.left.commonZeroLsbs = 32;
    data_.right.commonZeroLsbs = 32;
    data_.left.commonSignExtensionMsbs = 32;
    data_.right.commonSignExtensionMsbs = 32;
}

uint32_t RTALAudioAnalyzer::countTrailingZeroBits(uint32_t value)
{
    if (value == 0) return 32;
    return static_cast<uint32_t>(__builtin_ctz(value));
}

uint32_t RTALAudioAnalyzer::magnitudeBits(int32_t value)
{
    uint32_t magnitude;
    if (value == INT32_MIN) magnitude = 0x80000000UL;
    else magnitude = static_cast<uint32_t>(value < 0 ? -value : value);

    if (magnitude == 0) return 0;
    return 32U - static_cast<uint32_t>(__builtin_clz(magnitude));
}

uint32_t RTALAudioAnalyzer::signExtensionBits(int32_t value)
{
    // Number of identical sign bits at the MSB side.
    uint32_t bits = static_cast<uint32_t>(value);
    if (value >= 0)
    {
        if (bits == 0) return 32;
        return static_cast<uint32_t>(__builtin_clz(bits));
    }

    uint32_t inverted = ~bits;
    if (inverted == 0) return 32;
    return static_cast<uint32_t>(__builtin_clz(inverted));
}

void RTALAudioAnalyzer::updateChannel(
    RTALChannelAnalysis& channel,
    int32_t sample)
{
    if (sample < channel.minimum) channel.minimum = sample;
    if (sample > channel.maximum) channel.maximum = sample;

    int64_t absolute = sample;
    if (absolute < 0) absolute = -absolute;
    if (absolute > channel.peakAbsolute)
        channel.peakAbsolute = static_cast<int32_t>(absolute);

    channel.sum += sample;
    channel.sumSquares +=
        static_cast<long double>(sample) * static_cast<long double>(sample);
    ++channel.sampleCount;

    if (sample == 0) ++channel.zeroSamples;
    else
    {
        ++channel.nonZeroSamples;
        if (sample > 0) ++channel.positiveSamples;
        else ++channel.negativeSamples;
    }

    const uint32_t raw = static_cast<uint32_t>(sample);

    if ((raw & RTAL_ANALYSIS_LOW_8_MASK) != 0) ++channel.changedLow8;
    if ((raw & RTAL_ANALYSIS_LOW_16_MASK) != 0) ++channel.changedLow16;
    if ((raw & RTAL_ANALYSIS_LOW_24_MASK) != 0) ++channel.changedLow24;
    if ((raw & RTAL_ANALYSIS_HIGH_24_MASK) != 0) ++channel.changedHigh24;

    if (sample != 0)
    {
        const uint32_t trailing = countTrailingZeroBits(raw);
        if (trailing < channel.commonZeroLsbs)
            channel.commonZeroLsbs = trailing;

        const uint32_t signBits = signExtensionBits(sample);
        if (signBits < channel.commonSignExtensionMsbs)
            channel.commonSignExtensionMsbs = signBits;

        const uint32_t used = magnitudeBits(sample);
        if (used > channel.usedMagnitudeBits)
            channel.usedMagnitudeBits = used;
    }
}

void RTALAudioAnalyzer::processInterleavedStereo32(
    const int32_t* samples,
    size_t frames)
{
    if (!samples || frames == 0) return;

    portENTER_CRITICAL(&mux_);

    for (size_t frame = 0; frame < frames; ++frame)
    {
        const int32_t left = samples[frame * 2];
        const int32_t right = samples[frame * 2 + 1];

        if (data_.stereoFrames == 0)
        {
            data_.firstLeft = left;
            data_.firstRight = right;
        }

        data_.lastLeft = left;
        data_.lastRight = right;

        updateChannel(data_.left, left);
        updateChannel(data_.right, right);

        ++data_.stereoFrames;

        if (left == right) ++data_.identicalFrames;
        else ++data_.differentFrames;

        if (left != 0 && right == 0) ++data_.leftOnlyFrames;
        if (right != 0 && left == 0) ++data_.rightOnlyFrames;

        int64_t difference =
            static_cast<int64_t>(left) - static_cast<int64_t>(right);
        if (difference < 0) difference = -difference;
        if (difference > data_.maximumChannelDifference)
            data_.maximumChannelDifference =
                difference > INT32_MAX ? INT32_MAX :
                static_cast<int32_t>(difference);
    }

    portEXIT_CRITICAL(&mux_);
}

RTALAudioAnalysisSnapshot RTALAudioAnalyzer::snapshot(bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    RTALAudioAnalysisSnapshot copy = data_;
    if (resetWindow) resetUnlocked();
    portEXIT_CRITICAL(&mux_);
    return copy;
}

static double calculateMean(const RTALChannelAnalysis& c)
{
    return c.sampleCount
        ? static_cast<double>(c.sum) / static_cast<double>(c.sampleCount)
        : 0.0;
}

static double calculateRms(const RTALChannelAnalysis& c)
{
    if (!c.sampleCount) return 0.0;
    long double meanSquare =
        c.sumSquares / static_cast<long double>(c.sampleCount);
    return sqrt(static_cast<double>(meanSquare));
}

static double percent(uint64_t part, uint64_t whole)
{
    return whole ? (100.0 * static_cast<double>(part) /
                    static_cast<double>(whole)) : 0.0;
}

void RTALAudioAnalyzer::printReport()
{
    const RTALAudioAnalysisSnapshot s = snapshot(true);

    if (s.stereoFrames == 0)
    {
        RTALLogger::info("Format frames=0 analysis=IDLE");
        return;
    }

    const double meanL = calculateMean(s.left);
    const double meanR = calculateMean(s.right);
    const double rmsL = calculateRms(s.left);
    const double rmsR = calculateRms(s.right);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Format frames=%llu L[min=%ld max=%ld peak=%ld dc=%.1f rms=%.1f] R[min=%ld max=%ld peak=%ld dc=%.1f rms=%.1f]",
        static_cast<unsigned long long>(s.stereoFrames),
        static_cast<long>(s.left.minimum),
        static_cast<long>(s.left.maximum),
        static_cast<long>(s.left.peakAbsolute),
        meanL,
        rmsL,
        static_cast<long>(s.right.minimum),
        static_cast<long>(s.right.maximum),
        static_cast<long>(s.right.peakAbsolute),
        meanR,
        rmsR);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Channel identical=%lu(%.1f%%) different=%lu L_only=%lu R_only=%lu max_diff=%ld",
        static_cast<unsigned long>(s.identicalFrames),
        percent(s.identicalFrames, s.stereoFrames),
        static_cast<unsigned long>(s.differentFrames),
        static_cast<unsigned long>(s.leftOnlyFrames),
        static_cast<unsigned long>(s.rightOnlyFrames),
        static_cast<long>(s.maximumChannelDifference));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Bits L[used=%lu zero_lsb=%lu sign_msb=%lu low8=%lu low16=%lu low24=%lu high24=%lu]",
        static_cast<unsigned long>(s.left.usedMagnitudeBits),
        static_cast<unsigned long>(s.left.commonZeroLsbs),
        static_cast<unsigned long>(s.left.commonSignExtensionMsbs),
        static_cast<unsigned long>(s.left.changedLow8),
        static_cast<unsigned long>(s.left.changedLow16),
        static_cast<unsigned long>(s.left.changedLow24),
        static_cast<unsigned long>(s.left.changedHigh24));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Bits R[used=%lu zero_lsb=%lu sign_msb=%lu low8=%lu low16=%lu low24=%lu high24=%lu] first=%ld/%ld last=%ld/%ld",
        static_cast<unsigned long>(s.right.usedMagnitudeBits),
        static_cast<unsigned long>(s.right.commonZeroLsbs),
        static_cast<unsigned long>(s.right.commonSignExtensionMsbs),
        static_cast<unsigned long>(s.right.changedLow8),
        static_cast<unsigned long>(s.right.changedLow16),
        static_cast<unsigned long>(s.right.changedLow24),
        static_cast<unsigned long>(s.right.changedHigh24),
        static_cast<long>(s.firstLeft),
        static_cast<long>(s.firstRight),
        static_cast<long>(s.lastLeft),
        static_cast<long>(s.lastRight));
}
