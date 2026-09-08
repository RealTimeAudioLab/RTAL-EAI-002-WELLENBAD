#include "../include/RTALLogger.h"
#include <stdarg.h>

RTALLogLevel RTALLogger::level_ = RTALLogLevel::Info;
SemaphoreHandle_t RTALLogger::mutex_ = nullptr;

void RTALLogger::begin(uint32_t baud)
{
    Serial.begin(baud);
    mutex_ = xSemaphoreCreateMutex();
}

void RTALLogger::setLevel(RTALLogLevel level)
{
    level_ = level;
}

const char* RTALLogger::levelTag(RTALLogLevel level)
{
    switch (level)
    {
        case RTALLogLevel::Info: return "INFO ";
        case RTALLogLevel::Warning: return "WARN ";
        case RTALLogLevel::Error: return "ERROR";
        default: return "DEBUG";
    }
}

void RTALLogger::printLine(RTALLogLevel level, const char* message)
{
    if (level < level_ || level_ == RTALLogLevel::Silent) return;

    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    Serial.printf("[%s] %s\r\n", levelTag(level), message ? message : "");
    if (mutex_) xSemaphoreGive(mutex_);
}

void RTALLogger::info(const char* message) { printLine(RTALLogLevel::Info, message); }
void RTALLogger::warning(const char* message) { printLine(RTALLogLevel::Warning, message); }
void RTALLogger::error(const char* message) { printLine(RTALLogLevel::Error, message); }

void RTALLogger::printf(RTALLogLevel level, const char* format, ...)
{
    if (level < level_ || level_ == RTALLogLevel::Silent || !format) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    printLine(level, buffer);
}
