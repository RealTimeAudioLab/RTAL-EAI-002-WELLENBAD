#pragma once
#include <stdint.h>

enum class RTALStatus : uint8_t
{
    OK = 0,
    WARNING,
    ERROR,
    FATAL
};

const char* rtalStatusToString(RTALStatus status);
