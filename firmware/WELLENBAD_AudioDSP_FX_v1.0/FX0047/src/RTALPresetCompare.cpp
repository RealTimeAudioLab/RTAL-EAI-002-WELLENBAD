#include "../include/RTALPresetCompare.h"
#include "../include/RTALDelayPresetTransition.h"
#include "../include/RTALDelayRamPresets.h"
#include "../include/RTALLogger.h"
#include "../include/RTALMidiClock.h"
#include "../include/RTALPresetState.h"
#include "../include/RTALStereoDelay.h"
#include <math.h>

RTALStatus RTALPresetCompare::begin()
{
    RTALLogger::printf(
        RTALLogLevel::Info,
        "PresetCompare .. PASS compare=YES revert=SMOOTH commit=RAM_ONLY");
    return RTALStatus::OK;
}

bool RTALPresetCompare::different(float stored, float current, float tolerance)
{
    return fabsf(stored - current) > tolerance;
}

void RTALPresetCompare::printBooleanDifference(
    Stream& output, const char* label, bool stored, bool current,
    uint8_t& differenceCount)
{
    if (stored == current) return;
    ++differenceCount;
    output.print(label);
    output.print(" : stored=");
    output.print(stored ? "ON" : "OFF");
    output.print(" current=");
    output.println(current ? "ON" : "OFF");
}

void RTALPresetCompare::printFloatDifference(
    Stream& output, const char* label, float stored, float current,
    float tolerance, uint8_t decimals, const char* suffix,
    uint8_t& differenceCount)
{
    if (!different(stored, current, tolerance)) return;
    ++differenceCount;
    output.print(label);
    output.print(" : stored=");
    output.print(stored, decimals);
    if (suffix && suffix[0]) output.print(suffix);
    output.print(" current=");
    output.print(current, decimals);
    if (suffix && suffix[0]) output.print(suffix);
    output.println();
}

RTALPresetOperationResult RTALPresetCompare::compare(Stream& output)
{
    if (RTALDelayPresetTransition::busy())
        return RTALPresetOperationResult::TransitionBusy;

    const RTALPresetStateSnapshot state = RTALPresetState::snapshot();
    if (state.activeSlot == 0)
        return RTALPresetOperationResult::NoActivePreset;

    RTALDelayPresetData stored = {};
    if (!RTALDelayRamPresets::exportSlot(state.activeSlot, stored))
        return RTALPresetOperationResult::PresetUnavailable;

    const RTALStereoDelayParameters current = RTALStereoDelay::parameters();
    const RTALDelayClockMode currentMode = RTALMidiClock::delayMode();
    const RTALClockDivision currentLeft = RTALMidiClock::leftDivision();
    const RTALClockDivision currentRight = RTALMidiClock::rightDivision();

    output.print("Preset ");
    output.print(state.activeSlot);
    output.print(" \"");
    output.print(stored.name);
    output.println("\" compare");

    uint8_t differences = 0;

    printBooleanDifference(output, "Enabled",
        stored.parameters.enabled, current.enabled, differences);
    printBooleanDifference(output, "Freeze",
        stored.parameters.freeze, current.freeze, differences);
    printFloatDifference(output, "Crossfeed",
        stored.parameters.crossfeed, current.crossfeed,
        0.0005f, 3, "", differences);
    printFloatDifference(output, "Duck amount",
        stored.parameters.duckAmount, current.duckAmount,
        0.0005f, 3, "", differences);
    printFloatDifference(output, "Duck threshold",
        stored.parameters.duckThresholdDb, current.duckThresholdDb,
        0.05f, 1, " dB", differences);
    printFloatDifference(output, "Duck release",
        stored.parameters.duckReleaseMs, current.duckReleaseMs,
        0.5f, 0, " ms", differences);

    if (stored.clockMode != currentMode)
    {
        ++differences;
        output.print("Mode : stored=");
        output.print(stored.clockMode == RTALDelayClockMode::Sync ? "SYNC" : "FREE");
        output.print(" current=");
        output.println(currentMode == RTALDelayClockMode::Sync ? "SYNC" : "FREE");
    }
    else if (currentMode == RTALDelayClockMode::Free)
    {
        printFloatDifference(output, "Left",
            stored.parameters.timeLeftMs, current.timeLeftMs,
            0.05f, 1, " ms", differences);
        printFloatDifference(output, "Right",
            stored.parameters.timeRightMs, current.timeRightMs,
            0.05f, 1, " ms", differences);
    }
    else
    {
        if (stored.leftDivision != currentLeft)
        {
            ++differences;
            output.print("Left division : stored=");
            output.print(RTALMidiClock::divisionName(stored.leftDivision));
            output.print(" current=");
            output.println(RTALMidiClock::divisionName(currentLeft));
        }
        if (stored.rightDivision != currentRight)
        {
            ++differences;
            output.print("Right division: stored=");
            output.print(RTALMidiClock::divisionName(stored.rightDivision));
            output.print(" current=");
            output.println(RTALMidiClock::divisionName(currentRight));
        }
    }

    printFloatDifference(output, "Feedback",
        stored.parameters.feedback, current.feedback,
        0.0005f, 3, "", differences);
    printFloatDifference(output, "Saturation",
        stored.parameters.feedbackSaturation, current.feedbackSaturation,
        0.0005f, 3, "", differences);
    printFloatDifference(output, "High-pass",
        stored.parameters.feedbackHighpassHz, current.feedbackHighpassHz,
        0.5f, 0, " Hz", differences);
    printFloatDifference(output, "Low-pass",
        stored.parameters.feedbackLowpassHz, current.feedbackLowpassHz,
        0.5f, 0, " Hz", differences);
    printFloatDifference(output, "Level",
        stored.parameters.level, current.level,
        0.0005f, 3, "", differences);

    if (differences == 0) output.println("No differences.");
    else
    {
        output.print("Differences=");
        output.println(differences);
    }

    return RTALPresetOperationResult::Ok;
}

RTALPresetOperationResult RTALPresetCompare::revert()
{
    if (RTALDelayPresetTransition::busy())
        return RTALPresetOperationResult::TransitionBusy;

    const RTALPresetStateSnapshot state = RTALPresetState::snapshot();
    if (state.activeSlot == 0)
        return RTALPresetOperationResult::NoActivePreset;

    RTALDelayPresetData stored = {};
    if (!RTALDelayRamPresets::exportSlot(state.activeSlot, stored))
        return RTALPresetOperationResult::PresetUnavailable;

    if (!RTALDelayPresetTransition::request(
            state.activeSlot, RTALPresetSource::Serial))
        return RTALPresetOperationResult::Failed;

    return RTALPresetOperationResult::Ok;
}

RTALPresetOperationResult RTALPresetCompare::commit()
{
    if (RTALDelayPresetTransition::busy())
        return RTALPresetOperationResult::TransitionBusy;

    const RTALPresetStateSnapshot state = RTALPresetState::snapshot();
    if (state.activeSlot == 0)
        return RTALPresetOperationResult::NoActivePreset;

    RTALDelayPresetData stored = {};
    if (!RTALDelayRamPresets::exportSlot(state.activeSlot, stored))
        return RTALPresetOperationResult::PresetUnavailable;

    if (!RTALDelayRamPresets::save(state.activeSlot))
        return RTALPresetOperationResult::Failed;

    RTALPresetState::committed(state.activeSlot, RTALPresetSource::Serial);
    return RTALPresetOperationResult::Ok;
}

const char* RTALPresetCompare::resultName(RTALPresetOperationResult result)
{
    switch (result)
    {
        case RTALPresetOperationResult::Ok: return "OK";
        case RTALPresetOperationResult::NoActivePreset: return "NO_ACTIVE_PRESET";
        case RTALPresetOperationResult::PresetUnavailable: return "PRESET_UNAVAILABLE";
        case RTALPresetOperationResult::TransitionBusy: return "TRANSITION_BUSY";
        case RTALPresetOperationResult::Failed: return "FAILED";
    }
    return "?";
}
