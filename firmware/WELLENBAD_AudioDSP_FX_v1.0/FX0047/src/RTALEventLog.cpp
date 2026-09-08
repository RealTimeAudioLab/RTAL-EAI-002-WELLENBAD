#include "../include/RTALEventLog.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"

RTALEvent RTALEventLog::events_[RTAL_EVENT_LOG_CAPACITY] = {};
size_t RTALEventLog::writeIndex_ = 0;
size_t RTALEventLog::count_ = 0;
uint32_t RTALEventLog::dropped_ = 0;
SemaphoreHandle_t RTALEventLog::mutex_ = nullptr;

RTALStatus RTALEventLog::begin()
{
    mutex_ = xSemaphoreCreateMutex();
    return mutex_ ? RTALStatus::OK : RTALStatus::FATAL;
}

void RTALEventLog::add(RTALStatus severity, const char* source, const char* message)
{
    if (!mutex_) return;

    xSemaphoreTake(mutex_, portMAX_DELAY);

    RTALEvent& event = events_[writeIndex_];
    event.timestampMs = millis();
    event.severity = severity;
    strlcpy(event.source, source ? source : "Unknown", sizeof(event.source));
    strlcpy(event.message, message ? message : "", sizeof(event.message));

    writeIndex_ = (writeIndex_ + 1) % RTAL_EVENT_LOG_CAPACITY;
    if (count_ < RTAL_EVENT_LOG_CAPACITY) ++count_;
    else ++dropped_;

    xSemaphoreGive(mutex_);
}

size_t RTALEventLog::count() { return count_; }
uint32_t RTALEventLog::dropped() { return dropped_; }

bool RTALEventLog::latest(RTALEvent& event)
{
    if (!mutex_ || count_ == 0) return false;

    xSemaphoreTake(mutex_, portMAX_DELAY);
    const size_t index =
        (writeIndex_ + RTAL_EVENT_LOG_CAPACITY - 1) % RTAL_EVENT_LOG_CAPACITY;
    event = events_[index];
    xSemaphoreGive(mutex_);
    return true;
}

void RTALEventLog::printCompactReport()
{
    RTALEvent event{};

    if (!latest(event))
    {
        RTALLogger::info("Events count=0 dropped=0");
        return;
    }

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Events count=%u dropped=%lu latest=%s/%s at=%lu ms",
        static_cast<unsigned>(count_),
        static_cast<unsigned long>(dropped_),
        event.source,
        event.message,
        static_cast<unsigned long>(event.timestampMs));
}
