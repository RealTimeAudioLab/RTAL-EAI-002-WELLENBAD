#include <Arduino.h>
#include "../include/RTALBootManager.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALSystemInfo.h"
#include "../include/RTALTaskManager.h"
#include "../include/RTALEventLog.h"

void RTALBootManager::printResult(const char* component, RTALStatus status)
{
    Serial.printf("%-16s %s\r\n", component, rtalStatusToString(status));
}

RTALStatus RTALBootManager::begin()
{
    RTALLogger::begin(RTAL_LOG_BAUD);

    const uint32_t startedAt = millis();
    while (!Serial && (millis() - startedAt) < RTAL_SERIAL_WAIT_MS)
        delay(10);

    RTALSystemInfo::printBootReport();

    const RTALStatus eventStatus = RTALEventLog::begin();
    printResult("Event log", eventStatus);

    const RTALStatus memoryStatus = RTALSystemInfo::selfTest();
    printResult("Memory test", memoryStatus);
    printResult("Logger", RTALStatus::OK);

    RTALEventLog::add(RTALStatus::OK, "Boot", "Build 0008 started");

    const RTALStatus taskStatus = RTALTaskManager::begin();
    printResult("TaskManager", taskStatus);

    RTALStatus finalStatus = RTALStatus::OK;

    if (eventStatus == RTALStatus::FATAL ||
        taskStatus == RTALStatus::FATAL)
        finalStatus = RTALStatus::FATAL;
    else if (memoryStatus != RTALStatus::OK)
        finalStatus = memoryStatus;

    Serial.println();
    Serial.printf("System status    %s\r\n", rtalStatusToString(finalStatus));
    Serial.println("======================================================");

    if (finalStatus == RTALStatus::FATAL)
    {
        RTALLogger::error("Fatal boot error; system halted");
        while (true) delay(1000);
    }

    RTALLogger::info("Framework ...... READY");
    return finalStatus;
}
