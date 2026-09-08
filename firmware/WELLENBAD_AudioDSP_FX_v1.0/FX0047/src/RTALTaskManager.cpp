#include "../include/RTALTaskManager.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALSystemInfo.h"
#include "../include/RTALTaskRegistry.h"
#include "../include/RTALWatchdog.h"
#include "../include/RTALAudioEngine.h"
#include "../include/RTALI2SDriver.h"
#include "../include/RTALEventLog.h"
#include "../include/RTALAudioAnalyzer.h"
#include "../include/RTALDSPKernel.h"
#include "../include/RTALStereoDelay.h"
#include "../include/RTALStereoReverb.h"
#include "../include/RTALSerialControl.h"
#include "../include/RTALDelayRamPresets.h"
#include "../include/RTALDelayNvsPresets.h"
#include "../include/RTALMidiControl.h"
#include "../include/RTALMidiClock.h"
#include "../include/RTALDelayPresetTransition.h"
#include "../include/RTALPresetState.h"
#include "../include/RTALStartupPreset.h"
#include "../include/RTALPresetCompare.h"
#include "../include/RTALFxLink.h"
#include <limits.h>

TaskHandle_t RTALTaskManager::systemTaskHandle_ = nullptr;
int8_t RTALTaskManager::registryId_ = -1;

RTALStatus RTALTaskManager::begin()
{
    if (RTALDelayRamPresets::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALDelayNvsPresets::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALSerialControl::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALMidiControl::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALMidiClock::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALDelayPresetTransition::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALPresetState::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALStartupPreset::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALPresetCompare::begin() != RTALStatus::OK) return RTALStatus::FATAL;
    if (RTALTaskRegistry::begin() != RTALStatus::OK)
        return RTALStatus::FATAL;

    const BaseType_t result = xTaskCreatePinnedToCore(
        systemTask,
        "RTALSystem",
        RTAL_SYSTEM_TASK_STACK_WORDS,
        nullptr,
        RTAL_SYSTEM_TASK_PRIORITY,
        &systemTaskHandle_,
        RTAL_SYSTEM_TASK_CORE);

    if (result != pdPASS || !systemTaskHandle_)
        return RTALStatus::FATAL;

    registryId_ = RTALTaskRegistry::registerTask(
        "RTALSystem",
        systemTaskHandle_,
        RTAL_SYSTEM_TASK_CORE,
        RTAL_SYSTEM_TASK_PRIORITY,
        RTAL_SYSTEM_TASK_TIMEOUT_MS,
        true);

    if (registryId_ < 0) return RTALStatus::FATAL;
    if (RTALWatchdog::begin() == RTALStatus::FATAL) return RTALStatus::FATAL;
    if (RTALAudioEngine::begin() == RTALStatus::FATAL) return RTALStatus::FATAL;
    if (RTALFxLink::begin() == RTALStatus::FATAL) return RTALStatus::FATAL;

    RTALLogger::info("TaskManager .... PASS");
    return RTALStatus::OK;
}

void RTALTaskManager::systemTask(void* parameter)
{
    (void)parameter;

    TickType_t heartbeatWake = xTaskGetTickCount();
    TickType_t reportWake = heartbeatWake;

    for (;;)
    {
        const TickType_t now = xTaskGetTickCount();

        if ((now - heartbeatWake) >=
            pdMS_TO_TICKS(RTAL_SYSTEM_HEARTBEAT_INTERVAL_MS))
        {
            heartbeatWake = now;
            RTALTaskRegistry::heartbeat(registryId_);
        }

        if ((now - reportWake) >=
            pdMS_TO_TICKS(RTAL_SYSTEM_REPORT_INTERVAL_MS))
        {
            reportWake = now;

            RTALSystemInfo::printRuntimeReport();
            RTALTaskRegistry::printReport();

            const RTALI2SStatistics i2s = RTALI2SDriver::statistics();
            const RTALAudioStatistics audio = RTALAudioEngine::statistics();

            const uint32_t averageUs = audio.audioBlocks > 0
                ? static_cast<uint32_t>(audio.accumulatedProcessingUs / audio.audioBlocks)
                : 0;
            const float load = RTALAudioEngine::blockBudgetUs() > 0
                ? (100.0f * averageUs) / RTALAudioEngine::blockBudgetUs()
                : 0.0f;
            const uint32_t averageActiveUs = audio.audioBlocks > 0
                ? static_cast<uint32_t>(audio.accumulatedActiveUs / audio.audioBlocks)
                : 0;

            RTALLogger::printf(
                RTALLogLevel::Info,
                "Audio clock=%s audio_blocks=%lu failed=%lu peak=%ld proc_avg=%lu us proc_max=%lu us budget=%lu us load=%.2f%%",
                RTALAudioEngine::clockStateName(audio.clockState),
                static_cast<unsigned long>(audio.audioBlocks),
                static_cast<unsigned long>(audio.failedAudioBlocks),
                static_cast<long>(audio.peakAbsolute),
                static_cast<unsigned long>(averageUs),
                static_cast<unsigned long>(audio.maximumProcessingUs),
                static_cast<unsigned long>(RTALAudioEngine::blockBudgetUs()),
                load);

            RTALLogger::printf(
                RTALLogLevel::Info,
                "Audio deadline proc_miss=%lu proc_over_max=%lu us active_miss=%lu active_avg=%lu us active_max=%lu us",
                static_cast<unsigned long>(audio.processingDeadlineMisses),
                static_cast<unsigned long>(audio.maximumProcessingOverrunUs),
                static_cast<unsigned long>(audio.activeDeadlineMisses),
                static_cast<unsigned long>(averageActiveUs),
                static_cast<unsigned long>(audio.maximumActiveUs));

            if (RTAL_PEAK_FORENSICS_ENABLED)
            {
                RTALLogger::printf(
                    RTALLogLevel::Info,
                    "PeakHist ACTIVE >2600=%lu >2700=%lu >2800=%lu >budget=%lu >3000=%lu >3200=%lu events=%lu",
                    static_cast<unsigned long>(audio.activePeakGt2600),
                    static_cast<unsigned long>(audio.activePeakGt2700),
                    static_cast<unsigned long>(audio.activePeakGt2800),
                    static_cast<unsigned long>(audio.activePeakGtBudget),
                    static_cast<unsigned long>(audio.activePeakGt3000),
                    static_cast<unsigned long>(audio.activePeakGt3200),
                    static_cast<unsigned long>(audio.activePeakEventTotal));

                const uint32_t available = audio.activePeakEventCount < RTAL_PEAK_FORENSICS_RING_SIZE
                    ? audio.activePeakEventCount : RTAL_PEAK_FORENSICS_RING_SIZE;
                const uint32_t first = audio.activePeakEventCount > available
                    ? audio.activePeakEventCount - available : 0;
                const uint32_t show = available > 4 ? 4 : available;
                for (uint32_t j = available - show; j < available; ++j)
                {
                    const uint32_t logical = first + j;
                    const RTALActivePeakEvent& e = audio.activePeakEvents[logical % RTAL_PEAK_FORENSICS_RING_SIZE];
                    RTALLogger::printf(
                        RTALLogLevel::Info,
                        "PeakA #%lu active=%lu process=%lu pre=%lu tx=%lu other=%lu us",
                        static_cast<unsigned long>(e.audioBlock),
                        static_cast<unsigned long>(e.activeUs),
                        static_cast<unsigned long>(e.processingUs),
                        static_cast<unsigned long>(e.preUs),
                        static_cast<unsigned long>(e.txUs),
                        static_cast<unsigned long>(e.otherUs));
                }
            }

            if (RTAL_RAW_INPUT_DIAGNOSTICS_ENABLED)
            {
                RTALLogger::printf(
                    RTALLogLevel::Info,
                    "RawFreeze events=%lu current=%lu max=%lu frames value=%ld/%ld identical=%llu repeat_events=%lu repeat_current=%lu repeat_max=%lu blocks",
                    static_cast<unsigned long>(audio.rawFreezeEvents),
                    static_cast<unsigned long>(audio.rawFreezeCurrentFrames),
                    static_cast<unsigned long>(audio.rawFreezeMaximumFrames),
                    static_cast<long>(audio.rawFreezeLeft),
                    static_cast<long>(audio.rawFreezeRight),
                    static_cast<unsigned long long>(audio.rawIdenticalFrames),
                    static_cast<unsigned long>(audio.rawRepeatEvents),
                    static_cast<unsigned long>(audio.rawRepeatCurrentBlocks),
                    static_cast<unsigned long>(audio.rawRepeatMaximumBlocks));
            }
            else
            {
                RTALLogger::printf(
                    RTALLogLevel::Info,
                    "RawFreeze scanner=OFF realtime_optimization=ON format_sample=%lu/%lu_frames peak_decim=1/%lu",
                    static_cast<unsigned long>(RTAL_AUDIO_ANALYSIS_FRAMES_PER_BLOCK),
                    static_cast<unsigned long>(RTAL_AUDIO_BLOCK_FRAMES),
                    static_cast<unsigned long>(RTAL_AUDIO_PEAK_BLOCK_DECIMATION));
            }

            RTALLogger::printf(
                RTALLogLevel::Info,
                "Clock probe_full=%lu probe_partial=%lu rejected_timing=%lu transitions_present=%lu transitions_lost=%lu",
                static_cast<unsigned long>(audio.probeFullBlocks),
                static_cast<unsigned long>(audio.probePartialReads),
                static_cast<unsigned long>(audio.rejectedTimingBlocks),
                static_cast<unsigned long>(audio.clockPresentTransitions),
                static_cast<unsigned long>(audio.clockLostTransitions));

            RTALLogger::printf(
                RTALLogLevel::Info,
                "I2S read=%lu write=%lu no_clock_read=%lu no_clock_write=%lu read_err=%lu write_err=%lu",
                static_cast<unsigned long>(i2s.readCalls),
                static_cast<unsigned long>(i2s.writeCalls),
                static_cast<unsigned long>(i2s.noClockReads),
                static_cast<unsigned long>(i2s.noClockWrites),
                static_cast<unsigned long>(i2s.readErrors),
                static_cast<unsigned long>(i2s.writeErrors));

            const uint32_t readWaitAvg = i2s.readWaitSamples
                ? static_cast<uint32_t>(i2s.accumulatedReadWaitUs / i2s.readWaitSamples)
                : 0;
            const uint32_t writeWaitAvg = i2s.writeWaitSamples
                ? static_cast<uint32_t>(i2s.accumulatedWriteWaitUs / i2s.writeWaitSamples)
                : 0;

            RTALLogger::printf(
                RTALLogLevel::Info,
                "I2S RX last=%luB short=%lu zero=%lu timeout=%lu wait_us avg=%lu min=%lu max=%lu",
                static_cast<unsigned long>(i2s.lastReadBytes),
                static_cast<unsigned long>(i2s.shortReads),
                static_cast<unsigned long>(i2s.zeroReads),
                static_cast<unsigned long>(i2s.readTimeouts),
                static_cast<unsigned long>(readWaitAvg),
                static_cast<unsigned long>(i2s.minimumReadWaitUs == UINT32_MAX ? 0 : i2s.minimumReadWaitUs),
                static_cast<unsigned long>(i2s.maximumReadWaitUs));

            RTALLogger::printf(
                RTALLogLevel::Info,
                "I2S TX last=%luB short=%lu zero=%lu timeout=%lu wait_us avg=%lu min=%lu max=%lu",
                static_cast<unsigned long>(i2s.lastWriteBytes),
                static_cast<unsigned long>(i2s.shortWrites),
                static_cast<unsigned long>(i2s.zeroWrites),
                static_cast<unsigned long>(i2s.writeTimeouts),
                static_cast<unsigned long>(writeWaitAvg),
                static_cast<unsigned long>(i2s.minimumWriteWaitUs == UINT32_MAX ? 0 : i2s.minimumWriteWaitUs),
                static_cast<unsigned long>(i2s.maximumWriteWaitUs));

            RTALLogger::printf(
                RTALLogLevel::Info,
                "Watchdog timeouts=%lu recoveries=%lu",
                static_cast<unsigned long>(RTALWatchdog::timeoutCount()),
                static_cast<unsigned long>(RTALWatchdog::recoveryCount()));

            RTALEventLog::printCompactReport();

            if (RTAL_AUDIO_FORMAT_ANALYSIS_ENABLED)
                RTALAudioAnalyzer::printReport();

            if (RTAL_SINGLE_PASS_DSP_ENABLED)
                RTALDSPKernel::printReport();

            if (RTAL_STEREO_DELAY_ENABLED)
                RTALStereoDelay::printReport();

            if (RTAL_STEREO_REVERB_ENABLED)
                RTALStereoReverb::printReport();

            if (RTAL_MIDI_CONTROL_ENABLED)
                RTALMidiControl::printReport();

            if (RTAL_MIDI_CLOCK_SYNC_ENABLED)
                RTALMidiClock::printReport();

            if (RTAL_FX_LINK_ENABLED)
                RTALFxLink::printReport();

            if (RTAL_DELAY_SMOOTH_PROGRAM_CHANGE_ENABLED)
                RTALDelayPresetTransition::printReport();

            RTALPresetState::printReport();
            RTALStartupPreset::printReport();
}

        RTALSerialControl::service();
        RTALMidiControl::service();
        RTALFxLink::service();
        RTALMidiClock::service();
        RTALDelayPresetTransition::service();
        RTALStartupPreset::service();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
