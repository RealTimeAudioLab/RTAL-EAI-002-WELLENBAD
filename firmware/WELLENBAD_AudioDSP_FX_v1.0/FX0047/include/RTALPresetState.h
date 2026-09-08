#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

enum class RTALPresetSource : uint8_t
{
    Startup = 0,
    Serial,
    Midi,
    Unknown
};

enum class RTALPresetStateValue : uint8_t
{
    None = 0,
    Transition,
    Active,
    Modified,
    Failed
};

struct RTALPresetStateSnapshot
{
    uint8_t activeSlot;
    uint8_t requestedSlot;
    uint8_t lastMidiProgram;
    RTALPresetSource source;
    RTALPresetStateValue state;
    uint32_t loadCount;
    uint32_t modifiedCount;
    uint32_t commitCount;
    uint32_t failedCount;
};

class RTALPresetState
{
public:
    static RTALStatus begin();
    static void transitionRequested(uint8_t slot, RTALPresetSource source);
    static void activated(uint8_t slot, RTALPresetSource source);
    static void loadFailed(uint8_t slot, RTALPresetSource source);
    static void markModified();
    static void committed(uint8_t slot, RTALPresetSource source);
    static void setLastMidiProgram(uint8_t displayedProgram);
    static RTALPresetStateSnapshot snapshot();
    static void print(Stream& output);
    static void printReport();
    static const char* sourceName(RTALPresetSource source);
    static const char* stateName(RTALPresetStateValue state);

private:
    static portMUX_TYPE mux_;
    static RTALPresetStateSnapshot state_;
};
