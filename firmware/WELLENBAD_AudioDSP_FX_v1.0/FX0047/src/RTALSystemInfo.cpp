#include "../include/RTALSystemInfo.h"
#include "../include/RTALLogger.h"
#include "../include/RTALConfig.h"
#include "../include/RTALVersion.h"
#include <esp_heap_caps.h>
#include <esp_idf_version.h>

RTALMemorySnapshot RTALSystemInfo::memory()
{
    RTALMemorySnapshot m{};

    m.internalFree =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m.internalMinimumFree =
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m.internalLargestBlock =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    m.psramTotal = ESP.getPsramSize();
    m.psramFree =
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    m.psramLargestBlock =
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    return m;
}

RTALStatus RTALSystemInfo::selfTest()
{
    const RTALMemorySnapshot m = memory();

    if (m.internalFree < RTAL_MIN_FREE_INTERNAL_HEAP_BYTES)
        return RTALStatus::ERROR;

    if (m.psramTotal == 0 || m.psramTotal < RTAL_MIN_PSRAM_BYTES)
        return RTALStatus::WARNING;

    return RTALStatus::OK;
}

void RTALSystemInfo::printBootReport()
{
    const RTALMemorySnapshot m = memory();

    Serial.println("======================================================");
    Serial.printf(" %s\r\n", RTAL_PLATFORM_NAME);
    Serial.println("======================================================");
    Serial.printf("Firmware : %s\r\n", RTAL_VERSION_STRING);
    Serial.printf("Build    : %s\r\n", RTAL_BUILD_STRING);
    Serial.printf("Compiled : %s %s\r\n", __DATE__, __TIME__);
    Serial.println();
    Serial.printf("Chip     : %s rev. %u\r\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("Cores    : %u\r\n", ESP.getChipCores());
    Serial.printf("CPU      : %u MHz\r\n", getCpuFrequencyMhz());
    Serial.printf("Flash    : %u bytes\r\n", ESP.getFlashChipSize());
    Serial.printf("ESP-IDF  : %s\r\n", esp_get_idf_version());
    Serial.println();
    Serial.printf("Internal free    : %u bytes\r\n", m.internalFree);
    Serial.printf("Internal minimum : %u bytes\r\n", m.internalMinimumFree);
    Serial.printf("Internal largest : %u bytes\r\n", m.internalLargestBlock);
    Serial.printf("PSRAM total      : %u bytes\r\n", m.psramTotal);
    Serial.printf("PSRAM free       : %u bytes\r\n", m.psramFree);
    Serial.printf("PSRAM largest    : %u bytes\r\n", m.psramLargestBlock);
    Serial.println();
}

void RTALSystemInfo::printRuntimeReport()
{
    const RTALMemorySnapshot m = memory();

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Memory internal_free=%u internal_min=%u internal_largest=%u",
        m.internalFree,
        m.internalMinimumFree,
        m.internalLargestBlock);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Memory psram_free=%u psram_largest=%u uptime=%lu ms",
        m.psramFree,
        m.psramLargestBlock,
        static_cast<unsigned long>(millis()));
}
