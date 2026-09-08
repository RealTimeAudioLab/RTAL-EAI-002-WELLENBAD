#include "../include/RTALWatchdog.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALTaskRegistry.h"
#include "../include/RTALEventLog.h"

TaskHandle_t RTALWatchdog::taskHandle_ = nullptr;
int8_t RTALWatchdog::registryId_ = -1;
volatile uint32_t RTALWatchdog::timeoutCount_ = 0;
volatile uint32_t RTALWatchdog::recoveryCount_ = 0;

RTALStatus RTALWatchdog::begin()
{
    const BaseType_t result = xTaskCreatePinnedToCore(
        task,
        "RTALWatchdog",
        RTAL_WATCHDOG_TASK_STACK_WORDS,
        nullptr,
        RTAL_WATCHDOG_TASK_PRIORITY,
        &taskHandle_,
        RTAL_WATCHDOG_TASK_CORE);

    if (result != pdPASS || !taskHandle_) return RTALStatus::FATAL;

    registryId_ = RTALTaskRegistry::registerTask(
        "RTALWatchdog",
        taskHandle_,
        RTAL_WATCHDOG_TASK_CORE,
        RTAL_WATCHDOG_TASK_PRIORITY,
        0,
        false);

    if (registryId_ < 0) return RTALStatus::ERROR;

    RTALLogger::info("Watchdog ...... PASS");
    return RTALStatus::OK;
}

void RTALWatchdog::task(void* parameter)
{
    (void)parameter;

    for (;;)
    {
        RTALTaskRegistry::heartbeat(registryId_);
        const uint32_t now = millis();

        for (size_t i = 0; i < RTALTaskRegistry::count(); ++i)
        {
            const RTALTaskRecord* record = RTALTaskRegistry::get(i);

            if (!record || !record->monitorEnabled)
                continue;

            const bool timedOut = RTALTaskRegistry::isTimedOut(i, now);

            if (timedOut && record->state != RTALTaskState::Timeout)
            {
                RTALTaskRegistry::setState(i, RTALTaskState::Timeout);
                ++timeoutCount_;

                RTALLogger::printf(
                    RTALLogLevel::Error,
                    "Watchdog timeout task=%s age=%lu ms limit=%lu ms",
                    record->name,
                    static_cast<unsigned long>(now - record->lastAliveMs),
                    static_cast<unsigned long>(record->timeoutMs));

                RTALEventLog::add(
                    RTALStatus::ERROR,
                    record->name,
                    "Task heartbeat timeout");
            }
            else if (!timedOut && record->state == RTALTaskState::Recovered)
            {
                ++recoveryCount_;
                RTALTaskRegistry::setState(i, RTALTaskState::Running);

                RTALLogger::printf(
                    RTALLogLevel::Info,
                    "Watchdog recovery task=%s",
                    record->name);

                RTALEventLog::add(
                    RTALStatus::OK,
                    record->name,
                    "Task recovered");
            }
        }

        RTALTaskRegistry::inspectStacks();
        vTaskDelay(pdMS_TO_TICKS(RTAL_WATCHDOG_CHECK_INTERVAL_MS));
    }
}

uint32_t RTALWatchdog::timeoutCount() { return timeoutCount_; }
uint32_t RTALWatchdog::recoveryCount() { return recoveryCount_; }
