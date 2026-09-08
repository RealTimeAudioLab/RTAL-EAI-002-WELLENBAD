#include "../include/RTALMidiControl.h"
#include "../include/RTALLogger.h"
#include "../include/RTALStereoDelay.h"
#include "../include/RTALMidiClock.h"
#include "../include/RTALDelayRamPresets.h"
#include "../include/RTALDelayPresetTransition.h"
#include "../include/RTALPresetState.h"
#include <string.h>
#include <math.h>

HardwareSerial RTALMidiControl::midiSerial_(2);
uint8_t RTALMidiControl::runningStatus_ = 0;
uint8_t RTALMidiControl::data_[2] = {};
uint8_t RTALMidiControl::dataCount_ = 0;
uint8_t RTALMidiControl::expectedData_ = 0;
RTALMidiControlStatistics RTALMidiControl::statistics_ = {};
portMUX_TYPE RTALMidiControl::mux_ = portMUX_INITIALIZER_UNLOCKED;
uint32_t RTALMidiControl::lastActivityMs_ = 0;

uint8_t RTALMidiControl::pendingCcValue_[128] = {};
uint8_t RTALMidiControl::pendingCcChannel_[128] = {};
bool RTALMidiControl::pendingCc_[128] = {};
uint8_t RTALMidiControl::lastAppliedCcValue_[128] = {};
bool RTALMidiControl::lastAppliedCcValid_[128] = {};

RTALStatus RTALMidiControl::begin()
{
    if (!RTAL_MIDI_CONTROL_ENABLED)
        return RTALStatus::OK;

    runningStatus_ = 0;
    dataCount_ = 0;
    expectedData_ = 0;
    lastActivityMs_ = 0;
    memset(pendingCcValue_, 0, sizeof(pendingCcValue_));
    memset(pendingCcChannel_, 0, sizeof(pendingCcChannel_));
    memset(pendingCc_, 0, sizeof(pendingCc_));
    memset(lastAppliedCcValue_, 0, sizeof(lastAppliedCcValue_));
    memset(lastAppliedCcValid_, 0, sizeof(lastAppliedCcValid_));

    portENTER_CRITICAL(&mux_);
    memset(&statistics_, 0, sizeof(statistics_));
    portEXIT_CRITICAL(&mux_);

    // RX-only MIDI input. TX is deliberately disabled in this build.
    midiSerial_.begin(
        RTAL_MIDI_BAUD,
        SERIAL_8N1,
        RTAL_MIDI_RX_PIN,
        -1);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "MIDIControl .... PASS RX=%d TX=DISABLED baud=%lu channel=%s",
        static_cast<int>(RTAL_MIDI_RX_PIN),
        static_cast<unsigned long>(RTAL_MIDI_BAUD),
        RTAL_MIDI_CHANNEL == 0 ? "OMNI" : "FIXED");

    RTALLogger::printf(
        RTALLogLevel::Info,
        "MIDI CC level=%u feedback=%u left=%u right=%u SAT=%u HPF=%u LPF=%u crossfeed=%u duck_amt=%u duck_thr=%u duck_rel=%u freeze=%u enable=%u",
        RTAL_MIDI_CC_DELAY_LEVEL,
        RTAL_MIDI_CC_DELAY_FEEDBACK,
        RTAL_MIDI_CC_DELAY_LEFT_TIME,
        RTAL_MIDI_CC_DELAY_RIGHT_TIME,
        RTAL_MIDI_CC_DELAY_SATURATION,
        RTAL_MIDI_CC_DELAY_HIGHPASS,
        RTAL_MIDI_CC_DELAY_LOWPASS,
        RTAL_MIDI_CC_DELAY_CROSSFEED,
        RTAL_MIDI_CC_DELAY_DUCK_AMOUNT,
        RTAL_MIDI_CC_DELAY_DUCK_THRESHOLD,
        RTAL_MIDI_CC_DELAY_DUCK_RELEASE,
        RTAL_MIDI_CC_DELAY_FREEZE,
        RTAL_MIDI_CC_DELAY_ENABLE);

    return RTALStatus::OK;
}

uint8_t RTALMidiControl::dataLengthForStatus(uint8_t status)
{
    switch (status & 0xF0)
    {
        case 0xC0:
        case 0xD0:
            return 1;

        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        case 0xE0:
            return 2;

        default:
            return 0;
    }
}

bool RTALMidiControl::acceptsChannel(uint8_t channel)
{
    return RTAL_MIDI_CHANNEL == 0 ||
           channel == RTAL_MIDI_CHANNEL;
}

float RTALMidiControl::mapLinear(
    uint8_t value,
    float minimum,
    float maximum)
{
    const float normalized =
        static_cast<float>(value) / 127.0f;

    return minimum + normalized * (maximum - minimum);
}

void RTALMidiControl::service()
{
    if (!RTAL_MIDI_CONTROL_ENABLED)
        return;

    uint8_t processed = 0;

    while (midiSerial_.available() > 0 &&
           processed < RTAL_MIDI_SERVICE_MAX_BYTES)
    {
        const int input = midiSerial_.read();

        if (input >= 0)
        {
            consumeByte(static_cast<uint8_t>(input));
            ++processed;
        }
        else
        {
            break;
        }
    }

    flushPendingControlChanges();

    portENTER_CRITICAL(&mux_);
    ++statistics_.serviceCalls;

    if (processed > statistics_.maximumBytesPerService)
        statistics_.maximumBytesPerService = processed;

    if (processed >= RTAL_MIDI_SERVICE_MAX_BYTES &&
        midiSerial_.available() > 0)
    {
        ++statistics_.serviceBudgetHits;
    }
    portEXIT_CRITICAL(&mux_);
}

void RTALMidiControl::consumeByte(uint8_t value)
{
    lastActivityMs_ = millis();

    portENTER_CRITICAL(&mux_);
    ++statistics_.receivedBytes;
    portEXIT_CRITICAL(&mux_);

    if (RTAL_MIDI_RAW_BYTE_MONITOR_ENABLED)
    {
        Serial.print("[MIDI BYTE] 0x");
        if (value < 16) Serial.print('0');
        Serial.println(value, HEX);
    }

    // MIDI real-time messages may occur anywhere and do not disturb
    // running status or partially collected channel messages.
    if (value >= 0xF8)
    {
        portENTER_CRITICAL(&mux_);
        ++statistics_.realtimeBytes;
        portEXIT_CRITICAL(&mux_);

        RTALMidiClock::handleRealtime(value);
        return;
    }

    if (value & 0x80)
    {
        // System-common and SysEx are not used by this control layer.
        if (value >= 0xF0)
        {
            runningStatus_ = 0;
            dataCount_ = 0;
            expectedData_ = 0;
            return;
        }

        runningStatus_ = value;
        expectedData_ = dataLengthForStatus(value);
        dataCount_ = 0;
        return;
    }

    if (runningStatus_ == 0 || expectedData_ == 0)
        return;

    data_[dataCount_++] = value & 0x7F;

    if (dataCount_ >= expectedData_)
    {
        const uint8_t data2 =
            expectedData_ == 2 ? data_[1] : 0;

        dispatchChannelMessage(
            runningStatus_,
            data_[0],
            data2);

        // Running status remains active.
        dataCount_ = 0;
    }
}

void RTALMidiControl::dispatchChannelMessage(
    uint8_t status,
    uint8_t data1,
    uint8_t data2)
{
    const uint8_t channel =
        static_cast<uint8_t>((status & 0x0F) + 1);

    portENTER_CRITICAL(&mux_);
    ++statistics_.channelMessages;
    statistics_.lastChannel = channel;
    portEXIT_CRITICAL(&mux_);

    if (!acceptsChannel(channel))
    {
        portENTER_CRITICAL(&mux_);
        ++statistics_.ignoredChannelMessages;
        portEXIT_CRITICAL(&mux_);
        return;
    }

    const uint8_t messageType = status & 0xF0;

    if (messageType == 0xB0)
    {
        queueControlChange(channel, data1, data2);
    }
    else if (messageType == 0xC0 &&
             RTAL_MIDI_PROGRAM_CHANGE_ENABLED)
    {
        handleProgramChange(channel, data1);
    }
}


void RTALMidiControl::handleProgramChange(
    uint8_t channel,
    uint8_t program)
{
    portENTER_CRITICAL(&mux_);
    ++statistics_.programChanges;
    statistics_.lastProgram = program;
    portEXIT_CRITICAL(&mux_);

    RTALPresetState::setLastMidiProgram(static_cast<uint8_t>(program + 1));

    const uint16_t first = RTAL_MIDI_PROGRAM_FIRST;
    const uint16_t end = first + RTAL_MIDI_PROGRAM_COUNT;

    if (program < first || program >= end)
    {
        Serial.print("[MIDI PC] CH");
        Serial.print(channel);
        Serial.print(" PROGRAM=");
        Serial.print(program + 1);
        Serial.println(" -> outside preset range");
        return;
    }

    const uint8_t slot =
        static_cast<uint8_t>(
            program - RTAL_MIDI_PROGRAM_FIRST + 1);

    const bool loaded =
        RTAL_DELAY_SMOOTH_PROGRAM_CHANGE_ENABLED
        ? RTALDelayPresetTransition::request(
            slot,
            RTALPresetSource::Midi)
        : RTALDelayRamPresets::load(slot);

    portENTER_CRITICAL(&mux_);
    if (loaded)
        ++statistics_.loadedPrograms;
    else
        ++statistics_.failedPrograms;
    portEXIT_CRITICAL(&mux_);

    Serial.print(loaded
        ? (RTAL_DELAY_SMOOTH_PROGRAM_CHANGE_ENABLED
            ? "[MIDI PC QUEUED] "
            : "[MIDI PC LOADED] ")
        : "[MIDI PC FAILED] ");

    Serial.print("CH");
    Serial.print(channel);
    Serial.print(" PROGRAM=");
    Serial.print(program + 1);
    Serial.print(" -> RAM preset ");
    Serial.println(slot);

    if (!loaded)
    {
        RTALPresetState::loadFailed(slot, RTALPresetSource::Midi);
        Serial.println(
            "RAM preset is empty; use nvs load <slot> or preset save <slot> first.");
    }
}

void RTALMidiControl::handleControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    queueControlChange(channel, controller, value);
}

void RTALMidiControl::queueControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if (controller >= 128)
        return;

    // Count every received CC, even when several values are collapsed
    // to the newest one before the DSP parameter is updated.
    portENTER_CRITICAL(&mux_);
    ++statistics_.controlChanges;
    statistics_.lastController = controller;
    statistics_.lastValue = value;

    if (pendingCc_[controller])
        ++statistics_.coalescedControlChanges;

    pendingCcValue_[controller] = value;
    pendingCcChannel_[controller] = channel;
    pendingCc_[controller] = true;
    portEXIT_CRITICAL(&mux_);
}

void RTALMidiControl::flushPendingControlChanges()
{
    for (uint16_t controller = 0; controller < 128; ++controller)
    {
        uint8_t value = 0;
        uint8_t channel = 0;
        bool pending = false;

        portENTER_CRITICAL(&mux_);
        pending = pendingCc_[controller];

        if (pending)
        {
            value = pendingCcValue_[controller];
            channel = pendingCcChannel_[controller];
            pendingCc_[controller] = false;
        }
        portEXIT_CRITICAL(&mux_);

        if (!pending)
            continue;

        if (RTAL_MIDI_IGNORE_DUPLICATE_CC_VALUES &&
            lastAppliedCcValid_[controller] &&
            lastAppliedCcValue_[controller] == value)
        {
            portENTER_CRITICAL(&mux_);
            ++statistics_.duplicateControlChanges;
            portEXIT_CRITICAL(&mux_);
            continue;
        }

        lastAppliedCcValue_[controller] = value;
        lastAppliedCcValid_[controller] = true;

        applyControlChange(
            channel,
            static_cast<uint8_t>(controller),
            value);
    }
}

void RTALMidiControl::applyControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    (void)channel;

    bool mapped = true;
    bool applied = true;

    switch (controller)
    {
        case RTAL_MIDI_CC_DELAY_LEVEL:
            RTALStereoDelay::setLevel(
                mapLinear(value, 0.0f, 1.0f));
            break;

        case RTAL_MIDI_CC_DELAY_FEEDBACK:
            RTALStereoDelay::setFeedback(
                mapLinear(value, 0.0f, 0.92f));
            break;

        case RTAL_MIDI_CC_DELAY_LEFT_TIME:
            if (RTALMidiClock::delayMode() ==
                RTALDelayClockMode::Sync)
            {
                RTALMidiClock::setLeftDivisionFromMidi(value);
            }
            else
            {
                RTALStereoDelay::setTimeLeftMs(
                    mapLinear(value, 1.0f, RTAL_DELAY_MAX_MS));
            }
            break;

        case RTAL_MIDI_CC_DELAY_RIGHT_TIME:
            if (RTALMidiClock::delayMode() ==
                RTALDelayClockMode::Sync)
            {
                RTALMidiClock::setRightDivisionFromMidi(value);
            }
            else
            {
                RTALStereoDelay::setTimeRightMs(
                    mapLinear(value, 1.0f, RTAL_DELAY_MAX_MS));
            }
            break;

        case RTAL_MIDI_CC_DELAY_LOWPASS:
            RTALStereoDelay::setFeedbackLowpassHz(
                mapLinear(
                    value,
                    RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MIN,
                    RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MAX));
            break;

        case RTAL_MIDI_CC_DELAY_SATURATION:
            RTALStereoDelay::setFeedbackSaturation(
                mapLinear(value, 0.0f, 1.0f));
            break;

        case RTAL_MIDI_CC_DELAY_HIGHPASS:
        {
            // Precomputed logarithmic CC90 mapping, 20 Hz ... 4000 Hz.
            static const float kHighpassLut[128] = {
                20.000f,20.853f,21.742f,22.669f,23.636f,24.643f,25.693f,26.788f,
                27.930f,29.120f,30.361f,31.655f,33.004f,34.411f,35.878f,37.407f,
                39.001f,40.663f,42.395f,44.201f,46.084f,48.048f,50.095f,52.230f,
                54.455f,56.775f,59.194f,61.716f,64.346f,67.088f,69.946f,72.926f,
                76.031f,79.269f,82.645f,86.165f,89.835f,93.662f,97.652f,101.812f,
                106.150f,110.672f,115.388f,120.304f,125.430f,130.775f,136.348f,142.159f,
                148.217f,154.534f,161.120f,167.987f,175.147f,182.612f,190.395f,198.509f,
                206.969f,215.790f,224.987f,234.576f,244.573f,254.996f,265.864f,277.196f,
                289.011f,301.330f,314.174f,327.566f,341.529f,356.087f,371.266f,387.091f,
                403.590f,420.792f,438.727f,457.426f,476.921f,497.247f,518.439f,540.535f,
                563.572f,587.591f,612.634f,638.744f,665.967f,694.350f,723.943f,754.798f,
                786.967f,820.507f,855.476f,891.936f,929.949f,969.582f,1010.903f,1053.986f,
                1098.905f,1145.740f,1194.572f,1245.485f,1298.568f,1353.915f,1411.621f,1471.788f,
                1534.521f,1599.927f,1668.121f,1739.220f,1813.349f,1890.638f,1971.219f,2055.236f,
                2142.835f,2234.169f,2329.398f,2428.686f,2532.205f,2640.134f,2752.663f,2869.990f,
                2992.318f,3119.862f,3252.843f,3391.493f,3536.054f,3686.778f,3843.926f,4000.000f
            };
            RTALStereoDelay::setFeedbackHighpassHz(
                kHighpassLut[value]);
            break;
        }

        case RTAL_MIDI_CC_DELAY_CROSSFEED:
            RTALStereoDelay::setCrossfeed(
                mapLinear(value, 0.0f, 1.0f));
            break;

        case RTAL_MIDI_CC_DELAY_DUCK_AMOUNT:
            RTALStereoDelay::setDuckAmount(mapLinear(value, 0.0f, 1.0f));
            break;

        case RTAL_MIDI_CC_DELAY_DUCK_THRESHOLD:
            RTALStereoDelay::setDuckThresholdDb(
                mapLinear(value, RTAL_DELAY_DUCK_THRESHOLD_DB_MIN, RTAL_DELAY_DUCK_THRESHOLD_DB_MAX));
            break;

        case RTAL_MIDI_CC_DELAY_DUCK_RELEASE:
            RTALStereoDelay::setDuckReleaseMs(
                mapLinear(value, RTAL_DELAY_DUCK_RELEASE_MS_MIN, RTAL_DELAY_DUCK_RELEASE_MS_MAX));
            break;

        case RTAL_MIDI_CC_DELAY_FREEZE:
            if (value >= RTAL_MIDI_SWITCH_ON_MIN)
                RTALStereoDelay::setFreeze(true);
            else if (value <= RTAL_MIDI_SWITCH_OFF_MAX)
                RTALStereoDelay::setFreeze(false);
            else
                applied = false;
            break;

        case RTAL_MIDI_CC_DELAY_ENABLE:
            if (value >= RTAL_MIDI_SWITCH_ON_MIN)
                RTALStereoDelay::setEnabled(true);
            else if (value <= RTAL_MIDI_SWITCH_OFF_MAX)
                RTALStereoDelay::setEnabled(false);
            else
                applied = false;
            break;

        default:
            mapped = false;
            break;
    }

    if (mapped)
    {
        portENTER_CRITICAL(&mux_);
        ++statistics_.mappedControlChanges;
        portEXIT_CRITICAL(&mux_);
        if (applied)
            RTALPresetState::markModified();
    }

    if (RTAL_MIDI_LIVE_MONITOR_ENABLED)
    {
        Serial.print(mapped ? "[MIDI CC MAPPED] " : "[MIDI CC] ");
        Serial.print("CH");
        Serial.print(channel);
        Serial.print(" CC");
        Serial.print(controller);
        Serial.print(" VALUE=");
        Serial.print(value);

        if (mapped)
        {
            Serial.print(" -> ");

            switch (controller)
            {
                case RTAL_MIDI_CC_DELAY_LEVEL:
                    Serial.print("Delay-Level ");
                    Serial.print(mapLinear(value, 0.0f, 1.0f), 3);
                    break;

                case RTAL_MIDI_CC_DELAY_FEEDBACK:
                    Serial.print("Feedback ");
                    Serial.print(mapLinear(value, 0.0f, 0.92f), 3);
                    break;

                case RTAL_MIDI_CC_DELAY_LEFT_TIME:
                    if (RTALMidiClock::delayMode() ==
                        RTALDelayClockMode::Sync)
                    {
                        Serial.print("Delay-L division ");
                        Serial.print(
                            RTALMidiClock::divisionName(
                                RTALMidiClock::divisionFromMidiValue(value)));
                    }
                    else
                    {
                        Serial.print("Delay-L ");
                        Serial.print(
                            mapLinear(value, 1.0f, RTAL_DELAY_MAX_MS),
                            1);
                        Serial.print(" ms");
                    }
                    break;

                case RTAL_MIDI_CC_DELAY_RIGHT_TIME:
                    if (RTALMidiClock::delayMode() ==
                        RTALDelayClockMode::Sync)
                    {
                        Serial.print("Delay-R division ");
                        Serial.print(
                            RTALMidiClock::divisionName(
                                RTALMidiClock::divisionFromMidiValue(value)));
                    }
                    else
                    {
                        Serial.print("Delay-R ");
                        Serial.print(
                            mapLinear(value, 1.0f, RTAL_DELAY_MAX_MS),
                            1);
                        Serial.print(" ms");
                    }
                    break;

                case RTAL_MIDI_CC_DELAY_LOWPASS:
                    Serial.print("Feedback-LPF ");
                    Serial.print(
                        mapLinear(
                            value,
                            RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MIN,
                            RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MAX),
                        0);
                    Serial.print(" Hz");
                    break;

                case RTAL_MIDI_CC_DELAY_CROSSFEED:
                    Serial.print("Crossfeed ");
                    Serial.print(value);
                    break;

                case RTAL_MIDI_CC_DELAY_ENABLE:
                    Serial.print("Delay ");
                    Serial.print(value >= 64 ? "ON" : "OFF");
                    break;
            }
        }

        Serial.println();
    }
}

bool RTALMidiControl::activityActive()
{
    return lastActivityMs_ != 0 &&
           (millis() - lastActivityMs_) <= RTAL_MIDI_ACTIVITY_HOLD_MS;
}

uint32_t RTALMidiControl::lastActivityMs()
{
    return lastActivityMs_;
}

RTALMidiControlStatistics RTALMidiControl::statistics(
    bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    const RTALMidiControlStatistics copy = statistics_;

    if (resetWindow)
    {
        const uint8_t lastChannel = statistics_.lastChannel;
        const uint8_t lastController = statistics_.lastController;
        const uint8_t lastValue = statistics_.lastValue;
        const uint8_t lastProgram = statistics_.lastProgram;

        memset(&statistics_, 0, sizeof(statistics_));
        statistics_.lastChannel = lastChannel;
        statistics_.lastController = lastController;
        statistics_.lastValue = lastValue;
        statistics_.lastProgram = lastProgram;
    }

    portEXIT_CRITICAL(&mux_);
    return copy;
}

void RTALMidiControl::printReport()
{
    const RTALMidiControlStatistics s = statistics(true);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "MIDI activity=%s bytes=%lu messages=%lu CC=%lu mapped=%lu coalesced=%lu duplicate=%lu svc=%lu budget_hits=%lu svc_max_bytes=%lu PC=%lu loaded=%lu failed=%lu ignored_ch=%lu realtime=%lu last=ch%u cc%u value%u pc%u",
        activityActive() ? "ACTIVE" : "IDLE",
        static_cast<unsigned long>(s.receivedBytes),
        static_cast<unsigned long>(s.channelMessages),
        static_cast<unsigned long>(s.controlChanges),
        static_cast<unsigned long>(s.mappedControlChanges),
        static_cast<unsigned long>(s.coalescedControlChanges),
        static_cast<unsigned long>(s.duplicateControlChanges),
        static_cast<unsigned long>(s.serviceCalls),
        static_cast<unsigned long>(s.serviceBudgetHits),
        static_cast<unsigned long>(s.maximumBytesPerService),
        static_cast<unsigned long>(s.programChanges),
        static_cast<unsigned long>(s.loadedPrograms),
        static_cast<unsigned long>(s.failedPrograms),
        static_cast<unsigned long>(s.ignoredChannelMessages),
        static_cast<unsigned long>(s.realtimeBytes),
        static_cast<unsigned>(s.lastChannel),
        static_cast<unsigned>(s.lastController),
        static_cast<unsigned>(s.lastValue),
        static_cast<unsigned>(s.lastProgram + 1));
}
