#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

#include "../include/RTALDSPKernel.h"
#include "../include/RTALAudioFormat.h"
#include "../include/RTALAudioEngine.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALStereoDelay.h"
#include "../include/RTALModCore.h"
#include "../include/RTALStereoChorus.h"
#include "../include/RTALStereoFlanger.h"
#include "../include/RTALStereoPhaser.h"
#include "../include/RTALStereoWidth.h"
#include "../include/RTALStereoReverb.h"
#include <math.h>
#include <string.h>
#include <limits.h>
#include <Esp.h>

portMUX_TYPE RTALDSPKernel::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALDSPKernelParameters RTALDSPKernel::target_ = {};
RTALDSPKernelParameters RTALDSPKernel::blockTarget_ = {};
RTALDSPKernelParameters RTALDSPKernel::current_ = {};
RTALDSPKernelStatistics RTALDSPKernel::statistics_ = {};

float RTALDSPKernel::dcPreviousInputLeft_ = 0.0f;
float RTALDSPKernel::dcPreviousInputRight_ = 0.0f;
float RTALDSPKernel::dcPreviousOutputLeft_ = 0.0f;
float RTALDSPKernel::dcPreviousOutputRight_ = 0.0f;

RTALOutputLimiter RTALDSPKernel::limiterLeft_;
RTALOutputLimiter RTALDSPKernel::limiterRight_;

RTALStatus RTALDSPKernel::begin()
{
    target_.fxBypass = RTAL_FX_BYPASS_DEFAULT;
    target_.hardBypass = RTAL_HARD_BYPASS_DEFAULT;
    target_.dry = RTAL_DSP_DRY_DEFAULT;
    target_.wet = RTAL_DSP_WET_DEFAULT;

    blockTarget_ = target_;
    current_ = target_;

    dcPreviousInputLeft_ = 0.0f;
    dcPreviousInputRight_ = 0.0f;
    dcPreviousOutputLeft_ = 0.0f;
    dcPreviousOutputRight_ = 0.0f;

    limiterLeft_.reset();
    limiterRight_.reset();

    if (RTALStereoDelay::begin(RTAL_AUDIO_SAMPLE_RATE) != RTALStatus::OK)
        return RTALStatus::FATAL;
    if (RTALModCore::begin(RTAL_AUDIO_SAMPLE_RATE) != RTALStatus::OK)
        return RTALStatus::FATAL;
    if (RTALStereoChorus::begin(RTAL_AUDIO_SAMPLE_RATE) != RTALStatus::OK)
        return RTALStatus::FATAL;
    if (RTALStereoFlanger::begin(RTAL_AUDIO_SAMPLE_RATE) != RTALStatus::OK)
        return RTALStatus::FATAL;
    if (RTALStereoPhaser::begin(RTAL_AUDIO_SAMPLE_RATE) != RTALStatus::OK)
        return RTALStatus::FATAL;
    if (RTALStereoWidth::begin() != RTALStatus::OK)
        return RTALStatus::FATAL;
    if (RTALStereoReverb::begin(RTAL_AUDIO_SAMPLE_RATE) != RTALStatus::OK)
        return RTALStatus::FATAL;

    // Build0045a boot invariant: transparent, low-load state before link sync.
    RTALStereoDelay::setEnabled(false);
    RTALStereoDelay::setFreeze(false);
    RTALStereoChorus::setEnabled(false);
    RTALStereoFlanger::setEnabled(false);
    RTALStereoPhaser::setEnabled(false);
    RTALStereoReverb::setEnabled(false);
    RTALStereoWidth::setEnabled(true);
    RTALStereoWidth::setWidth(1.0f);

    portENTER_CRITICAL(&mux_);
    resetStatisticsUnlocked();
    portEXIT_CRITICAL(&mux_);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "DSPKernel ...... PASS fx_bypass=%s hard_bypass=%s dry=%.3f wet=%.3f profiler=%s rms=%s",
        target_.fxBypass ? "ON" : "OFF",
        target_.hardBypass ? "ON" : "OFF",
        target_.dry,
        target_.wet,
        RTAL_DSP_BLOCK_PROFILER_ENABLED ? "BLOCK" : "OFF",
        RTAL_DSP_RMS_DIAGNOSTICS_ENABLED ? "ON" : "OFF");

    return RTALStatus::OK;
}

float RTALDSPKernel::clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

bool RTALDSPKernel::nearUnity(float value)
{
    return fabsf(value - 1.0f) <= RTAL_DSP_UNITY_EPSILON;
}

bool RTALDSPKernel::nearZero(float value)
{
    return fabsf(value) <= RTAL_DSP_UNITY_EPSILON;
}

void RTALDSPKernel::setFxBypass(bool enabled)
{
    portENTER_CRITICAL(&mux_);
    target_.fxBypass = enabled;
    portEXIT_CRITICAL(&mux_);
}

void RTALDSPKernel::setHardBypass(bool enabled)
{
    portENTER_CRITICAL(&mux_);
    target_.hardBypass = enabled;
    portEXIT_CRITICAL(&mux_);
}

void RTALDSPKernel::setDry(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.dry = clamp01(value);
    portEXIT_CRITICAL(&mux_);
}

void RTALDSPKernel::setWet(float value)
{
    portENTER_CRITICAL(&mux_);
    target_.wet = clamp01(value);
    portEXIT_CRITICAL(&mux_);
}

RTALDSPKernelParameters RTALDSPKernel::parameters()
{
    portENTER_CRITICAL(&mux_);
    const RTALDSPKernelParameters copy = target_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

void RTALDSPKernel::snapshotTargetParameters()
{
    portENTER_CRITICAL(&mux_);
    blockTarget_ = target_;
    portEXIT_CRITICAL(&mux_);
}

float RTALDSPKernel::processDcLeft(float input)
{
    const float output =
        input - dcPreviousInputLeft_ +
        RTAL_DC_BLOCKER_R * dcPreviousOutputLeft_;

    dcPreviousInputLeft_ = input;
    dcPreviousOutputLeft_ = output;
    return output;
}

float RTALDSPKernel::processDcRight(float input)
{
    const float output =
        input - dcPreviousInputRight_ +
        RTAL_DC_BLOCKER_R * dcPreviousOutputRight_;

    dcPreviousInputRight_ = input;
    dcPreviousOutputRight_ = output;
    return output;
}

float RTALDSPKernel::wetLeft(float left, float)
{
    return left;
}

float RTALDSPKernel::wetRight(float, float right)
{
    return right;
}

int16_t RTALDSPKernel::floatToInt16(
    float sample,
    uint32_t& clipCounter)
{
    if (sample > 0.999969f)
    {
        sample = 0.999969f;
        ++clipCounter;
    }
    else if (sample < -1.0f)
    {
        sample = -1.0f;
        ++clipCounter;
    }

    long converted = lrintf(sample * RTAL_DSP_FLOAT_TO_INT16);

    if (converted > 32767)
    {
        converted = 32767;
        ++clipCounter;
    }
    else if (converted < -32768)
    {
        converted = -32768;
        ++clipCounter;
    }

    return static_cast<int16_t>(converted);
}

void RTALDSPKernel::processBlock(
    int32_t* samples,
    size_t frames)
{
    if (!samples || frames == 0) return;

    const uint32_t blockStartedUs =
        RTAL_DSP_BLOCK_PROFILER_ENABLED ? micros() : 0;

    snapshotTargetParameters();

    // Build0043: one shared ModCore feeds exactly one active MOD FX.
    // Build0045a: determine exactly one MOD FX once per block. OFF is a true bypass.
    // Build0046c: Width 100% remains decided here before the sample loop; no M/S work at unity.
    enum : uint8_t { MODFX_OFF=0, MODFX_CHORUS=1, MODFX_FLANGER=2, MODFX_PHASER=3 };
    uint8_t modFx = MODFX_OFF;
    const bool phaserEnabled = RTALStereoPhaser::parameters().enabled;
    const bool flangerEnabled = RTALStereoFlanger::parameters().enabled;
    const bool chorusEnabled = RTALStereoChorus::parameters().enabled;
    if (phaserEnabled) modFx = MODFX_PHASER;
    else if (flangerEnabled) modFx = MODFX_FLANGER;
    else if (chorusEnabled) modFx = MODFX_CHORUS;
    if (modFx == MODFX_PHASER) RTALStereoPhaser::beginBlock(frames);
    else if (modFx == MODFX_FLANGER) RTALStereoFlanger::beginBlock(frames);
    else if (modFx == MODFX_CHORUS) RTALStereoChorus::beginBlock(frames);
    RTALStereoReverb::beginBlock(frames);
    const bool reverbActive = RTALStereoReverb::activeForBlock();
    RTALStereoWidth::beginBlock(frames);
    const bool widthActive = RTALStereoWidth::activeForBlock();

    if (blockTarget_.hardBypass)
    {
        const uint32_t blockUs =
            RTAL_DSP_BLOCK_PROFILER_ENABLED
            ? micros() - blockStartedUs
            : 0;

        portENTER_CRITICAL(&mux_);
        statistics_.processedFrames += frames;
        ++statistics_.processedBlocks;
        statistics_.accumulatedBlockUs += blockUs;
        if (blockUs > statistics_.maximumBlockUs)
            statistics_.maximumBlockUs = blockUs;
        if (blockUs < statistics_.minimumBlockUs)
            statistics_.minimumBlockUs = blockUs;
        const uint32_t budgetUs = RTALAudioEngine::blockBudgetUs();
        if (blockUs > budgetUs)
        {
            ++statistics_.deadlineMisses;
            const uint32_t overrunUs = blockUs - budgetUs;
            if (overrunUs > statistics_.maximumOverrunUs)
                statistics_.maximumOverrunUs = overrunUs;
        }
        if (modFx <= MODFX_PHASER)
        {
            ++statistics_.modFxBlocks[modFx];
            statistics_.modFxAccumulatedBlockUs[modFx] += blockUs;
            if (blockUs > statistics_.modFxMaximumBlockUs[modFx])
                statistics_.modFxMaximumBlockUs[modFx] = blockUs;
        }
        portEXIT_CRITICAL(&mux_);
        return;
    }

    uint32_t limiterEventsLeft = 0;
    uint32_t limiterEventsRight = 0;
    uint32_t hardClipsLeft = 0;
    uint32_t hardClipsRight = 0;

    float inputPeakLeft = 0.0f;
    float inputPeakRight = 0.0f;
    float outputPeakLeft = 0.0f;
    float outputPeakRight = 0.0f;

    int64_t inputSumLeft = 0;
    int64_t inputSumRight = 0;
    int64_t outputSumLeft = 0;
    int64_t outputSumRight = 0;

    double inputSquareLeft = 0.0;
    double inputSquareRight = 0.0;
    double outputSquareLeft = 0.0;
    double outputSquareRight = 0.0;
    uint32_t rmsSamples = 0;

    // Build0046f Peak Forensics: cycle-counter brackets around existing stages.
    // ESP.getCycleCount() is a single-register read on ESP32-S3; no Serial,
    // allocation or locks are used in the sample loop.
    const uint32_t forensicBlockStartCycles = RTAL_PEAK_FORENSICS_ENABLED ? ESP.getCycleCount() : 0;
    uint32_t forensicPreCycles = 0;
    uint32_t forensicDelayCycles = 0;
    uint32_t forensicModCycles = 0;
    uint32_t forensicReverbCycles = 0;
    uint32_t forensicPostCycles = 0;

    // The unity-dry shortcut is valid only when no real wet processor
    // needs to run. Build 0014a skipped the stereo delay because the kernel's
    // generic Wet parameter was zero, although the delay has its own level.
    const bool unityDryTarget =
        RTAL_DSP_FAST_UNITY_DRY_PATH &&
        !RTAL_STEREO_DELAY_ENABLED &&
        nearUnity(blockTarget_.dry) &&
        nearZero(blockTarget_.wet);

    if (RTAL_STEREO_DELAY_ENABLED)
        RTALStereoDelay::beginBlock(frames);

    for (size_t frame = 0; frame < frames; ++frame)
    {
        uint32_t forensicStageStart = RTAL_PEAK_FORENSICS_ENABLED ? ESP.getCycleCount() : 0;
        const size_t li = frame * 2;
        const size_t ri = li + 1;

        const int16_t rawLeft =
            RTALAudioFormat::containerToInt16(samples[li]);
        const int16_t rawRight =
            RTALAudioFormat::containerToInt16(samples[ri]);

        inputSumLeft += rawLeft;
        inputSumRight += rawRight;

        const float inputLeft =
            static_cast<float>(rawLeft) * RTAL_DSP_INT16_TO_FLOAT;
        const float inputRight =
            static_cast<float>(rawRight) * RTAL_DSP_INT16_TO_FLOAT;

        const float inputAbsLeft = fabsf(inputLeft);
        const float inputAbsRight = fabsf(inputRight);

        if (inputAbsLeft > inputPeakLeft)
            inputPeakLeft = inputAbsLeft;
        if (inputAbsRight > inputPeakRight)
            inputPeakRight = inputAbsRight;

        float left = inputLeft;
        float right = inputRight;

        if (RTAL_DC_BLOCKER_ENABLED)
        {
            left = processDcLeft(left);
            right = processDcRight(right);
        }

        current_.dry +=
            (blockTarget_.dry - current_.dry) *
            RTAL_DSP_SMOOTHING_COEFFICIENT;
        current_.wet +=
            (blockTarget_.wet - current_.wet) *
            RTAL_DSP_SMOOTHING_COEFFICIENT;

        float outputLeft;
        float outputRight;

        const bool unityDryNow =
            unityDryTarget &&
            nearUnity(current_.dry) &&
            nearZero(current_.wet);

        if (blockTarget_.fxBypass || unityDryNow)
        {
            if (RTAL_PEAK_FORENSICS_ENABLED)
            {
                const uint32_t nowCycles = ESP.getCycleCount();
                forensicPreCycles += nowCycles - forensicStageStart;
                forensicStageStart = nowCycles;
            }
            outputLeft = left;
            outputRight = right;
        }
        else
        {
            float effectLeft = 0.0f;
            float effectRight = 0.0f;

            if (RTAL_PEAK_FORENSICS_ENABLED)
            {
                const uint32_t nowCycles = ESP.getCycleCount();
                forensicPreCycles += nowCycles - forensicStageStart;
                forensicStageStart = nowCycles;
            }
            RTALStereoDelay::process(
                left,
                right,
                effectLeft,
                effectRight);
            if (RTAL_PEAK_FORENSICS_ENABLED)
            {
                const uint32_t nowCycles = ESP.getCycleCount();
                forensicDelayCycles += nowCycles - forensicStageStart;
                forensicStageStart = nowCycles;
            }

            float stageLeft = current_.dry * left + effectLeft;
            float stageRight = current_.dry * right + effectRight;

            // Fixed v1.4 chain: Delay -> one selected MOD FX.
            // Build0043 offers Chorus OR Flanger OR Phaser.
            float modLeft = stageLeft;
            float modRight = stageRight;
            if (modFx == MODFX_PHASER)
                RTALStereoPhaser::process(stageLeft, stageRight, modLeft, modRight);
            else if (modFx == MODFX_FLANGER)
                RTALStereoFlanger::process(stageLeft, stageRight, modLeft, modRight);
            else if (modFx == MODFX_CHORUS)
                RTALStereoChorus::process(stageLeft, stageRight, modLeft, modRight);
            if (RTAL_PEAK_FORENSICS_ENABLED)
            {
                const uint32_t nowCycles = ESP.getCycleCount();
                forensicModCycles += nowCycles - forensicStageStart;
                forensicStageStart = nowCycles;
            }

            // Build0046 fixed chain: Delay -> MOD FX -> Reverb -> Width -> Limiter.
            float reverbLeft = modLeft;
            float reverbRight = modRight;
            if (reverbActive)
                RTALStereoReverb::process(modLeft, modRight, reverbLeft, reverbRight);
            if (RTAL_PEAK_FORENSICS_ENABLED)
            {
                const uint32_t nowCycles = ESP.getCycleCount();
                forensicReverbCycles += nowCycles - forensicStageStart;
                forensicStageStart = nowCycles;
            }

            // Width OFF or exact 100% is a block-level true bypass.
            if (widthActive)
                RTALStereoWidth::process(reverbLeft, reverbRight, outputLeft, outputRight);
            else {
                outputLeft = reverbLeft;
                outputRight = reverbRight;
            }
        }

        if (RTAL_DSP_LIMITER_ENABLED)
        {
            const bool limiterFastLeft =
                RTAL_DSP_FAST_LIMITER_PATH &&
                limiterLeft_.gain() >= 0.999999f &&
                fabsf(outputLeft) <= RTAL_DSP_LIMITER_THRESHOLD;

            const bool limiterFastRight =
                RTAL_DSP_FAST_LIMITER_PATH &&
                limiterRight_.gain() >= 0.999999f &&
                fabsf(outputRight) <= RTAL_DSP_LIMITER_THRESHOLD;

            if (!limiterFastLeft)
            {
                outputLeft = limiterLeft_.process(
                    outputLeft,
                    limiterEventsLeft,
                    hardClipsLeft);
            }

            if (!limiterFastRight)
            {
                outputRight = limiterRight_.process(
                    outputRight,
                    limiterEventsRight,
                    hardClipsRight);
            }
        }

        const float outputAbsLeft = fabsf(outputLeft);
        const float outputAbsRight = fabsf(outputRight);

        if (outputAbsLeft > outputPeakLeft)
            outputPeakLeft = outputAbsLeft;
        if (outputAbsRight > outputPeakRight)
            outputPeakRight = outputAbsRight;

        if (RTAL_DSP_RMS_DIAGNOSTICS_ENABLED &&
            (frame % RTAL_DSP_RMS_DECIMATION) == 0)
        {
            inputSquareLeft +=
                static_cast<double>(inputLeft) * inputLeft;
            inputSquareRight +=
                static_cast<double>(inputRight) * inputRight;
            outputSquareLeft +=
                static_cast<double>(outputLeft) * outputLeft;
            outputSquareRight +=
                static_cast<double>(outputRight) * outputRight;
            ++rmsSamples;
        }

        uint32_t conversionClipsLeft = 0;
        uint32_t conversionClipsRight = 0;

        const int16_t output16Left =
            floatToInt16(outputLeft, conversionClipsLeft);
        const int16_t output16Right =
            floatToInt16(outputRight, conversionClipsRight);

        hardClipsLeft += conversionClipsLeft;
        hardClipsRight += conversionClipsRight;

        samples[li] =
            RTALAudioFormat::int16ToContainer(output16Left);
        samples[ri] =
            RTALAudioFormat::int16ToContainer(output16Right);

        outputSumLeft += output16Left;
        outputSumRight += output16Right;

        if (RTAL_PEAK_FORENSICS_ENABLED)
            forensicPostCycles += ESP.getCycleCount() - forensicStageStart;
    }

    if (RTAL_STEREO_DELAY_ENABLED)
        RTALStereoDelay::endBlock();

    const uint32_t forensicTotalCycles = RTAL_PEAK_FORENSICS_ENABLED
        ? ESP.getCycleCount() - forensicBlockStartCycles
        : 0;

    const uint32_t blockUs =
        RTAL_DSP_BLOCK_PROFILER_ENABLED
        ? micros() - blockStartedUs
        : 0;

    portENTER_CRITICAL(&mux_);

    const uint64_t previousFrames = statistics_.processedFrames;
    const uint64_t totalFrames = previousFrames + frames;

    statistics_.inputDcLeft =
        (statistics_.inputDcLeft * previousFrames +
         (static_cast<double>(inputSumLeft) / frames) * frames) /
        totalFrames;
    statistics_.inputDcRight =
        (statistics_.inputDcRight * previousFrames +
         (static_cast<double>(inputSumRight) / frames) * frames) /
        totalFrames;
    statistics_.outputDcLeft =
        (statistics_.outputDcLeft * previousFrames +
         (static_cast<double>(outputSumLeft) / frames) * frames) /
        totalFrames;
    statistics_.outputDcRight =
        (statistics_.outputDcRight * previousFrames +
         (static_cast<double>(outputSumRight) / frames) * frames) /
        totalFrames;

    statistics_.processedFrames = totalFrames;
    ++statistics_.processedBlocks;
    statistics_.rmsSamples += rmsSamples;

    statistics_.inputSquareLeft += inputSquareLeft;
    statistics_.inputSquareRight += inputSquareRight;
    statistics_.outputSquareLeft += outputSquareLeft;
    statistics_.outputSquareRight += outputSquareRight;

    statistics_.limiterEventsLeft += limiterEventsLeft;
    statistics_.limiterEventsRight += limiterEventsRight;
    statistics_.hardClipsLeft += hardClipsLeft;
    statistics_.hardClipsRight += hardClipsRight;

    if (inputPeakLeft > statistics_.inputPeakLeft)
        statistics_.inputPeakLeft = inputPeakLeft;
    if (inputPeakRight > statistics_.inputPeakRight)
        statistics_.inputPeakRight = inputPeakRight;
    if (outputPeakLeft > statistics_.outputPeakLeft)
        statistics_.outputPeakLeft = outputPeakLeft;
    if (outputPeakRight > statistics_.outputPeakRight)
        statistics_.outputPeakRight = outputPeakRight;

    statistics_.accumulatedBlockUs += blockUs;

    if (blockUs > statistics_.maximumBlockUs)
        statistics_.maximumBlockUs = blockUs;
    if (blockUs < statistics_.minimumBlockUs)
        statistics_.minimumBlockUs = blockUs;

    const uint32_t budgetUs = RTALAudioEngine::blockBudgetUs();
    if (blockUs > budgetUs)
    {
        ++statistics_.deadlineMisses;
        const uint32_t overrunUs = blockUs - budgetUs;
        if (overrunUs > statistics_.maximumOverrunUs)
            statistics_.maximumOverrunUs = overrunUs;
    }

    if (modFx <= MODFX_PHASER)
    {
        ++statistics_.modFxBlocks[modFx];
        statistics_.modFxAccumulatedBlockUs[modFx] += blockUs;
        if (blockUs > statistics_.modFxMaximumBlockUs[modFx])
            statistics_.modFxMaximumBlockUs[modFx] = blockUs;
    }
    if (reverbActive) ++statistics_.reverbActiveBlocks;

    if (RTAL_PEAK_FORENSICS_ENABLED)
    {
        if (blockUs > RTAL_PEAK_HIST_2600_US) ++statistics_.peakGt2600;
        if (blockUs > RTAL_PEAK_HIST_2700_US) ++statistics_.peakGt2700;
        if (blockUs > RTAL_PEAK_HIST_2800_US) ++statistics_.peakGt2800;
        if (blockUs > budgetUs) ++statistics_.peakGtBudget;
        if (blockUs > RTAL_PEAK_HIST_3000_US) ++statistics_.peakGt3000;
        if (blockUs > RTAL_PEAK_HIST_3200_US) ++statistics_.peakGt3200;

        if (blockUs >= RTAL_PEAK_FORENSICS_TRIGGER_US)
        {
            RTALDSPPeakEvent& e = statistics_.peakEvents[statistics_.peakEventCount % RTAL_PEAK_FORENSICS_RING_SIZE];
            const uint32_t accountedCycles = forensicPreCycles + forensicDelayCycles +
                forensicModCycles + forensicReverbCycles + forensicPostCycles;
            const uint32_t otherCycles = forensicTotalCycles > accountedCycles
                ? forensicTotalCycles - accountedCycles : 0;
            const RTALModCoreState ms = RTALModCore::state();
            const RTALStereoDelayForensics df = RTALStereoDelay::forensics();
            const uint32_t delayScale = df.sampledFrames
                ? df.totalFrames / df.sampledFrames : 0;
            const auto delayEstimateUs = [delayScale](uint64_t cycles) -> uint32_t
            {
                return static_cast<uint32_t>(
                    (cycles * delayScale) / RTAL_CPU_CYCLES_PER_US);
            };

            e.blockNumber = statistics_.processedBlocks;
            e.totalUs = blockUs;
            e.preUs = forensicPreCycles / RTAL_CPU_CYCLES_PER_US;
            e.delayUs = forensicDelayCycles / RTAL_CPU_CYCLES_PER_US;
            e.modUs = forensicModCycles / RTAL_CPU_CYCLES_PER_US;
            e.reverbUs = forensicReverbCycles / RTAL_CPU_CYCLES_PER_US;
            e.postUs = forensicPostCycles / RTAL_CPU_CYCLES_PER_US;
            e.otherUs = otherCycles / RTAL_CPU_CYCLES_PER_US;
            e.delaySmoothUs = delayEstimateUs(df.smoothCycles);
            e.delayReadUs = delayEstimateUs(df.readCycles);
            e.delayFeedbackUs = delayEstimateUs(df.feedbackCycles);
            e.delayDuckUs = delayEstimateUs(df.duckCycles);
            e.delayWriteUs = delayEstimateUs(df.writeCycles);
            e.delayStatsUs = delayEstimateUs(df.statsCycles);
            e.lfoPhase = ms.phase;
            e.modFx = modFx;
            e.reverbActive = reverbActive;
            ++statistics_.peakEventCount;
            ++statistics_.peakEventTotal;
        }
    }

    statistics_.limiterGainLeft = limiterLeft_.gain();
    statistics_.limiterGainRight = limiterRight_.gain();

    portEXIT_CRITICAL(&mux_);
}

void RTALDSPKernel::resetStatisticsUnlocked()
{
    memset(&statistics_, 0, sizeof(statistics_));
    statistics_.minimumBlockUs = UINT32_MAX;
    statistics_.limiterGainLeft = limiterLeft_.gain();
    statistics_.limiterGainRight = limiterRight_.gain();
}

RTALDSPKernelStatistics RTALDSPKernel::statistics(bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    const RTALDSPKernelStatistics copy = statistics_;
    if (resetWindow) resetStatisticsUnlocked();
    portEXIT_CRITICAL(&mux_);
    return copy;
}

void RTALDSPKernel::printReport()
{
    const RTALDSPKernelStatistics s = statistics(true);
    const RTALDSPKernelParameters p = parameters();

    const uint32_t averageBlockUs =
        s.processedBlocks
        ? static_cast<uint32_t>(
            s.accumulatedBlockUs / s.processedBlocks)
        : 0;

    const double rmsDivisor =
        s.rmsSamples ? static_cast<double>(s.rmsSamples) : 1.0;

    const double inputRmsLeft =
        sqrt(s.inputSquareLeft / rmsDivisor);
    const double inputRmsRight =
        sqrt(s.inputSquareRight / rmsDivisor);
    const double outputRmsLeft =
        sqrt(s.outputSquareLeft / rmsDivisor);
    const double outputRmsRight =
        sqrt(s.outputSquareRight / rmsDivisor);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Kernel fx_bypass=%s hard_bypass=%s dry=%.3f wet=%.3f frames=%llu blocks=%lu",
        p.fxBypass ? "ON" : "OFF",
        p.hardBypass ? "ON" : "OFF",
        p.dry,
        p.wet,
        static_cast<unsigned long long>(s.processedFrames),
        static_cast<unsigned long>(s.processedBlocks));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Kernel peak_in=%.5f/%.5f peak_out=%.5f/%.5f rms_in=%.5f/%.5f rms_out=%.5f/%.5f decim=%u",
        s.inputPeakLeft,
        s.inputPeakRight,
        s.outputPeakLeft,
        s.outputPeakRight,
        inputRmsLeft,
        inputRmsRight,
        outputRmsLeft,
        outputRmsRight,
        static_cast<unsigned>(RTAL_DSP_RMS_DECIMATION));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Kernel DC_in=%.2f/%.2f DC_out=%.2f/%.2f limiter_gain=%.5f/%.5f events=%lu/%lu clips=%lu/%lu",
        s.inputDcLeft,
        s.inputDcRight,
        s.outputDcLeft,
        s.outputDcRight,
        s.limiterGainLeft,
        s.limiterGainRight,
        static_cast<unsigned long>(s.limiterEventsLeft),
        static_cast<unsigned long>(s.limiterEventsRight),
        static_cast<unsigned long>(s.hardClipsLeft),
        static_cast<unsigned long>(s.hardClipsRight));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Kernel block_us avg=%lu min=%lu max=%lu budget=%lu load=%.2f%%",
        static_cast<unsigned long>(averageBlockUs),
        static_cast<unsigned long>(
            s.minimumBlockUs == UINT32_MAX ? 0 : s.minimumBlockUs),
        static_cast<unsigned long>(s.maximumBlockUs),
        static_cast<unsigned long>(RTALAudioEngine::blockBudgetUs()),
        RTALAudioEngine::blockBudgetUs()
        ? (100.0f * averageBlockUs) /
          RTALAudioEngine::blockBudgetUs()
        : 0.0f);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Kernel deadline_miss=%lu max_overrun=%lu us reverb_blocks=%lu",
        static_cast<unsigned long>(s.deadlineMisses),
        static_cast<unsigned long>(s.maximumOverrunUs),
        static_cast<unsigned long>(s.reverbActiveBlocks));

    const char* modNames[4] = {"OFF", "CHORUS", "FLANGER", "PHASER"};
    for (uint8_t i = 0; i < 4; ++i)
    {
        if (!s.modFxBlocks[i]) continue;
        const uint32_t avgUs = static_cast<uint32_t>(
            s.modFxAccumulatedBlockUs[i] / s.modFxBlocks[i]);
        RTALLogger::printf(
            RTALLogLevel::Info,
            "Kernel MODFX %s blocks=%lu avg=%lu us max=%lu us",
            modNames[i],
            static_cast<unsigned long>(s.modFxBlocks[i]),
            static_cast<unsigned long>(avgUs),
            static_cast<unsigned long>(s.modFxMaximumBlockUs[i]));
    }

    if (RTAL_PEAK_FORENSICS_ENABLED)
    {
        RTALLogger::printf(
            RTALLogLevel::Info,
            "PeakHist KERNEL >2600=%lu >2700=%lu >2800=%lu >budget=%lu >3000=%lu >3200=%lu events=%lu",
            static_cast<unsigned long>(s.peakGt2600),
            static_cast<unsigned long>(s.peakGt2700),
            static_cast<unsigned long>(s.peakGt2800),
            static_cast<unsigned long>(s.peakGtBudget),
            static_cast<unsigned long>(s.peakGt3000),
            static_cast<unsigned long>(s.peakGt3200),
            static_cast<unsigned long>(s.peakEventTotal));

        const uint32_t available = s.peakEventCount < RTAL_PEAK_FORENSICS_RING_SIZE
            ? s.peakEventCount : RTAL_PEAK_FORENSICS_RING_SIZE;
        const char* peakNames[4] = {"OFF", "CHORUS", "FLANGER", "PHASER"};
        const uint32_t first = s.peakEventCount > available ? s.peakEventCount - available : 0;
        const uint32_t show = available > 4 ? 4 : available;
        for (uint32_t j = available - show; j < available; ++j)
        {
            const uint32_t logical = first + j;
            const RTALDSPPeakEvent& e = s.peakEvents[logical % RTAL_PEAK_FORENSICS_RING_SIZE];
            RTALLogger::printf(
                RTALLogLevel::Info,
                "PeakK #%lu total=%lu pre=%lu delay=%lu mod=%lu reverb=%lu post=%lu other=%lu us fx=%s rv=%s phase=%08lX",
                static_cast<unsigned long>(e.blockNumber),
                static_cast<unsigned long>(e.totalUs),
                static_cast<unsigned long>(e.preUs),
                static_cast<unsigned long>(e.delayUs),
                static_cast<unsigned long>(e.modUs),
                static_cast<unsigned long>(e.reverbUs),
                static_cast<unsigned long>(e.postUs),
                static_cast<unsigned long>(e.otherUs),
                peakNames[e.modFx <= 3 ? e.modFx : 0],
                e.reverbActive ? "ON" : "OFF",
                static_cast<unsigned long>(e.lfoPhase));

            if (RTAL_DELAY_FORENSICS_ENABLED)
            {
                RTALLogger::printf(
                    RTALLogLevel::Info,
                    "DelayK #%lu est smooth=%lu read=%lu feedback=%lu duck=%lu write=%lu stats=%lu us",
                    static_cast<unsigned long>(e.blockNumber),
                    static_cast<unsigned long>(e.delaySmoothUs),
                    static_cast<unsigned long>(e.delayReadUs),
                    static_cast<unsigned long>(e.delayFeedbackUs),
                    static_cast<unsigned long>(e.delayDuckUs),
                    static_cast<unsigned long>(e.delayWriteUs),
                    static_cast<unsigned long>(e.delayStatsUs));
            }
        }
    }
}
