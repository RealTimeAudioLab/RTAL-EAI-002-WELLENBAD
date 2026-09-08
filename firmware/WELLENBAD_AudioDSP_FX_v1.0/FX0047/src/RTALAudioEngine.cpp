#include "../include/RTALAudioEngine.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALTaskRegistry.h"
#include "../include/RTALI2SDriver.h"
#include "../include/RTALEventLog.h"
#include "../include/RTALAudioAnalyzer.h"
#include "../include/RTALDSPKernel.h"
#include <cstring>

TaskHandle_t RTALAudioEngine::taskHandle_ = nullptr;
int8_t RTALAudioEngine::registryId_ = -1;
portMUX_TYPE RTALAudioEngine::statisticsMux_ = portMUX_INITIALIZER_UNLOCKED;
RTALAudioStatistics RTALAudioEngine::statistics_ = {};

static int32_t gAudioBlock[RTAL_AUDIO_BLOCK_FRAMES * RTAL_AUDIO_CHANNELS];

// Optional raw-input diagnostics retained from Build0046d. Disabled by default in 0046e.
static int32_t gPreviousRawBlock[RTAL_AUDIO_BLOCK_FRAMES * RTAL_AUDIO_CHANNELS];
static bool gPreviousRawBlockValid = false;
static bool gRawPairValid = false;
static bool gRawFreezeLatched = false;
static bool gRawRepeatLatched = false;
static int32_t gRawLastLeft = 0;
static int32_t gRawLastRight = 0;
static uint32_t gRawIdenticalRunFrames = 0;
static uint32_t gRawRepeatRunBlocks = 0;

uint32_t RTALAudioEngine::blockBudgetUs()
{
    return static_cast<uint32_t>((1000000ULL * RTAL_AUDIO_BLOCK_FRAMES) / RTAL_AUDIO_SAMPLE_RATE);
}

const char* RTALAudioEngine::clockStateName(RTALAudioClockState state)
{
    switch (state)
    {
        case RTALAudioClockState::Waiting: return "WAITING";
        case RTALAudioClockState::Present: return "PRESENT";
        case RTALAudioClockState::Lost: return "LOST";
        default: return "UNKNOWN";
    }
}

bool RTALAudioEngine::intervalIsPlausible(uint32_t intervalUs)
{
    return intervalUs >= RTAL_CLOCK_MIN_BLOCK_INTERVAL_US &&
           intervalUs <= RTAL_CLOCK_MAX_BLOCK_INTERVAL_US;
}

RTALStatus RTALAudioEngine::begin()
{
    statistics_ = {};
    statistics_.clockState = RTALAudioClockState::Waiting;
    memset(gPreviousRawBlock, 0, sizeof(gPreviousRawBlock));
    gPreviousRawBlockValid = false;
    gRawPairValid = false;
    gRawFreezeLatched = false;
    gRawRepeatLatched = false;
    gRawLastLeft = 0;
    gRawLastRight = 0;
    gRawIdenticalRunFrames = 0;
    gRawRepeatRunBlocks = 0;

    if (RTALAudioAnalyzer::begin() != RTALStatus::OK)
        return RTALStatus::FATAL;

    if (RTALDSPKernel::begin() != RTALStatus::OK)
        return RTALStatus::FATAL;

    const RTALStatus i2sStatus = RTALI2SDriver::begin();
    if (i2sStatus != RTALStatus::OK) return i2sStatus;

    const BaseType_t result = xTaskCreatePinnedToCore(
        audioTask, "RTALAudio", RTAL_AUDIO_TASK_STACK_WORDS, nullptr,
        RTAL_AUDIO_TASK_PRIORITY, &taskHandle_, RTAL_AUDIO_TASK_CORE);

    if (result != pdPASS || !taskHandle_)
    {
        RTALI2SDriver::end();
        return RTALStatus::FATAL;
    }

    registryId_ = RTALTaskRegistry::registerTask(
        "RTALAudio", taskHandle_, RTAL_AUDIO_TASK_CORE,
        RTAL_AUDIO_TASK_PRIORITY, RTAL_AUDIO_TASK_TIMEOUT_MS, false);

    if (registryId_ < 0) return RTALStatus::FATAL;

    RTALTaskRegistry::setState(static_cast<size_t>(registryId_), RTALTaskState::WaitClock);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "AudioTask ..... PASS core=%d prio=%u state=WAIT_CLOCK budget=%lu us qualify=%lu",
        RTAL_AUDIO_TASK_CORE,
        static_cast<unsigned>(RTAL_AUDIO_TASK_PRIORITY),
        static_cast<unsigned long>(blockBudgetUs()),
        static_cast<unsigned long>(RTAL_CLOCK_REQUIRED_GOOD_BLOCKS));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Production .... %s block_profiler=%s deadline=ON i2s_errors=ON",
        RTAL_PRODUCTION_PROFILE ? "STABLE_CANDIDATE" : "DIAGNOSTIC",
        RTAL_DSP_BLOCK_PROFILER_ENABLED ? "ON" : "OFF");

    RTALLogger::printf(
        RTALLogLevel::Info,
        "RealtimeDiag .. format=%s peak=1/%lu raw=%s rms=%s stage=%s i2s_wait=%s peak_forensics=%s",
        RTAL_AUDIO_FORMAT_ANALYSIS_ENABLED ? "ON" : "OFF",
        static_cast<unsigned long>(RTAL_AUDIO_PEAK_BLOCK_DECIMATION),
        RTAL_RAW_INPUT_DIAGNOSTICS_ENABLED ? "ON" : "OFF",
        RTAL_DSP_RMS_DIAGNOSTICS_ENABLED ? "ON" : "OFF",
        RTAL_DSP_STAGE_TIMING_ENABLED ? "ON" : "OFF",
        RTAL_I2S_WAIT_TIMING_ENABLED ? "ON" : "OFF",
        RTAL_PEAK_FORENSICS_ENABLED ? "ON" : "OFF");

    RTALLogger::printf(
        RTALLogLevel::Info,
        "DelayCoeffOpt .. PASS interval=%lu samples moving_only=YES substage_forensics=%s",
        static_cast<unsigned long>(RTAL_DELAY_COEFFICIENT_UPDATE_INTERVAL),
        RTAL_DELAY_FORENSICS_ENABLED ? "ON" : "OFF");

    RTALLogger::printf(
        RTALLogLevel::Info,
        "MixLaw ........ %s mode=NORMALIZED_LINEAR_CONSTANT_POWER delay=ADDITIVE",
        RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED ? "PASS" : "LEGACY");

    RTALEventLog::add(RTALStatus::OK, "Audio", "Waiting for I2S clock");
    return RTALStatus::OK;
}

void RTALAudioEngine::updatePeak(const int32_t* samples, size_t sampleCount)
{
    int32_t peak = 0;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        int64_t value = samples[i];
        if (value < 0) value = -value;
        if (value > peak) peak = static_cast<int32_t>(value);
    }

    portENTER_CRITICAL(&statisticsMux_);
    if (peak > statistics_.peakAbsolute) statistics_.peakAbsolute = peak;
    portEXIT_CRITICAL(&statisticsMux_);
}

void RTALAudioEngine::analyzeRawInput(const int32_t* samples, size_t frames)
{
    if (!RTAL_RAW_INPUT_DIAGNOSTICS_ENABLED || !samples || frames == 0) return;

    uint32_t identicalThisBlock = 0;
    uint32_t newFreezeEvents = 0;
    int32_t freezeLeft = 0;
    int32_t freezeRight = 0;

    for (size_t frame = 0; frame < frames; ++frame)
    {
        const int32_t left = samples[frame * 2];
        const int32_t right = samples[frame * 2 + 1];

        if (gRawPairValid && left == gRawLastLeft && right == gRawLastRight)
        {
            ++gRawIdenticalRunFrames;
            ++identicalThisBlock;

            if (!gRawFreezeLatched &&
                gRawIdenticalRunFrames >= RTAL_RAW_FREEZE_THRESHOLD_FRAMES)
            {
                gRawFreezeLatched = true;
                ++newFreezeEvents;
                freezeLeft = left;
                freezeRight = right;
            }
        }
        else
        {
            gRawLastLeft = left;
            gRawLastRight = right;
            gRawPairValid = true;
            gRawIdenticalRunFrames = 1;
            gRawFreezeLatched = false;
        }
    }

    bool repeatedBlock = false;
    if (gPreviousRawBlockValid)
        repeatedBlock = memcmp(gPreviousRawBlock, samples, sizeof(gPreviousRawBlock)) == 0;

    uint32_t newRepeatEvents = 0;
    if (repeatedBlock)
    {
        ++gRawRepeatRunBlocks;
        if (!gRawRepeatLatched &&
            gRawRepeatRunBlocks >= RTAL_RAW_REPEAT_BLOCK_THRESHOLD)
        {
            gRawRepeatLatched = true;
            ++newRepeatEvents;
        }
    }
    else
    {
        // Start a new run with the current block so the threshold denotes
        // the actual number of equal consecutive 128-frame blocks.
        gRawRepeatRunBlocks = 1;
        gRawRepeatLatched = false;
    }

    memcpy(gPreviousRawBlock, samples, sizeof(gPreviousRawBlock));
    gPreviousRawBlockValid = true;

    portENTER_CRITICAL(&statisticsMux_);
    statistics_.rawIdenticalFrames += identicalThisBlock;
    statistics_.rawFreezeCurrentFrames = gRawIdenticalRunFrames;
    if (gRawIdenticalRunFrames > statistics_.rawFreezeMaximumFrames)
        statistics_.rawFreezeMaximumFrames = gRawIdenticalRunFrames;
    if (newFreezeEvents)
    {
        statistics_.rawFreezeEvents += newFreezeEvents;
        statistics_.rawFreezeLeft = freezeLeft;
        statistics_.rawFreezeRight = freezeRight;
    }

    if (repeatedBlock) ++statistics_.rawRepeatedBlocks;
    statistics_.rawRepeatCurrentBlocks = gRawRepeatRunBlocks;
    if (gRawRepeatRunBlocks > statistics_.rawRepeatMaximumBlocks)
        statistics_.rawRepeatMaximumBlocks = gRawRepeatRunBlocks;
    statistics_.rawRepeatEvents += newRepeatEvents;
    portEXIT_CRITICAL(&statisticsMux_);
}

void RTALAudioEngine::setClockState(RTALAudioClockState state)
{
    RTALAudioClockState previous;

    portENTER_CRITICAL(&statisticsMux_);
    previous = statistics_.clockState;
    if (previous == state)
    {
        portEXIT_CRITICAL(&statisticsMux_);
        return;
    }
    statistics_.clockState = state;
    if (state == RTALAudioClockState::Present) ++statistics_.clockPresentTransitions;
    if (state == RTALAudioClockState::Lost) ++statistics_.clockLostTransitions;
    portEXIT_CRITICAL(&statisticsMux_);

    const size_t taskIndex = static_cast<size_t>(registryId_);
    if (state == RTALAudioClockState::Present)
    {
        RTALTaskRegistry::setMonitorEnabled(taskIndex, true);
        RTALTaskRegistry::setState(taskIndex, RTALTaskState::Running);
        RTALLogger::info("I2S clock qualified; AudioTask RUNNING");
        RTALEventLog::add(RTALStatus::OK, "Audio", "I2S clock qualified");
    }
    else
    {
        RTALTaskRegistry::setMonitorEnabled(taskIndex, false);
        RTALTaskRegistry::setState(taskIndex, RTALTaskState::WaitClock);
        if (state == RTALAudioClockState::Lost)
        {
            RTALLogger::warning("I2S clock lost; AudioTask WAIT_CLOCK");
            RTALEventLog::add(RTALStatus::WARNING, "Audio", "I2S clock lost");
        }
    }
}

void RTALAudioEngine::audioTask(void*)
{
    memset(gAudioBlock, 0, sizeof(gAudioBlock));

    uint32_t goodSequence = 0;
    uint32_t previousFullBlockUs = 0;
    uint32_t lastValidAudioBlockMs = 0;

    // Build0046e: peak metering is decimated; the expensive format analyzer
    // receives only four evenly spaced stereo frames per block. This avoids
    // the large periodic CPU burst caused by analyzing a complete block.
    uint32_t peakCountdown = 0;
    int32_t analysisSamples[RTAL_AUDIO_ANALYSIS_FRAMES_PER_BLOCK * RTAL_AUDIO_CHANNELS];

    for (;;)
    {
        RTALTaskRegistry::heartbeat(registryId_);

        size_t receivedBytes = 0;
        const bool fullRead = RTALI2SDriver::readBlock(
            gAudioBlock, sizeof(gAudioBlock), receivedBytes,
            pdMS_TO_TICKS(RTAL_I2S_IO_TIMEOUT_MS));

        if (!fullRead)
        {
            portENTER_CRITICAL(&statisticsMux_);
            ++statistics_.probePartialReads;
            portEXIT_CRITICAL(&statisticsMux_);
            goodSequence = 0;

            RTALAudioClockState current;
            portENTER_CRITICAL(&statisticsMux_);
            current = statistics_.clockState;
            portEXIT_CRITICAL(&statisticsMux_);

            if (current == RTALAudioClockState::Present &&
                static_cast<uint32_t>(millis() - lastValidAudioBlockMs) > RTAL_CLOCK_LOST_TIMEOUT_MS)
                setClockState(RTALAudioClockState::Lost);

            vTaskDelay(1);
            continue;
        }

        const uint32_t nowUs = micros();
        const uint32_t intervalUs = previousFullBlockUs ? nowUs - previousFullBlockUs : 0;
        previousFullBlockUs = nowUs;

        portENTER_CRITICAL(&statisticsMux_);
        ++statistics_.probeFullBlocks;
        portEXIT_CRITICAL(&statisticsMux_);

        RTALAudioClockState current;
        portENTER_CRITICAL(&statisticsMux_);
        current = statistics_.clockState;
        portEXIT_CRITICAL(&statisticsMux_);

        if (current != RTALAudioClockState::Present)
        {
            if (intervalUs && intervalIsPlausible(intervalUs)) ++goodSequence;
            else
            {
                goodSequence = 0;
                if (intervalUs)
                {
                    portENTER_CRITICAL(&statisticsMux_);
                    ++statistics_.rejectedTimingBlocks;
                    portEXIT_CRITICAL(&statisticsMux_);
                }
            }

            // Probe data is discarded: no peak, no TX, no audio-block count.
            if (goodSequence >= RTAL_CLOCK_REQUIRED_GOOD_BLOCKS)
            {
                setClockState(RTALAudioClockState::Present);
                lastValidAudioBlockMs = millis();
            }
            continue;
        }

        if (RTAL_CLOCK_VALIDATE_INTERVAL_AFTER_PRESENT &&
            !intervalIsPlausible(intervalUs))
        {
            portENTER_CRITICAL(&statisticsMux_);
            ++statistics_.rejectedTimingBlocks;
            portEXIT_CRITICAL(&statisticsMux_);
            continue;
        }

        lastValidAudioBlockMs = millis();
        const uint32_t activeStartedUs = micros();

        if (RTAL_RAW_INPUT_DIAGNOSTICS_ENABLED)
            analyzeRawInput(gAudioBlock, RTAL_AUDIO_BLOCK_FRAMES);

        if (++peakCountdown >= RTAL_AUDIO_PEAK_BLOCK_DECIMATION)
        {
            peakCountdown = 0;
            updatePeak(gAudioBlock, RTAL_AUDIO_BLOCK_FRAMES * RTAL_AUDIO_CHANNELS);
        }

        if (RTAL_AUDIO_FORMAT_ANALYSIS_ENABLED)
        {
            constexpr size_t analysisStride =
                RTAL_AUDIO_BLOCK_FRAMES / RTAL_AUDIO_ANALYSIS_FRAMES_PER_BLOCK;
            for (size_t i = 0; i < RTAL_AUDIO_ANALYSIS_FRAMES_PER_BLOCK; ++i)
            {
                const size_t srcFrame = i * analysisStride;
                analysisSamples[i * 2] = gAudioBlock[srcFrame * 2];
                analysisSamples[i * 2 + 1] = gAudioBlock[srcFrame * 2 + 1];
            }
            RTALAudioAnalyzer::processInterleavedStereo32(
                analysisSamples, RTAL_AUDIO_ANALYSIS_FRAMES_PER_BLOCK);
        }

        const uint32_t preUs = RTAL_PEAK_FORENSICS_ENABLED
            ? (micros() - activeStartedUs) : 0;
        const uint32_t processingStartedUs = micros();
        
        
        if (RTAL_SINGLE_PASS_DSP_ENABLED)
        {
            RTALDSPKernel::processBlock(gAudioBlock, RTAL_AUDIO_BLOCK_FRAMES);
        }


        const uint32_t processingUs = micros() - processingStartedUs;

        size_t transmittedBytes = 0;
        const uint32_t txStartedUs = RTAL_PEAK_FORENSICS_ENABLED ? micros() : 0;
        const bool writeOk = RTALI2SDriver::writeBlock(
            gAudioBlock, sizeof(gAudioBlock), transmittedBytes,
            pdMS_TO_TICKS(RTAL_I2S_IO_TIMEOUT_MS));
        const uint32_t txUs = RTAL_PEAK_FORENSICS_ENABLED
            ? (micros() - txStartedUs) : 0;

        const uint32_t activeUs = micros() - activeStartedUs;
        const uint32_t budgetUs = blockBudgetUs();

        portENTER_CRITICAL(&statisticsMux_);
        if (processingUs > budgetUs)
        {
            ++statistics_.processingDeadlineMisses;
            const uint32_t overrunUs = processingUs - budgetUs;
            if (overrunUs > statistics_.maximumProcessingOverrunUs)
                statistics_.maximumProcessingOverrunUs = overrunUs;
        }
        if (activeUs > budgetUs) ++statistics_.activeDeadlineMisses;
        statistics_.lastActiveUs = activeUs;
        statistics_.accumulatedActiveUs += activeUs;
        if (activeUs > statistics_.maximumActiveUs)
            statistics_.maximumActiveUs = activeUs;

        if (RTAL_PEAK_FORENSICS_ENABLED)
        {
            if (activeUs > RTAL_PEAK_HIST_2600_US) ++statistics_.activePeakGt2600;
            if (activeUs > RTAL_PEAK_HIST_2700_US) ++statistics_.activePeakGt2700;
            if (activeUs > RTAL_PEAK_HIST_2800_US) ++statistics_.activePeakGt2800;
            if (activeUs > budgetUs) ++statistics_.activePeakGtBudget;
            if (activeUs > RTAL_PEAK_HIST_3000_US) ++statistics_.activePeakGt3000;
            if (activeUs > RTAL_PEAK_HIST_3200_US) ++statistics_.activePeakGt3200;

            if (activeUs >= RTAL_PEAK_FORENSICS_TRIGGER_US)
            {
                RTALActivePeakEvent& e = statistics_.activePeakEvents[statistics_.activePeakEventCount % RTAL_PEAK_FORENSICS_RING_SIZE];
                const uint32_t accountedUs = preUs + processingUs + txUs;
                e.audioBlock = statistics_.audioBlocks + 1;
                e.activeUs = activeUs;
                e.processingUs = processingUs;
                e.preUs = preUs;
                e.txUs = txUs;
                e.otherUs = activeUs > accountedUs ? activeUs - accountedUs : 0;
                ++statistics_.activePeakEventCount;
                ++statistics_.activePeakEventTotal;
            }
        }

        if (writeOk)
        {
            ++statistics_.audioBlocks;
            statistics_.lastProcessingUs = processingUs;
            statistics_.accumulatedProcessingUs += processingUs;
            if (processingUs > statistics_.maximumProcessingUs)
                statistics_.maximumProcessingUs = processingUs;
        }
        else ++statistics_.failedAudioBlocks;
        portEXIT_CRITICAL(&statisticsMux_);
    }
}

RTALAudioStatistics RTALAudioEngine::statistics()
{
    portENTER_CRITICAL(&statisticsMux_);
    const RTALAudioStatistics copy = statistics_;
    statistics_.peakAbsolute = 0;
    statistics_.activePeakGt2600 = 0;
    statistics_.activePeakGt2700 = 0;
    statistics_.activePeakGt2800 = 0;
    statistics_.activePeakGtBudget = 0;
    statistics_.activePeakGt3000 = 0;
    statistics_.activePeakGt3200 = 0;
    statistics_.activePeakEventCount = 0;
    statistics_.activePeakEventTotal = 0;
    memset(statistics_.activePeakEvents, 0, sizeof(statistics_.activePeakEvents));
    portEXIT_CRITICAL(&statisticsMux_);
    return copy;
}
