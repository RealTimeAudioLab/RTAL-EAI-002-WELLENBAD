#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"

struct RTALStartupPresetState
{
    bool enabled;
    bool pending;
    bool attempted;
    bool queued;
    uint8_t slot;
    uint32_t applyAtMs;
};

class RTALStartupPreset
{
public:
    static RTALStatus begin();
    static void service();
    static bool setSlot(uint8_t slot);
    static bool disable();
    static RTALStartupPresetState state();
    static void print(Stream& output);
    static void printReport();
private:
    static bool loadConfiguration();
    static bool saveConfiguration();
    static bool validSlot(uint8_t slot);
    static portMUX_TYPE mux_;
    static RTALStartupPresetState state_;
};
