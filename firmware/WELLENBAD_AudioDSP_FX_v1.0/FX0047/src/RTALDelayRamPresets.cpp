#include "../include/RTALDelayRamPresets.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include <math.h>
#include <string.h>

RTALDelayRamPresetSlot
RTALDelayRamPresets::slots_[RTAL_DELAY_RAM_PRESET_COUNT] = {};

bool RTALDelayRamPresets::validSlot(uint8_t slot)
{
    return slot >= 1 && slot <= RTAL_DELAY_RAM_PRESET_COUNT;
}

bool RTALDelayRamPresets::validName(const char* name)
{
    if (!name || !name[0])
        return false;

    const size_t length = strlen(name);

    if (length == 0 ||
        length > RTAL_DELAY_PRESET_NAME_LENGTH)
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        const uint8_t character =
            static_cast<uint8_t>(name[i]);

        if (character < 32 || character > 126)
            return false;
    }

    return true;
}

void RTALDelayRamPresets::makeDefaultName(
    uint8_t slot,
    char* destination,
    size_t destinationSize)
{
    snprintf(
        destination,
        destinationSize,
        "Preset %u",
        static_cast<unsigned>(slot));
}

const char* RTALDelayRamPresets::originName(
    RTALPresetOrigin origin)
{
    switch (origin)
    {
        case RTALPresetOrigin::User:
            return "USER";

        case RTALPresetOrigin::Factory:
            return "FACTORY";
    }

    return "?";
}

bool RTALDelayRamPresets::validate(
    const RTALDelayPresetData& data)
{
    const RTALStereoDelayParameters& p = data.parameters;

    if (!isfinite(p.timeLeftMs) ||
        !isfinite(p.timeRightMs) ||
        !isfinite(p.feedback) ||
        !isfinite(p.level) ||
        !isfinite(p.feedbackLowpassHz) ||
        !isfinite(p.feedbackHighpassHz) ||
        !isfinite(p.feedbackSaturation) ||
        !isfinite(p.crossfeed) ||
        !isfinite(p.duckAmount) ||
        !isfinite(p.duckThresholdDb) ||
        !isfinite(p.duckReleaseMs))
        return false;

    if (p.timeLeftMs < 1.0f ||
        p.timeLeftMs > RTAL_DELAY_MAX_MS)
        return false;

    if (p.timeRightMs < 1.0f ||
        p.timeRightMs > RTAL_DELAY_MAX_MS)
        return false;

    if (p.feedback < 0.0f || p.feedback > 0.92f)
        return false;

    if (p.level < 0.0f || p.level > 1.0f)
        return false;

    if (p.feedbackLowpassHz <
            RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MIN ||
        p.feedbackLowpassHz >
            RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MAX)
        return false;

    if (p.feedbackHighpassHz <
            RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MIN ||
        p.feedbackHighpassHz >
            RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MAX)
        return false;

    if (p.feedbackSaturation <
            RTAL_DELAY_FEEDBACK_SATURATION_MIN ||
        p.feedbackSaturation >
            RTAL_DELAY_FEEDBACK_SATURATION_MAX)
        return false;

    if (p.crossfeed < RTAL_DELAY_CROSSFEED_MIN ||
        p.crossfeed > RTAL_DELAY_CROSSFEED_MAX)
        return false;

    if (p.duckAmount < RTAL_DELAY_DUCK_AMOUNT_MIN || p.duckAmount > RTAL_DELAY_DUCK_AMOUNT_MAX) return false;
    if (p.duckThresholdDb < RTAL_DELAY_DUCK_THRESHOLD_DB_MIN || p.duckThresholdDb > RTAL_DELAY_DUCK_THRESHOLD_DB_MAX) return false;
    if (p.duckReleaseMs < RTAL_DELAY_DUCK_RELEASE_MS_MIN || p.duckReleaseMs > RTAL_DELAY_DUCK_RELEASE_MS_MAX) return false;

    if (data.clockMode != RTALDelayClockMode::Free &&
        data.clockMode != RTALDelayClockMode::Sync)
        return false;

    const uint8_t maxDivision =
        static_cast<uint8_t>(
            RTALClockDivision::SixteenthTriplet);

    if (static_cast<uint8_t>(data.leftDivision) >
            maxDivision ||
        static_cast<uint8_t>(data.rightDivision) >
            maxDivision)
        return false;

    if (data.metadataVersion !=
        RTAL_DELAY_PRESET_METADATA_VERSION)
        return false;

    if (data.origin != RTALPresetOrigin::User &&
        data.origin != RTALPresetOrigin::Factory)
        return false;

    if (!validName(data.name))
        return false;

    return data.name[RTAL_DELAY_PRESET_NAME_LENGTH] == '\0';
}

RTALStatus RTALDelayRamPresets::begin()
{
    clearAll();

    RTALLogger::printf(
        RTALLogLevel::Info,
        "DelayRAMPresets PASS slots=%u metadata=%u names=%u volatile=YES",
        static_cast<unsigned>(
            RTAL_DELAY_RAM_PRESET_COUNT),
        static_cast<unsigned>(
            RTAL_DELAY_PRESET_METADATA_VERSION),
        static_cast<unsigned>(
            RTAL_DELAY_PRESET_NAME_LENGTH));

    return RTALStatus::OK;
}

bool RTALDelayRamPresets::save(uint8_t slot)
{
    if (!validSlot(slot))
        return false;

    RTALDelayPresetData data = {};
    data.parameters = RTALStereoDelay::parameters();
    data.clockMode = RTALMidiClock::delayMode();
    data.leftDivision = RTALMidiClock::leftDivision();
    data.rightDivision = RTALMidiClock::rightDivision();
    data.metadataVersion =
        RTAL_DELAY_PRESET_METADATA_VERSION;
    data.origin = RTALPresetOrigin::User;

    // Preserve an existing name when overwriting a slot.
    if (slots_[slot - 1].occupied &&
        validName(slots_[slot - 1].data.name))
    {
        strncpy(
            data.name,
            slots_[slot - 1].data.name,
            RTAL_DELAY_PRESET_NAME_LENGTH);
        data.name[RTAL_DELAY_PRESET_NAME_LENGTH] = '\0';
        data.origin = slots_[slot - 1].data.origin;
    }
    else
    {
        makeDefaultName(
            slot,
            data.name,
            sizeof(data.name));
    }

    if (!validate(data))
        return false;

    slots_[slot - 1].data = data;
    slots_[slot - 1].occupied = true;
    return true;
}

bool RTALDelayRamPresets::apply(
    const RTALDelayPresetData& data)
{
    if (!validate(data))
        return false;

    const RTALStereoDelayParameters& p = data.parameters;

    RTALMidiClock::setDelayMode(
        RTALDelayClockMode::Free);

    RTALStereoDelay::setEnabled(false);
    RTALStereoDelay::setFreeze(p.freeze);
    RTALStereoDelay::setCrossfeed(p.crossfeed);
    RTALStereoDelay::setDuckAmount(p.duckAmount);
    RTALStereoDelay::setDuckThresholdDb(p.duckThresholdDb);
    RTALStereoDelay::setDuckReleaseMs(p.duckReleaseMs);
    RTALStereoDelay::setTimeLeftMs(p.timeLeftMs);
    RTALStereoDelay::setTimeRightMs(p.timeRightMs);
    RTALStereoDelay::setFeedback(p.feedback);
    RTALStereoDelay::setFeedbackHighpassHz(
        p.feedbackHighpassHz);
    RTALStereoDelay::setFeedbackSaturation(
        p.feedbackSaturation);
    RTALStereoDelay::setFeedbackLowpassHz(
        p.feedbackLowpassHz);
    RTALStereoDelay::setLevel(p.level);

    if (!RTALMidiClock::setLeftDivision(
            data.leftDivision))
        return false;

    if (!RTALMidiClock::setRightDivision(
            data.rightDivision))
        return false;

    RTALMidiClock::setDelayMode(data.clockMode);
    RTALStereoDelay::setEnabled(p.enabled);
    return true;
}

bool RTALDelayRamPresets::load(uint8_t slot)
{
    if (!validSlot(slot) ||
        !slots_[slot - 1].occupied)
        return false;

    return apply(slots_[slot - 1].data);
}

bool RTALDelayRamPresets::erase(uint8_t slot)
{
    if (!validSlot(slot))
        return false;

    slots_[slot - 1] = {};
    return true;
}

bool RTALDelayRamPresets::setName(
    uint8_t slot,
    const char* name)
{
    if (!validSlot(slot) ||
        !slots_[slot - 1].occupied ||
        !validName(name))
        return false;

    strncpy(
        slots_[slot - 1].data.name,
        name,
        RTAL_DELAY_PRESET_NAME_LENGTH);

    slots_[slot - 1]
        .data
        .name[RTAL_DELAY_PRESET_NAME_LENGTH] = '\0';

    slots_[slot - 1].data.metadataVersion =
        RTAL_DELAY_PRESET_METADATA_VERSION;

    return validate(slots_[slot - 1].data);
}

bool RTALDelayRamPresets::setOrigin(
    uint8_t slot,
    RTALPresetOrigin origin)
{
    if (!validSlot(slot) ||
        !slots_[slot - 1].occupied)
        return false;

    if (origin != RTALPresetOrigin::User &&
        origin != RTALPresetOrigin::Factory)
        return false;

    slots_[slot - 1].data.origin = origin;
    return validate(slots_[slot - 1].data);
}

bool RTALDelayRamPresets::exportSlot(
    uint8_t slot,
    RTALDelayPresetData& data)
{
    if (!validSlot(slot) ||
        !slots_[slot - 1].occupied)
        return false;

    data = slots_[slot - 1].data;
    return validate(data);
}

bool RTALDelayRamPresets::importSlot(
    uint8_t slot,
    const RTALDelayPresetData& data)
{
    if (!validSlot(slot) || !validate(data))
        return false;

    slots_[slot - 1].data = data;
    slots_[slot - 1].occupied = true;
    return true;
}

void RTALDelayRamPresets::clearAll()
{
    memset(slots_, 0, sizeof(slots_));
}

bool RTALDelayRamPresets::show(
    uint8_t slot,
    Stream& output)
{
    if (!validSlot(slot) ||
        !slots_[slot - 1].occupied)
        return false;

    const RTALDelayPresetData& data =
        slots_[slot - 1].data;

    const RTALStereoDelayParameters& p =
        data.parameters;

    output.print("Preset ");
    output.println(slot);
    output.print("  Name     : ");
    output.println(data.name);
    output.print("  Category : DELAY");
    output.println();
    output.print("  Origin   : ");
    output.println(originName(data.origin));
    output.print("  Metadata : ");
    output.println(data.metadataVersion);
    output.print("  Enabled  : ");
    output.println(p.enabled ? "ON" : "OFF");
    output.print("  Freeze   : ");
    output.println(p.freeze ? "ON" : "OFF");
    output.print("  Mode     : ");
    output.println(
        data.clockMode == RTALDelayClockMode::Sync
            ? "SYNC"
            : "FREE");
    output.print("  Crossfeed: ");
    output.println(p.crossfeed, 3);

    if (data.clockMode == RTALDelayClockMode::Sync)
    {
        output.print("  Left     : ");
        output.println(
            RTALMidiClock::divisionName(
                data.leftDivision));
        output.print("  Right    : ");
        output.println(
            RTALMidiClock::divisionName(
                data.rightDivision));
    }
    else
    {
        output.print("  Left     : ");
        output.print(p.timeLeftMs, 1);
        output.println(" ms");
        output.print("  Right    : ");
        output.print(p.timeRightMs, 1);
        output.println(" ms");
    }

    output.print("  Feedback : ");
    output.println(p.feedback, 3);
    output.print("  Saturation: ");
    output.println(p.feedbackSaturation, 3);
    output.print("  High-pass: ");
    output.print(p.feedbackHighpassHz, 0);
    output.println(" Hz");
    output.print("  Low-pass : ");
    output.print(p.feedbackLowpassHz, 0);
    output.println(" Hz");
    output.print("  Level    : ");
    output.println(p.level, 3);
    return true;
}

void RTALDelayRamPresets::list(Stream& output)
{
    output.println("RAM delay presets");

    for (uint8_t slot = 1;
         slot <= RTAL_DELAY_RAM_PRESET_COUNT;
         ++slot)
    {
        const RTALDelayRamPresetSlot& item =
            slots_[slot - 1];

        output.print(slot);
        output.print(": ");

        if (!item.occupied)
        {
            output.println("EMPTY");
            continue;
        }

        const RTALDelayPresetData& data =
            item.data;

        const RTALStereoDelayParameters& p =
            data.parameters;

        output.print('"');
        output.print(data.name);
        output.print("\" ");
        output.print(originName(data.origin));
        output.print(" DELAY ");
        output.print(p.enabled ? "ON " : "OFF ");
        output.print(p.freeze ? "FRZ " : "--- ");
        output.print(
            data.clockMode == RTALDelayClockMode::Sync
                ? "SYNC "
                : "FREE ");
        output.print("XF=");
        output.print(p.crossfeed, 2);
        output.print(" ");

        if (data.clockMode == RTALDelayClockMode::Sync)
        {
            output.print(
                RTALMidiClock::divisionName(
                    data.leftDivision));
            output.print("/");
            output.print(
                RTALMidiClock::divisionName(
                    data.rightDivision));
            output.print(" ");
        }
        else
        {
            output.print(p.timeLeftMs, 0);
            output.print("/");
            output.print(p.timeRightMs, 0);
            output.print("ms ");
        }

        output.print("FB=");
        output.print(p.feedback, 2);
        output.print(" SAT=");
        output.print(p.feedbackSaturation, 2);
        output.print(" HPF=");
        output.print(p.feedbackHighpassHz, 0);
        output.print("Hz LPF=");
        output.print(p.feedbackLowpassHz, 0);
        output.print("Hz LEVEL=");
        output.println(p.level, 2);
    }

    output.println(
        "RAM metadata changes require nvs save <slot> for persistence.");
}
