#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

class RTALWatchdog
{
public:
    static RTALStatus begin();
    static uint32_t timeoutCount();
    static uint32_t recoveryCount();

private:
    static void task(void* parameter);

    static TaskHandle_t taskHandle_;
    static int8_t registryId_;
    static volatile uint32_t timeoutCount_;
    static volatile uint32_t recoveryCount_;
};
