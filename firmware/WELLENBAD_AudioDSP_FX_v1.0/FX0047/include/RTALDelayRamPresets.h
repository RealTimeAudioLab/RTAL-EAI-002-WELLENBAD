#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"
#include "RTALStereoDelay.h"
#include "RTALMidiClock.h"

enum class RTALPresetOrigin : uint8_t
{
    User = 0,
    Factory = 1
};

struct RTALDelayPresetData
{
    RTALStereoDelayParameters parameters;
    RTALDelayClockMode clockMode;
    RTALClockDivision leftDivision;
    RTALClockDivision rightDivision;

    uint16_t metadataVersion;
    RTALPresetOrigin origin;
    char name[RTAL_DELAY_PRESET_NAME_LENGTH + 1];
};

struct RTALDelayRamPresetSlot
{
    bool occupied;
    RTALDelayPresetData data;
};

class RTALDelayRamPresets
{
public:
    static RTALStatus begin();
    static bool save(uint8_t slot);
    static bool load(uint8_t slot);
    static bool erase(uint8_t slot);

    static bool setName(uint8_t slot, const char* name);
    static bool setOrigin(uint8_t slot, RTALPresetOrigin origin);
    static bool show(uint8_t slot, Stream& output);

    static bool exportSlot(
        uint8_t slot,
        RTALDelayPresetData& data);

    static bool importSlot(
        uint8_t slot,
        const RTALDelayPresetData& data);

    static bool validate(const RTALDelayPresetData& data);
    static void clearAll();
    static void list(Stream& output);

    static const char* originName(RTALPresetOrigin origin);

private:
    static bool validSlot(uint8_t slot);
    static bool validName(const char* name);
    static void makeDefaultName(
        uint8_t slot,
        char* destination,
        size_t destinationSize);
    static bool apply(const RTALDelayPresetData& data);

    static RTALDelayRamPresetSlot
        slots_[RTAL_DELAY_RAM_PRESET_COUNT];
};
