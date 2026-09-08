#include "../include/RTALDelayNvsPresets.h"
#include "../include/RTALLogger.h"
#include <Preferences.h>
#include <string.h>

namespace
{
    constexpr const char* kNamespace = "rtal-dly";
    constexpr uint32_t kMagic = 0x5254414CUL;
}

uint8_t RTALDelayNvsPresets::bootImportedCount_ = 0;
uint8_t RTALDelayNvsPresets::bootInvalidCount_ = 0;

RTALStatus RTALDelayNvsPresets::begin()
{
    bootImportedCount_ = 0;
    bootInvalidCount_ = 0;

    if (RTAL_DELAY_NVS_BOOT_IMPORT_ENABLED)
        bootImportedCount_ = importAllToRam();

    RTALLogger::printf(
        RTALLogLevel::Info,
        "DelayNVSPresets PASS slots=%u format=%u complete=YES boot_import=%s imported=%u invalid=%u",
        static_cast<unsigned>(
            RTAL_DELAY_NVS_PRESET_COUNT),
        static_cast<unsigned>(
            RTAL_DELAY_NVS_FORMAT_VERSION),
        RTAL_DELAY_NVS_BOOT_IMPORT_ENABLED
            ? "ENABLED"
            : "DISABLED",
        static_cast<unsigned>(bootImportedCount_),
        static_cast<unsigned>(bootInvalidCount_));

    return RTALStatus::OK;
}

bool RTALDelayNvsPresets::validSlot(uint8_t slot)
{
    return slot >= 1 &&
           slot <= RTAL_DELAY_NVS_PRESET_COUNT;
}

void RTALDelayNvsPresets::makeKey(
    uint8_t slot,
    char* key,
    size_t keySize)
{
    snprintf(
        key,
        keySize,
        "p%u",
        static_cast<unsigned>(slot));
}

uint32_t RTALDelayNvsPresets::checksum(
    const RTALDelayNvsRecord& record)
{
    RTALDelayNvsRecord copy = record;
    copy.checksum = 0;

    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(&copy);

    uint32_t hash = 2166136261UL;

    for (size_t i = 0; i < sizeof(copy); ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }

    return hash;
}

RTALDelayPresetData
RTALDelayNvsPresets::dataFromRecord(
    const RTALDelayNvsRecord& record)
{
    RTALDelayPresetData data = {};

    data.parameters.enabled = record.enabled != 0;
    data.parameters.freeze = record.freeze != 0;
    data.parameters.pingPong = record.pingPong != 0;
    data.parameters.crossfeed = record.crossfeed;
    data.parameters.duckAmount = record.duckAmount;
    data.parameters.duckThresholdDb = record.duckThresholdDb;
    data.parameters.duckReleaseMs = record.duckReleaseMs;
    data.parameters.timeLeftMs = record.timeLeftMs;
    data.parameters.timeRightMs = record.timeRightMs;
    data.parameters.feedback = record.feedback;
    data.parameters.level = record.level;
    data.parameters.feedbackHighpassHz =
        record.feedbackHighpassHz;
    data.parameters.feedbackLowpassHz =
        record.feedbackLowpassHz;
    data.parameters.feedbackSaturation =
        record.feedbackSaturation;

    data.clockMode =
        static_cast<RTALDelayClockMode>(
            record.clockMode);

    data.leftDivision =
        static_cast<RTALClockDivision>(
            record.leftDivision);

    data.rightDivision =
        static_cast<RTALClockDivision>(
            record.rightDivision);

    data.metadataVersion = record.metadataVersion;
    data.origin =
        static_cast<RTALPresetOrigin>(
            record.origin);

    memcpy(
        data.name,
        record.name,
        sizeof(data.name));

    data.name[RTAL_DELAY_PRESET_NAME_LENGTH] =
        '\0';

    return data;
}

RTALDelayNvsRecord
RTALDelayNvsPresets::recordFromData(
    const RTALDelayPresetData& data)
{
    RTALDelayNvsRecord record = {};

    record.magic = kMagic;
    record.formatVersion =
        RTAL_DELAY_NVS_FORMAT_VERSION;
    record.recordSize = sizeof(record);
    record.occupied = 1;

    record.enabled =
        data.parameters.enabled ? 1 : 0;
    record.freeze =
        data.parameters.freeze ? 1 : 0;
    record.pingPong =
        data.parameters.crossfeed >= 0.999f ? 1 : 0;
    record.crossfeed = data.parameters.crossfeed;
    record.duckAmount = data.parameters.duckAmount;
    record.duckThresholdDb = data.parameters.duckThresholdDb;
    record.duckReleaseMs = data.parameters.duckReleaseMs;
    record.clockMode =
        static_cast<uint8_t>(data.clockMode);
    record.leftDivision =
        static_cast<uint8_t>(data.leftDivision);
    record.rightDivision =
        static_cast<uint8_t>(data.rightDivision);
    record.origin =
        static_cast<uint8_t>(data.origin);
    record.metadataVersion =
        data.metadataVersion;

    memcpy(
        record.name,
        data.name,
        sizeof(record.name));

    record.name[RTAL_DELAY_PRESET_NAME_LENGTH] =
        '\0';

    record.timeLeftMs =
        data.parameters.timeLeftMs;
    record.timeRightMs =
        data.parameters.timeRightMs;
    record.feedback =
        data.parameters.feedback;
    record.level =
        data.parameters.level;
    record.feedbackHighpassHz =
        data.parameters.feedbackHighpassHz;
    record.feedbackLowpassHz =
        data.parameters.feedbackLowpassHz;
    record.feedbackSaturation =
        data.parameters.feedbackSaturation;

    record.checksum = checksum(record);
    return record;
}

bool RTALDelayNvsPresets::validateRecord(
    const RTALDelayNvsRecord& record)
{
    if (record.magic != kMagic)
        return false;

    if (record.formatVersion !=
        RTAL_DELAY_NVS_FORMAT_VERSION)
        return false;

    if (record.recordSize != sizeof(record))
        return false;

    if (record.occupied != 1)
        return false;

    if (record.enabled > 1 ||
        record.freeze > 1 ||
        record.pingPong > 1)
        return false;

    if (record.name[RTAL_DELAY_PRESET_NAME_LENGTH] !=
        '\0')
        return false;

    if (record.checksum != checksum(record))
        return false;

    return RTALDelayRamPresets::validate(
        dataFromRecord(record));
}

bool RTALDelayNvsPresets::readRecord(
    uint8_t slot,
    RTALDelayNvsRecord& record)
{
    if (!validSlot(slot))
        return false;

    char key[8];
    makeKey(slot, key, sizeof(key));

    Preferences preferences;

    if (!preferences.begin(kNamespace, true))
        return false;

    const size_t length =
        preferences.getBytesLength(key);

    if (length != sizeof(record))
    {
        preferences.end();
        return false;
    }

    const size_t read =
        preferences.getBytes(
            key,
            &record,
            sizeof(record));

    preferences.end();

    return read == sizeof(record) &&
           validateRecord(record);
}

bool RTALDelayNvsPresets::saveFromRam(uint8_t slot)
{
    if (!validSlot(slot))
        return false;

    RTALDelayPresetData data = {};

    if (!RTALDelayRamPresets::exportSlot(
            slot,
            data))
        return false;

    if (!RTALDelayRamPresets::validate(data))
        return false;

    const RTALDelayNvsRecord record =
        recordFromData(data);

    char key[8];
    makeKey(slot, key, sizeof(key));

    Preferences preferences;

    if (!preferences.begin(kNamespace, false))
        return false;

    const size_t written =
        preferences.putBytes(
            key,
            &record,
            sizeof(record));

    preferences.end();

    if (written != sizeof(record))
        return false;

    RTALDelayNvsRecord verification = {};
    return readRecord(slot, verification);
}

bool RTALDelayNvsPresets::loadToRam(uint8_t slot)
{
    RTALDelayNvsRecord record = {};

    if (!readRecord(slot, record))
        return false;

    return RTALDelayRamPresets::importSlot(
        slot,
        dataFromRecord(record));
}

bool RTALDelayNvsPresets::erase(uint8_t slot)
{
    if (!validSlot(slot))
        return false;

    char key[8];
    makeKey(slot, key, sizeof(key));

    Preferences preferences;

    if (!preferences.begin(kNamespace, false))
        return false;

    const bool existed = preferences.isKey(key);
    const bool removed =
        existed && preferences.remove(key);

    preferences.end();
    return removed;
}


uint8_t RTALDelayNvsPresets::importAllToRam()
{
    uint8_t imported = 0;
    bootInvalidCount_ = 0;

    for (uint8_t slot = 1;
         slot <= RTAL_DELAY_NVS_PRESET_COUNT;
         ++slot)
    {
        RTALDelayNvsRecord record = {};

        // readRecord performs exact size, version, magic, checksum and
        // parameter validation. Missing slots are treated as empty.
        if (!readRecord(slot, record))
        {
            char key[8];
            makeKey(slot, key, sizeof(key));

            Preferences preferences;
            if (preferences.begin(kNamespace, true))
            {
                const size_t length =
                    preferences.getBytesLength(key);

                if (length > 0)
                    ++bootInvalidCount_;

                preferences.end();
            }

            continue;
        }

        if (RTALDelayRamPresets::importSlot(
                slot,
                dataFromRecord(record)))
        {
            ++imported;
        }
        else
        {
            ++bootInvalidCount_;
        }
    }

    return imported;
}

uint8_t RTALDelayNvsPresets::bootImportedCount()
{
    return bootImportedCount_;
}

uint8_t RTALDelayNvsPresets::bootInvalidCount()
{
    return bootInvalidCount_;
}

void RTALDelayNvsPresets::list(Stream& output)
{
    output.println("NVS complete delay presets");

    for (uint8_t slot = 1;
         slot <= RTAL_DELAY_NVS_PRESET_COUNT;
         ++slot)
    {
        RTALDelayNvsRecord record = {};

        output.print(slot);
        output.print(": ");

        if (!readRecord(slot, record))
        {
            output.println("EMPTY/INVALID/OLD_FORMAT");
            continue;
        }

        const RTALDelayPresetData data =
            dataFromRecord(record);

        const RTALStereoDelayParameters& p =
            data.parameters;

        output.print('"');
        output.print(data.name);
        output.print("\" ");
        output.print(
            RTALDelayRamPresets::originName(
                data.origin));
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

    output.print("Boot import: ");
    output.print(bootImportedCount_);
    output.print(" imported, ");
    output.print(bootInvalidCount_);
    output.println(" invalid.");
    output.println(
        "NVS presets are imported to RAM at boot but never applied automatically.");
}
