#include "../include/RTALTaskRegistry.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALEventLog.h"

RTALTaskRecord RTALTaskRegistry::tasks_[RTAL_MAX_REGISTERED_TASKS] = {};
size_t RTALTaskRegistry::count_ = 0;
SemaphoreHandle_t RTALTaskRegistry::mutex_ = nullptr;

RTALStatus RTALTaskRegistry::begin()
{
    mutex_ = xSemaphoreCreateMutex();
    count_ = 0;
    return mutex_ ? RTALStatus::OK : RTALStatus::FATAL;
}

int8_t RTALTaskRegistry::registerTask(
    const char* name,
    TaskHandle_t handle,
    BaseType_t core,
    UBaseType_t priority,
    uint32_t timeoutMs,
    bool monitorEnabled)
{
    if (!name || !handle || !mutex_) return -1;

    xSemaphoreTake(mutex_, portMAX_DELAY);

    if (count_ >= RTAL_MAX_REGISTERED_TASKS)
    {
        xSemaphoreGive(mutex_);
        return -1;
    }

    const size_t index = count_++;
    tasks_[index] = {
        name,
        handle,
        core,
        priority,
        timeoutMs,
        millis(),
        0,
        RTALTaskState::Starting,
        monitorEnabled,
        false
    };

    xSemaphoreGive(mutex_);
    return static_cast<int8_t>(index);
}

void RTALTaskRegistry::heartbeat(int8_t taskId)
{
    if (taskId < 0 || static_cast<size_t>(taskId) >= count_) return;

    RTALTaskRecord& task = tasks_[taskId];
    task.lastAliveMs = millis();
    ++task.aliveCount;

    if (task.state == RTALTaskState::Timeout)
        task.state = RTALTaskState::Recovered;
    else if (task.state == RTALTaskState::Starting ||
             task.state == RTALTaskState::Recovered ||
             task.state == RTALTaskState::Warning)
        task.state = RTALTaskState::Running;
}

bool RTALTaskRegistry::isTimedOut(size_t index, uint32_t nowMs)
{
    if (index >= count_) return false;

    const RTALTaskRecord& task = tasks_[index];

    if (!task.monitorEnabled || task.timeoutMs == 0)
        return false;

    return static_cast<uint32_t>(nowMs - task.lastAliveMs) > task.timeoutMs;
}

void RTALTaskRegistry::setState(size_t index, RTALTaskState state)
{
    if (index < count_) tasks_[index].state = state;
}

void RTALTaskRegistry::setMonitorEnabled(size_t index, bool enabled)
{
    if (index >= count_) return;

    tasks_[index].monitorEnabled = enabled;
    tasks_[index].lastAliveMs = millis();
}

const char* RTALTaskRegistry::stateName(RTALTaskState state)
{
    switch (state)
    {
        case RTALTaskState::Starting: return "STARTING";
        case RTALTaskState::Running: return "RUNNING";
        case RTALTaskState::WaitClock: return "WAIT_CLOCK";
        case RTALTaskState::Warning: return "WARNING";
        case RTALTaskState::Timeout: return "TIMEOUT";
        case RTALTaskState::Recovered: return "RECOVERED";
        case RTALTaskState::Stopped: return "STOPPED";
        default: return "UNKNOWN";
    }
}

void RTALTaskRegistry::inspectStacks()
{
    for (size_t i = 0; i < count_; ++i)
    {
        RTALTaskRecord& task = tasks_[i];

        if (!task.handle) continue;

        const UBaseType_t freeWords = uxTaskGetStackHighWaterMark(task.handle);

        if (freeWords < RTAL_STACK_CRITICAL_WORDS && !task.stackWarningIssued)
        {
            task.stackWarningIssued = true;
            RTALLogger::printf(
                RTALLogLevel::Error,
                "Stack critical task=%s free=%u words",
                task.name,
                static_cast<unsigned>(freeWords));
            RTALEventLog::add(RTALStatus::ERROR, task.name, "Stack critical");
        }
        else if (freeWords < RTAL_STACK_WARNING_WORDS && !task.stackWarningIssued)
        {
            task.stackWarningIssued = true;
            RTALLogger::printf(
                RTALLogLevel::Warning,
                "Stack warning task=%s free=%u words",
                task.name,
                static_cast<unsigned>(freeWords));
            RTALEventLog::add(RTALStatus::WARNING, task.name, "Stack warning");
        }
    }
}

void RTALTaskRegistry::printReport()
{
    for (size_t i = 0; i < count_; ++i)
    {
        const RTALTaskRecord& task = tasks_[i];
        const UBaseType_t stackFree =
            task.handle ? uxTaskGetStackHighWaterMark(task.handle) : 0;

        RTALLogger::printf(
            RTALLogLevel::Info,
            "Task %-12s state=%-10s core=%d prio=%u stack_free=%u alive=%lu age=%lu ms monitor=%s",
            task.name,
            stateName(task.state),
            static_cast<int>(task.core),
            static_cast<unsigned>(task.priority),
            static_cast<unsigned>(stackFree),
            static_cast<unsigned long>(task.aliveCount),
            static_cast<unsigned long>(millis() - task.lastAliveMs),
            task.monitorEnabled ? "ON" : "OFF");
    }
}

size_t RTALTaskRegistry::count() { return count_; }

const RTALTaskRecord* RTALTaskRegistry::get(size_t index)
{
    return index < count_ ? &tasks_[index] : nullptr;
}
