#include "../include/RTALStatus.h"

const char* rtalStatusToString(RTALStatus status)
{
    switch (status)
    {
        case RTALStatus::OK: return "OK";
        case RTALStatus::WARNING: return "WARNING";
        case RTALStatus::ERROR: return "ERROR";
        case RTALStatus::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}
