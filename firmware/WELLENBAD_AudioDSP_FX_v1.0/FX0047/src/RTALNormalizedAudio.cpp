#include "../include/RTALNormalizedAudio.h"
#include "../include/RTALAudioFormat.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include <string.h>
#include <limits.h>

portMUX_TYPE RTALNormalizedAudio::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALNormalizedStatistics RTALNormalizedAudio::statistics_ = {};
RTALDCBlocker RTALNormalizedAudio::dcLeft_;
RTALDCBlocker RTALNormalizedAudio::dcRight_;

RTALStatus RTALNormalizedAudio::begin()
{
    dcLeft_.reset();
    dcRight_.reset();
    portENTER_CRITICAL(&mux_);
    resetUnlocked();
    portEXIT_CRITICAL(&mux_);
    return RTALStatus::OK;
}

void RTALNormalizedAudio::resetUnlocked()
{
    memset(&statistics_, 0, sizeof(statistics_));
}

int16_t RTALNormalizedAudio::absolute16(int16_t value)
{
    if (value == INT16_MIN) return INT16_MAX;
    return value < 0 ? static_cast<int16_t>(-value) : value;
}

void RTALNormalizedAudio::processBlock(int32_t* samples, size_t frames)
{
    if (!samples || frames == 0) return;

    RTALNormalizedStatistics local{};

    for (size_t frame = 0; frame < frames; ++frame)
    {
        const size_t li = frame * 2;
        const size_t ri = li + 1;

        const int16_t inL = RTALAudioFormat::containerToInt16(samples[li]);
        const int16_t inR = RTALAudioFormat::containerToInt16(samples[ri]);

        int16_t outL = inL;
        int16_t outR = inR;

        if (RTAL_DC_BLOCKER_ENABLED)
        {
            outL = dcLeft_.process(inL);
            outR = dcRight_.process(inR);
        }

        samples[li] = RTALAudioFormat::int16ToContainer(outL);
        samples[ri] = RTALAudioFormat::int16ToContainer(outR);

        const int16_t inAbsL = absolute16(inL);
        const int16_t inAbsR = absolute16(inR);
        const int16_t outAbsL = absolute16(outL);
        const int16_t outAbsR = absolute16(outR);

        if (inAbsL > local.inputPeakLeft) local.inputPeakLeft = inAbsL;
        if (inAbsR > local.inputPeakRight) local.inputPeakRight = inAbsR;
        if (outAbsL > local.outputPeakLeft) local.outputPeakLeft = outAbsL;
        if (outAbsR > local.outputPeakRight) local.outputPeakRight = outAbsR;

        local.inputSumLeft += inL;
        local.inputSumRight += inR;
        local.outputSumLeft += outL;
        local.outputSumRight += outR;
        ++local.processedFrames;
    }

    portENTER_CRITICAL(&mux_);
    statistics_.processedFrames += local.processedFrames;
    statistics_.inputSumLeft += local.inputSumLeft;
    statistics_.inputSumRight += local.inputSumRight;
    statistics_.outputSumLeft += local.outputSumLeft;
    statistics_.outputSumRight += local.outputSumRight;

    if (local.inputPeakLeft > statistics_.inputPeakLeft)
        statistics_.inputPeakLeft = local.inputPeakLeft;
    if (local.inputPeakRight > statistics_.inputPeakRight)
        statistics_.inputPeakRight = local.inputPeakRight;
    if (local.outputPeakLeft > statistics_.outputPeakLeft)
        statistics_.outputPeakLeft = local.outputPeakLeft;
    if (local.outputPeakRight > statistics_.outputPeakRight)
        statistics_.outputPeakRight = local.outputPeakRight;
    portEXIT_CRITICAL(&mux_);
}

RTALNormalizedStatistics RTALNormalizedAudio::statistics(bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    const RTALNormalizedStatistics copy = statistics_;
    if (resetWindow) resetUnlocked();
    portEXIT_CRITICAL(&mux_);
    return copy;
}

void RTALNormalizedAudio::printReport()
{
    const RTALNormalizedStatistics s = statistics(true);

    if (s.processedFrames == 0)
    {
        RTALLogger::info("Normalize frames=0 state=IDLE");
        return;
    }

    const double inDcL = static_cast<double>(s.inputSumLeft) /
                         static_cast<double>(s.processedFrames);
    const double inDcR = static_cast<double>(s.inputSumRight) /
                         static_cast<double>(s.processedFrames);
    const double outDcL = static_cast<double>(s.outputSumLeft) /
                          static_cast<double>(s.processedFrames);
    const double outDcR = static_cast<double>(s.outputSumRight) /
                          static_cast<double>(s.processedFrames);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Normalize frames=%llu input_peak=%d/%d output_peak=%d/%d",
        static_cast<unsigned long long>(s.processedFrames),
        static_cast<int>(s.inputPeakLeft),
        static_cast<int>(s.inputPeakRight),
        static_cast<int>(s.outputPeakLeft),
        static_cast<int>(s.outputPeakRight));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "DC input=%.2f/%.2f output=%.2f/%.2f",
        inDcL, inDcR, outDcL, outDcR);
}
