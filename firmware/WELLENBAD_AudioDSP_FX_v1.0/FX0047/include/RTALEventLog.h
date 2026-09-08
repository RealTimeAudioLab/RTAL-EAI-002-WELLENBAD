#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALEvent
{
    uint32_t timestampMs;
    RTALStatus severity;
    char source[16];
    char message[64];
};

class RTALEventLog
{
public:
    static RTALStatus begin();
    static void add(RTALStatus severity, const char* source, const char* message);
    static size_t count();
    static uint32_t dropped();
    static bool latest(RTALEvent& event);
    static void printCompactReport();

private:
    static RTALEvent events_[];
    static size_t writeIndex_;
    static size_t count_;
    static uint32_t dropped_;
    static SemaphoreHandle_t mutex_;
};
