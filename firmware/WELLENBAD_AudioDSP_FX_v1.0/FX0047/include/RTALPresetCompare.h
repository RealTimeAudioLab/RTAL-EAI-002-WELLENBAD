#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

enum class RTALPresetOperationResult : uint8_t
{
    Ok = 0,
    NoActivePreset,
    PresetUnavailable,
    TransitionBusy,
    Failed
};

class RTALPresetCompare
{
public:
    static RTALStatus begin();
    static RTALPresetOperationResult compare(Stream& output);
    static RTALPresetOperationResult revert();
    static RTALPresetOperationResult commit();
    static const char* resultName(RTALPresetOperationResult result);

private:
    static bool different(float stored, float current, float tolerance);
    static void printBooleanDifference(
        Stream& output, const char* label, bool stored, bool current,
        uint8_t& differenceCount);
    static void printFloatDifference(
        Stream& output, const char* label, float stored, float current,
        float tolerance, uint8_t decimals, const char* suffix,
        uint8_t& differenceCount);
};
