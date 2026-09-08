#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

enum class RTALTaskState : uint8_t
{
    Starting = 0,
    Running,
    WaitClock,
    Warning,
    Timeout,
    Recovered,
    Stopped
};

struct RTALTaskRecord
{
    const char* name;
    TaskHandle_t handle;
    BaseType_t core;
    UBaseType_t priority;
    uint32_t timeoutMs;
    volatile uint32_t lastAliveMs;
    volatile uint32_t aliveCount;
    volatile RTALTaskState state;
    volatile bool monitorEnabled;
    volatile bool stackWarningIssued;
};

class RTALTaskRegistry
{
public:
    static RTALStatus begin();

    static int8_t registerTask(
        const char* name,
        TaskHandle_t handle,
        BaseType_t core,
        UBaseType_t priority,
        uint32_t timeoutMs,
        bool monitorEnabled);

    static void heartbeat(int8_t taskId);
    static bool isTimedOut(size_t index, uint32_t nowMs);
    static void setState(size_t index, RTALTaskState state);
    static void setMonitorEnabled(size_t index, bool enabled);
    static void inspectStacks();
    static void printReport();

    static size_t count();
    static const RTALTaskRecord* get(size_t index);
    static const char* stateName(RTALTaskState state);

private:
    static RTALTaskRecord tasks_[];
    static size_t count_;
    static SemaphoreHandle_t mutex_;
};
