#pragma once
#include <Arduino.h>

enum class RTALLogLevel : uint8_t
{
    Debug = 0,
    Info,
    Warning,
    Error,
    Silent
};

class RTALLogger
{
public:
    static void begin(uint32_t baud);
    static void setLevel(RTALLogLevel level);

    static void info(const char* message);
    static void warning(const char* message);
    static void error(const char* message);

    static void printf(RTALLogLevel level, const char* format, ...)
        __attribute__((format(printf, 2, 3)));

private:
    static void printLine(RTALLogLevel level, const char* message);
    static const char* levelTag(RTALLogLevel level);

    static RTALLogLevel level_;
    static SemaphoreHandle_t mutex_;
};
