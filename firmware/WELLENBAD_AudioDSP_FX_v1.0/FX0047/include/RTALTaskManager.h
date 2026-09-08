#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

class RTALTaskManager
{
public:
    static RTALStatus begin();

private:
    static void systemTask(void* parameter);

    static TaskHandle_t systemTaskHandle_;
    static int8_t registryId_;
};
