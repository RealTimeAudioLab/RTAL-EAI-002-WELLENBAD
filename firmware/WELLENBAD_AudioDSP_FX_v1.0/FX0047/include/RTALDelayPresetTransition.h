#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"
#include "RTALDelayRamPresets.h"
#include "RTALPresetState.h"

enum class RTALDelayPresetTransitionState : uint8_t
{
    Idle = 0,
    FadeOut,
    Apply,
    Settle,
    FadeIn
};

struct RTALDelayPresetTransitionStatistics
{
    uint32_t requests;
    uint32_t completed;
    uint32_t rejected;
    uint32_t replaced;
    uint8_t activeSlot;
    uint8_t lastCompletedSlot;
    RTALDelayPresetTransitionState state;
};

class RTALDelayPresetTransition
{
public:
    static RTALStatus begin();
    static bool request(
        uint8_t slot,
        RTALPresetSource source = RTALPresetSource::Midi);
    static void service();
    static bool busy();
    static RTALDelayPresetTransitionStatistics statistics(
        bool resetWindow);
    static void printReport();

private:
    static bool applyMuted(const RTALDelayPresetData& data);
    static const char* stateName(
        RTALDelayPresetTransitionState state);

    static portMUX_TYPE mux_;
    static RTALDelayPresetTransitionState state_;
    static RTALDelayPresetData pending_;
    static uint8_t pendingSlot_;
    static uint32_t deadlineMs_;
    static float restoreLevel_;
    static bool restoreEnabled_;
    static RTALPresetSource pendingSource_;
    static RTALDelayPresetTransitionStatistics statistics_;
};
