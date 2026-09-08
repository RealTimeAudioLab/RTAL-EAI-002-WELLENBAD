#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"
#include "RTALDelayRamPresets.h"

struct RTALDelayNvsRecord
{
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t recordSize;
    uint32_t checksum;

    uint8_t occupied;
    uint8_t enabled;
    uint8_t freeze;
    uint8_t pingPong;
    uint8_t clockMode;

    uint8_t leftDivision;
    uint8_t rightDivision;
    uint8_t origin;
    uint8_t reserved0;

    uint16_t metadataVersion;
    uint16_t reserved1;

    float timeLeftMs;
    float timeRightMs;
    float feedback;
    float level;
    float feedbackHighpassHz;
    float feedbackLowpassHz;
    float feedbackSaturation;
    float crossfeed;
    float duckAmount;
    float duckThresholdDb;
    float duckReleaseMs;

    char name[RTAL_DELAY_PRESET_NAME_LENGTH + 1];
    uint8_t reserved2[3];
};

class RTALDelayNvsPresets
{
public:
    static RTALStatus begin();
    static bool saveFromRam(uint8_t slot);
    static bool loadToRam(uint8_t slot);
    static bool erase(uint8_t slot);
    static uint8_t importAllToRam();
    static uint8_t bootImportedCount();
    static uint8_t bootInvalidCount();
    static void list(Stream& output);

private:
    static bool validSlot(uint8_t slot);
    static bool validateRecord(
        const RTALDelayNvsRecord& record);
    static uint32_t checksum(
        const RTALDelayNvsRecord& record);
    static void makeKey(
        uint8_t slot,
        char* key,
        size_t keySize);
    static bool readRecord(
        uint8_t slot,
        RTALDelayNvsRecord& record);
    static RTALDelayPresetData dataFromRecord(
        const RTALDelayNvsRecord& record);
    static RTALDelayNvsRecord recordFromData(
        const RTALDelayPresetData& data);

    static uint8_t bootImportedCount_;
    static uint8_t bootInvalidCount_;
};
