#include "../include/RTALMidiClock.h"
#include "../include/RTALLogger.h"
#include "../include/RTALStereoDelay.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

portMUX_TYPE RTALMidiClock::mux_ = portMUX_INITIALIZER_UNLOCKED;
bool RTALMidiClock::midiActive_ = false;
bool RTALMidiClock::running_ = false;
bool RTALMidiClock::midiBpmValid_ = false;
bool RTALMidiClock::tapBpmValid_ = false;
RTALClockSource RTALMidiClock::source_ = RTALClockSource::Midi;

RTALDelayClockMode RTALMidiClock::delayMode_ =
    RTAL_DELAY_SYNC_DEFAULT
    ? RTALDelayClockMode::Sync
    : RTALDelayClockMode::Free;

RTALClockDivision RTALMidiClock::leftDivision_ =
    RTALClockDivision::EighthDotted;
RTALClockDivision RTALMidiClock::rightDivision_ =
    RTALClockDivision::Quarter;

uint32_t RTALMidiClock::lastClockUs_ = 0;
uint32_t RTALMidiClock::lastClockMs_ = 0;
uint32_t RTALMidiClock::tickIntervals_[
    RTAL_MIDI_CLOCK_SMOOTHING_TICKS] = {};
uint8_t RTALMidiClock::intervalIndex_ = 0;
uint8_t RTALMidiClock::intervalCount_ = 0;
uint64_t RTALMidiClock::intervalSumUs_ = 0;

float RTALMidiClock::midiBpm_ = 0.0f;
float RTALMidiClock::internalBpm_ =
    RTAL_INTERNAL_CLOCK_BPM_DEFAULT;
float RTALMidiClock::tapBpm_ = 0.0f;
float RTALMidiClock::lastStableMidiBpm_ = 0.0f;
float RTALMidiClock::tempoCandidateBpm_ = 0.0f;
uint8_t RTALMidiClock::tempoCandidateWindows_ = 0;
float RTALMidiClock::lastAppliedLeftMs_ = -1.0f;
float RTALMidiClock::lastAppliedRightMs_ = -1.0f;

uint32_t RTALMidiClock::tapIntervalsMs_[
    RTAL_TAP_TEMPO_INTERVAL_COUNT] = {};
uint8_t RTALMidiClock::tapIntervalIndex_ = 0;
uint8_t RTALMidiClock::tapIntervalCount_ = 0;
uint32_t RTALMidiClock::tapIntervalSumMs_ = 0;
uint32_t RTALMidiClock::lastTapMs_ = 0;
uint8_t RTALMidiClock::tapCount_ = 0;

uint32_t RTALMidiClock::clockTicks_ = 0;
uint32_t RTALMidiClock::startCount_ = 0;
uint32_t RTALMidiClock::continueCount_ = 0;
uint32_t RTALMidiClock::stopCount_ = 0;
bool RTALMidiClock::delayUpdatePending_ = false;

RTALStatus RTALMidiClock::begin()
{
    resetMidiTimingWindow();
    resetTapSequence();

    RTALLogger::printf(
        RTALLogLevel::Info,
        "MIDIClock ...... PASS source=%s mode=%s internal=%.2f left=%s right=%s timeout=%lums",
        sourceName(source_),
        delayMode_ == RTALDelayClockMode::Sync ? "SYNC" : "FREE",
        internalBpm_,
        divisionName(leftDivision_),
        divisionName(rightDivision_),
        static_cast<unsigned long>(
            RTAL_MIDI_CLOCK_LOST_TIMEOUT_MS));

    return RTALStatus::OK;
}

void RTALMidiClock::resetMidiTimingWindow()
{
    portENTER_CRITICAL(&mux_);
    midiActive_ = false;
    midiBpmValid_ = false;
    lastClockUs_ = 0;
    lastClockMs_ = 0;
    intervalIndex_ = 0;
    intervalCount_ = 0;
    intervalSumUs_ = 0;
    midiBpm_ = 0.0f;
    lastStableMidiBpm_ = 0.0f;
    tempoCandidateBpm_ = 0.0f;
    tempoCandidateWindows_ = 0;
    delayUpdatePending_ = false;
    memset(tickIntervals_, 0, sizeof(tickIntervals_));
    portEXIT_CRITICAL(&mux_);
}

void RTALMidiClock::resetTapSequence()
{
    portENTER_CRITICAL(&mux_);
    memset(tapIntervalsMs_, 0, sizeof(tapIntervalsMs_));
    tapIntervalIndex_ = 0;
    tapIntervalCount_ = 0;
    tapIntervalSumMs_ = 0;
    lastTapMs_ = 0;
    tapCount_ = 0;
    portEXIT_CRITICAL(&mux_);
}

void RTALMidiClock::handleRealtime(uint8_t value)
{
    if (!RTAL_MIDI_CLOCK_SYNC_ENABLED)
        return;

    if (value == 0xFA)
    {
        portENTER_CRITICAL(&mux_);
        running_ = true;
        ++startCount_;
        portEXIT_CRITICAL(&mux_);
        return;
    }

    if (value == 0xFB)
    {
        portENTER_CRITICAL(&mux_);
        running_ = true;
        ++continueCount_;
        portEXIT_CRITICAL(&mux_);
        return;
    }

    if (value == 0xFC)
    {
        portENTER_CRITICAL(&mux_);
        running_ = false;
        ++stopCount_;
        portEXIT_CRITICAL(&mux_);
        return;
    }

    if (value != 0xF8)
        return;

    const uint32_t nowUs = micros();
    const uint32_t nowMs = millis();

    portENTER_CRITICAL(&mux_);

    ++clockTicks_;
    midiActive_ = true;
    lastClockMs_ = nowMs;

    if (lastClockUs_ != 0)
    {
        const uint32_t intervalUs = nowUs - lastClockUs_;
        const uint32_t minimumUs =
            static_cast<uint32_t>(
                60000000.0f /
                (RTAL_MIDI_CLOCK_MAX_BPM * 24.0f));
        const uint32_t maximumUs =
            static_cast<uint32_t>(
                60000000.0f /
                (RTAL_MIDI_CLOCK_MIN_BPM * 24.0f));

        if (intervalUs >= minimumUs &&
            intervalUs <= maximumUs)
        {
            if (intervalCount_ <
                RTAL_MIDI_CLOCK_SMOOTHING_TICKS)
            {
                tickIntervals_[intervalIndex_] = intervalUs;
                intervalSumUs_ += intervalUs;
                ++intervalCount_;
            }
            else
            {
                intervalSumUs_ -=
                    tickIntervals_[intervalIndex_];
                tickIntervals_[intervalIndex_] = intervalUs;
                intervalSumUs_ += intervalUs;
            }

            intervalIndex_ =
                (intervalIndex_ + 1) %
                RTAL_MIDI_CLOCK_SMOOTHING_TICKS;

            if (intervalCount_ >= 6)
            {
                const float averageIntervalUs =
                    static_cast<float>(intervalSumUs_) /
                    static_cast<float>(intervalCount_);

                const float measuredBpm =
                    60000000.0f /
                    (averageIntervalUs * 24.0f);

                midiBpmValid_ =
                    measuredBpm >= RTAL_MIDI_CLOCK_MIN_BPM &&
                    measuredBpm <= RTAL_MIDI_CLOCK_MAX_BPM;

                if (midiBpmValid_)
                {
                    const bool beatBoundary =
                        (clockTicks_ % RTAL_MIDI_CLOCK_APPLY_EVERY_TICKS) == 0;

                    // Initial lock is immediate once the timing window is valid.
                    if (lastStableMidiBpm_ <= 0.0f)
                    {
                        midiBpm_ = measuredBpm;
                        lastStableMidiBpm_ = measuredBpm;
                        tempoCandidateBpm_ = 0.0f;
                        tempoCandidateWindows_ = 0;
                        if (source_ == RTALClockSource::Midi && beatBoundary)
                            delayUpdatePending_ = true;
                    }
                    else if (beatBoundary)
                    {
                        const float delta = fabsf(measuredBpm - lastStableMidiBpm_);

                        if (delta < RTAL_MIDI_CLOCK_BPM_HYSTERESIS)
                        {
                            // Normal clock jitter: keep the locked BPM and cancel
                            // any unfinished candidate. No delay-time movement.
                            tempoCandidateBpm_ = 0.0f;
                            tempoCandidateWindows_ = 0;
                        }
                        else
                        {
                            // A real tempo change must remain consistent for several
                            // complete beat windows before it can move the delay.
                            if (tempoCandidateWindows_ == 0 ||
                                fabsf(measuredBpm - tempoCandidateBpm_) >
                                    RTAL_MIDI_CLOCK_TEMPO_CONFIRM_TOLERANCE_BPM)
                            {
                                tempoCandidateBpm_ = measuredBpm;
                                tempoCandidateWindows_ = 1;
                            }
                            else
                            {
                                tempoCandidateBpm_ =
                                    (tempoCandidateBpm_ * tempoCandidateWindows_ + measuredBpm) /
                                    static_cast<float>(tempoCandidateWindows_ + 1);
                                if (tempoCandidateWindows_ < 255)
                                    ++tempoCandidateWindows_;
                            }

                            if (tempoCandidateWindows_ >=
                                RTAL_MIDI_CLOCK_TEMPO_CONFIRM_WINDOWS)
                            {
                                midiBpm_ = tempoCandidateBpm_;
                                lastStableMidiBpm_ = tempoCandidateBpm_;
                                tempoCandidateBpm_ = 0.0f;
                                tempoCandidateWindows_ = 0;
                                if (source_ == RTALClockSource::Midi)
                                    delayUpdatePending_ = true;
                            }
                        }
                    }
                }
            }
        }
    }

    lastClockUs_ = nowUs;
    portEXIT_CRITICAL(&mux_);
}

void RTALMidiClock::effectiveClock(
    bool& active,
    bool& valid,
    float& bpm)
{
    switch (source_)
    {
        case RTALClockSource::Midi:
            active = midiActive_;
            valid = midiBpmValid_;
            bpm = midiBpm_;
            return;

        case RTALClockSource::Internal:
            active = true;
            valid =
                internalBpm_ >= RTAL_INTERNAL_CLOCK_BPM_MIN &&
                internalBpm_ <= RTAL_INTERNAL_CLOCK_BPM_MAX;
            bpm = internalBpm_;
            return;

        case RTALClockSource::Tap:
            active = tapBpmValid_;
            valid = tapBpmValid_;
            bpm = tapBpm_;
            return;
    }

    active = false;
    valid = false;
    bpm = 0.0f;
}

void RTALMidiClock::service()
{
    if (!RTAL_MIDI_CLOCK_SYNC_ENABLED)
        return;

    bool shouldUpdate = false;

    portENTER_CRITICAL(&mux_);

    if (midiActive_ &&
        (millis() - lastClockMs_) >
            RTAL_MIDI_CLOCK_LOST_TIMEOUT_MS)
    {
        midiActive_ = false;
        midiBpmValid_ = false;

        if (source_ == RTALClockSource::Midi)
            delayUpdatePending_ = false;
    }

    bool selectedActive = false;
    bool selectedValid = false;
    float selectedBpm = 0.0f;
    effectiveClock(selectedActive, selectedValid, selectedBpm);

    shouldUpdate =
        delayMode_ == RTALDelayClockMode::Sync &&
        selectedActive &&
        selectedValid &&
        delayUpdatePending_;

    if (shouldUpdate)
        delayUpdatePending_ = false;

    portEXIT_CRITICAL(&mux_);

    if (shouldUpdate)
        updateDelayTimes(false);
}

void RTALMidiClock::setDelayMode(RTALDelayClockMode mode)
{
    portENTER_CRITICAL(&mux_);
    delayMode_ = mode;
    portEXIT_CRITICAL(&mux_);

    if (mode == RTALDelayClockMode::Sync)
        updateDelayTimes(true);
}

RTALDelayClockMode RTALMidiClock::delayMode()
{
    portENTER_CRITICAL(&mux_);
    const RTALDelayClockMode mode = delayMode_;
    portEXIT_CRITICAL(&mux_);
    return mode;
}

bool RTALMidiClock::setSource(RTALClockSource source)
{
    if (source != RTALClockSource::Midi &&
        source != RTALClockSource::Internal &&
        source != RTALClockSource::Tap)
        return false;

    portENTER_CRITICAL(&mux_);
    source_ = source;
    delayUpdatePending_ = true;
    portEXIT_CRITICAL(&mux_);

    updateDelayTimes(true);
    return true;
}

RTALClockSource RTALMidiClock::source()
{
    portENTER_CRITICAL(&mux_);
    const RTALClockSource result = source_;
    portEXIT_CRITICAL(&mux_);
    return result;
}

bool RTALMidiClock::setInternalBpm(float bpm)
{
    if (!isfinite(bpm) ||
        bpm < RTAL_INTERNAL_CLOCK_BPM_MIN ||
        bpm > RTAL_INTERNAL_CLOCK_BPM_MAX)
        return false;

    portENTER_CRITICAL(&mux_);
    internalBpm_ = bpm;
    if (source_ == RTALClockSource::Internal)
        delayUpdatePending_ = true;
    portEXIT_CRITICAL(&mux_);

    if (source() == RTALClockSource::Internal)
        updateDelayTimes(true);

    return true;
}

float RTALMidiClock::internalBpm()
{
    portENTER_CRITICAL(&mux_);
    const float result = internalBpm_;
    portEXIT_CRITICAL(&mux_);
    return result;
}

bool RTALMidiClock::tap()
{
    const uint32_t now = millis();
    bool accepted = false;

    portENTER_CRITICAL(&mux_);

    if (lastTapMs_ == 0 ||
        (now - lastTapMs_) > RTAL_TAP_TEMPO_TIMEOUT_MS)
    {
        memset(tapIntervalsMs_, 0, sizeof(tapIntervalsMs_));
        tapIntervalIndex_ = 0;
        tapIntervalCount_ = 0;
        tapIntervalSumMs_ = 0;
        tapCount_ = 1;
        lastTapMs_ = now;
        portEXIT_CRITICAL(&mux_);
        return false;
    }

    const uint32_t intervalMs = now - lastTapMs_;
    lastTapMs_ = now;

    const uint32_t minimumMs =
        static_cast<uint32_t>(
            60000.0f / RTAL_INTERNAL_CLOCK_BPM_MAX);
    const uint32_t maximumMs =
        static_cast<uint32_t>(
            60000.0f / RTAL_INTERNAL_CLOCK_BPM_MIN);

    if (intervalMs < minimumMs || intervalMs > maximumMs)
    {
        memset(tapIntervalsMs_, 0, sizeof(tapIntervalsMs_));
        tapIntervalIndex_ = 0;
        tapIntervalCount_ = 0;
        tapIntervalSumMs_ = 0;
        tapCount_ = 1;
        portEXIT_CRITICAL(&mux_);
        return false;
    }

    if (tapIntervalCount_ >= 1)
    {
        const float average =
            static_cast<float>(tapIntervalSumMs_) /
            static_cast<float>(tapIntervalCount_);

        const float relativeError =
            fabsf(static_cast<float>(intervalMs) - average) /
            average;

        if (relativeError > RTAL_TAP_TEMPO_OUTLIER_RATIO)
        {
            memset(tapIntervalsMs_, 0, sizeof(tapIntervalsMs_));
            tapIntervalsMs_[0] = intervalMs;
            tapIntervalIndex_ =
                1 % RTAL_TAP_TEMPO_INTERVAL_COUNT;
            tapIntervalCount_ = 1;
            tapIntervalSumMs_ = intervalMs;
            tapCount_ = 2;
        }
        else
        {
            if (tapIntervalCount_ <
                RTAL_TAP_TEMPO_INTERVAL_COUNT)
            {
                tapIntervalsMs_[tapIntervalIndex_] = intervalMs;
                tapIntervalSumMs_ += intervalMs;
                ++tapIntervalCount_;
            }
            else
            {
                tapIntervalSumMs_ -=
                    tapIntervalsMs_[tapIntervalIndex_];
                tapIntervalsMs_[tapIntervalIndex_] = intervalMs;
                tapIntervalSumMs_ += intervalMs;
            }

            tapIntervalIndex_ =
                (tapIntervalIndex_ + 1) %
                RTAL_TAP_TEMPO_INTERVAL_COUNT;
            ++tapCount_;
        }
    }
    else
    {
        tapIntervalsMs_[0] = intervalMs;
        tapIntervalIndex_ =
            1 % RTAL_TAP_TEMPO_INTERVAL_COUNT;
        tapIntervalCount_ = 1;
        tapIntervalSumMs_ = intervalMs;
        tapCount_ = 2;
    }

    if (tapIntervalCount_ >= 1)
    {
        const float averageMs =
            static_cast<float>(tapIntervalSumMs_) /
            static_cast<float>(tapIntervalCount_);

        tapBpm_ = 60000.0f / averageMs;
        tapBpmValid_ =
            tapBpm_ >= RTAL_INTERNAL_CLOCK_BPM_MIN &&
            tapBpm_ <= RTAL_INTERNAL_CLOCK_BPM_MAX;
        accepted = tapBpmValid_;

        if (accepted && source_ == RTALClockSource::Tap)
            delayUpdatePending_ = true;
    }

    portEXIT_CRITICAL(&mux_);

    if (accepted && source() == RTALClockSource::Tap)
        updateDelayTimes(true);

    return accepted;
}

RTALClockDivision RTALMidiClock::leftDivision()
{
    portENTER_CRITICAL(&mux_);
    const RTALClockDivision value = leftDivision_;
    portEXIT_CRITICAL(&mux_);
    return value;
}

RTALClockDivision RTALMidiClock::rightDivision()
{
    portENTER_CRITICAL(&mux_);
    const RTALClockDivision value = rightDivision_;
    portEXIT_CRITICAL(&mux_);
    return value;
}

bool RTALMidiClock::setLeftDivision(const char* text)
{
    RTALClockDivision value;
    if (!parseDivision(text, value))
        return false;
    return setLeftDivision(value);
}

bool RTALMidiClock::setRightDivision(const char* text)
{
    RTALClockDivision value;
    if (!parseDivision(text, value))
        return false;
    return setRightDivision(value);
}

bool RTALMidiClock::setLeftDivision(RTALClockDivision division)
{
    if (static_cast<uint8_t>(division) >
        static_cast<uint8_t>(
            RTALClockDivision::SixteenthTriplet))
        return false;

    portENTER_CRITICAL(&mux_);
    leftDivision_ = division;
    portEXIT_CRITICAL(&mux_);
    updateDelayTimes(true);
    return true;
}

bool RTALMidiClock::setRightDivision(RTALClockDivision division)
{
    if (static_cast<uint8_t>(division) >
        static_cast<uint8_t>(
            RTALClockDivision::SixteenthTriplet))
        return false;

    portENTER_CRITICAL(&mux_);
    rightDivision_ = division;
    portEXIT_CRITICAL(&mux_);
    updateDelayTimes(true);
    return true;
}

RTALClockDivision RTALMidiClock::divisionFromMidiValue(uint8_t value)
{
    if (value <= 10)  return RTALClockDivision::ThirtySecond;
    if (value <= 21)  return RTALClockDivision::SixteenthTriplet;
    if (value <= 31)  return RTALClockDivision::Sixteenth;
    if (value <= 42)  return RTALClockDivision::SixteenthDotted;
    if (value <= 53)  return RTALClockDivision::EighthTriplet;
    if (value <= 63)  return RTALClockDivision::Eighth;
    if (value <= 74)  return RTALClockDivision::EighthDotted;
    if (value <= 85)  return RTALClockDivision::QuarterTriplet;
    if (value <= 95)  return RTALClockDivision::Quarter;
    if (value <= 106) return RTALClockDivision::QuarterDotted;
    if (value <= 116) return RTALClockDivision::Half;
    return RTALClockDivision::Whole;
}

void RTALMidiClock::setLeftDivisionFromMidi(uint8_t value)
{
    setLeftDivision(divisionFromMidiValue(value));
}

void RTALMidiClock::setRightDivisionFromMidi(uint8_t value)
{
    setRightDivision(divisionFromMidiValue(value));
}

bool RTALMidiClock::parseDivision(
    const char* text,
    RTALClockDivision& division)
{
    if (!text) return false;

    char normalized[8] = {};
    const size_t length = strlen(text);

    if (length == 0 || length >= sizeof(normalized))
        return false;

    for (size_t i = 0; i < length; ++i)
        normalized[i] =
            static_cast<char>(
                toupper(static_cast<unsigned char>(text[i])));

    if (!strcmp(normalized, "1/1"))
        division = RTALClockDivision::Whole;
    else if (!strcmp(normalized, "1/2"))
        division = RTALClockDivision::Half;
    else if (!strcmp(normalized, "1/4"))
        division = RTALClockDivision::Quarter;
    else if (!strcmp(normalized, "1/8"))
        division = RTALClockDivision::Eighth;
    else if (!strcmp(normalized, "1/16"))
        division = RTALClockDivision::Sixteenth;
    else if (!strcmp(normalized, "1/32"))
        division = RTALClockDivision::ThirtySecond;
    else if (!strcmp(normalized, "1/4D"))
        division = RTALClockDivision::QuarterDotted;
    else if (!strcmp(normalized, "1/8D"))
        division = RTALClockDivision::EighthDotted;
    else if (!strcmp(normalized, "1/16D"))
        division = RTALClockDivision::SixteenthDotted;
    else if (!strcmp(normalized, "1/4T"))
        division = RTALClockDivision::QuarterTriplet;
    else if (!strcmp(normalized, "1/8T"))
        division = RTALClockDivision::EighthTriplet;
    else if (!strcmp(normalized, "1/16T"))
        division = RTALClockDivision::SixteenthTriplet;
    else
        return false;

    return true;
}

float RTALMidiClock::divisionQuarterFactor(
    RTALClockDivision division)
{
    switch (division)
    {
        case RTALClockDivision::Whole:             return 4.0f;
        case RTALClockDivision::Half:              return 2.0f;
        case RTALClockDivision::Quarter:           return 1.0f;
        case RTALClockDivision::Eighth:            return 0.5f;
        case RTALClockDivision::Sixteenth:         return 0.25f;
        case RTALClockDivision::ThirtySecond:      return 0.125f;
        case RTALClockDivision::QuarterDotted:     return 1.5f;
        case RTALClockDivision::EighthDotted:      return 0.75f;
        case RTALClockDivision::SixteenthDotted:   return 0.375f;
        case RTALClockDivision::QuarterTriplet:    return 2.0f / 3.0f;
        case RTALClockDivision::EighthTriplet:     return 1.0f / 3.0f;
        case RTALClockDivision::SixteenthTriplet:  return 1.0f / 6.0f;
    }
    return 1.0f;
}

float RTALMidiClock::divisionMilliseconds(
    RTALClockDivision division,
    float bpm)
{
    if (bpm <= 0.0f)
        return 0.0f;
    return (60000.0f / bpm) *
        divisionQuarterFactor(division);
}

float RTALMidiClock::quantizeMilliseconds(float value)
{
    const float step =
        RTAL_MIDI_CLOCK_DELAY_QUANTIZE_MS;
    return step > 0.0f
        ? roundf(value / step) * step
        : value;
}

void RTALMidiClock::updateDelayTimes(bool force)
{
    RTALDelayClockMode mode;
    RTALClockDivision left;
    RTALClockDivision right;
    bool active;
    bool valid;
    float bpm;

    portENTER_CRITICAL(&mux_);
    mode = delayMode_;
    left = leftDivision_;
    right = rightDivision_;
    effectiveClock(active, valid, bpm);
    portEXIT_CRITICAL(&mux_);

    if (mode != RTALDelayClockMode::Sync ||
        !active ||
        !valid)
        return;

    float leftMs =
        quantizeMilliseconds(
            divisionMilliseconds(left, bpm));
    float rightMs =
        quantizeMilliseconds(
            divisionMilliseconds(right, bpm));

    if (leftMs < 1.0f) leftMs = 1.0f;
    if (rightMs < 1.0f) rightMs = 1.0f;
    if (leftMs > RTAL_DELAY_MAX_MS)
        leftMs = RTAL_DELAY_MAX_MS;
    if (rightMs > RTAL_DELAY_MAX_MS)
        rightMs = RTAL_DELAY_MAX_MS;

    const bool updateLeft =
        force || lastAppliedLeftMs_ < 0.0f ||
        fabsf(leftMs - lastAppliedLeftMs_) >=
            RTAL_MIDI_CLOCK_DELAY_HYSTERESIS_MS;
    const bool updateRight =
        force || lastAppliedRightMs_ < 0.0f ||
        fabsf(rightMs - lastAppliedRightMs_) >=
            RTAL_MIDI_CLOCK_DELAY_HYSTERESIS_MS;

    if (updateLeft || updateRight)
    {
        // Keep an unchanged side exactly at its previously applied value while
        // the other side transitions. This avoids needless stereo image motion.
        const float applyLeft = updateLeft ? leftMs : lastAppliedLeftMs_;
        const float applyRight = updateRight ? rightMs : lastAppliedRightMs_;

        RTALStereoDelay::transitionSyncTimes(
            applyLeft, applyRight, RTAL_DELAY_SYNC_CROSSFADE_MS);

        if (updateLeft) lastAppliedLeftMs_ = leftMs;
        if (updateRight) lastAppliedRightMs_ = rightMs;
    }
}

RTALMidiClockState RTALMidiClock::state()
{
    RTALMidiClockState result = {};

    portENTER_CRITICAL(&mux_);

    bool selectedActive = false;
    bool selectedValid = false;
    float selectedBpm = 0.0f;
    effectiveClock(
        selectedActive,
        selectedValid,
        selectedBpm);

    result.active = selectedActive;
    result.running = running_;
    result.bpmValid = selectedValid;
    result.midiActive = midiActive_;
    result.midiBpmValid = midiBpmValid_;
    result.tapBpmValid = tapBpmValid_;
    result.source = source_;
    result.delayMode = delayMode_;
    result.leftDivision = leftDivision_;
    result.rightDivision = rightDivision_;
    result.bpm = selectedBpm;
    result.midiBpm = midiBpm_;
    result.internalBpm = internalBpm_;
    result.tapBpm = tapBpm_;
    result.leftMilliseconds =
        divisionMilliseconds(leftDivision_, selectedBpm);
    result.rightMilliseconds =
        divisionMilliseconds(rightDivision_, selectedBpm);
    result.tapCount = tapCount_;
    result.clockTicks = clockTicks_;
    result.startCount = startCount_;
    result.continueCount = continueCount_;
    result.stopCount = stopCount_;
    result.lastClockAgeMs =
        lastClockMs_ == 0
        ? 0
        : millis() - lastClockMs_;

    portEXIT_CRITICAL(&mux_);
    return result;
}

const char* RTALMidiClock::sourceName(
    RTALClockSource source)
{
    switch (source)
    {
        case RTALClockSource::Midi:     return "MIDI";
        case RTALClockSource::Internal: return "INTERNAL";
        case RTALClockSource::Tap:      return "TAP";
    }
    return "?";
}

const char* RTALMidiClock::divisionName(
    RTALClockDivision division)
{
    switch (division)
    {
        case RTALClockDivision::Whole:            return "1/1";
        case RTALClockDivision::Half:             return "1/2";
        case RTALClockDivision::Quarter:          return "1/4";
        case RTALClockDivision::Eighth:           return "1/8";
        case RTALClockDivision::Sixteenth:        return "1/16";
        case RTALClockDivision::ThirtySecond:     return "1/32";
        case RTALClockDivision::QuarterDotted:    return "1/4D";
        case RTALClockDivision::EighthDotted:     return "1/8D";
        case RTALClockDivision::SixteenthDotted:  return "1/16D";
        case RTALClockDivision::QuarterTriplet:   return "1/4T";
        case RTALClockDivision::EighthTriplet:    return "1/8T";
        case RTALClockDivision::SixteenthTriplet: return "1/16T";
    }
    return "?";
}

void RTALMidiClock::printStatus(Stream& output)
{
    const RTALMidiClockState s = state();

    output.println("Clock");
    output.print("  Source   : ");
    output.println(sourceName(s.source));
    output.print("  Active   : ");
    output.println(s.active ? "YES" : "NO");
    output.print("  BPM      : ");
    if (s.bpmValid) output.println(s.bpm, 2);
    else output.println("INVALID");

    output.print("  MIDI     : ");
    if (s.midiActive && s.midiBpmValid)
    {
        output.print(s.midiBpm, 2);
        output.println(" BPM");
    }
    else
        output.println("IDLE/INVALID");

    output.print("  Internal : ");
    output.print(s.internalBpm, 2);
    output.println(" BPM");

    output.print("  Tap      : ");
    if (s.tapBpmValid)
    {
        output.print(s.tapBpm, 2);
        output.print(" BPM taps=");
        output.println(s.tapCount);
    }
    else
    {
        output.print("WAITING taps=");
        output.println(s.tapCount);
    }

    output.print("  Transport: ");
    output.println(s.running ? "RUNNING" : "STOPPED");
    output.print("  Mode     : ");
    output.println(
        s.delayMode == RTALDelayClockMode::Sync
        ? "SYNC"
        : "FREE");

    output.print("  Left     : ");
    output.print(divisionName(s.leftDivision));
    output.print("  ");
    output.print(s.leftMilliseconds, 1);
    output.println(" ms");

    output.print("  Right    : ");
    output.print(divisionName(s.rightDivision));
    output.print("  ");
    output.print(s.rightMilliseconds, 1);
    output.println(" ms");
}

void RTALMidiClock::printReport()
{
    const RTALMidiClockState s = state();

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Clock source=%s active=%s bpm=%s%.2f internal=%.2f tap=%s%.2f taps=%u",
        sourceName(s.source),
        s.active ? "YES" : "NO",
        s.bpmValid ? "" : "INVALID/",
        s.bpm,
        s.internalBpm,
        s.tapBpmValid ? "" : "INVALID/",
        s.tapBpm,
        static_cast<unsigned>(s.tapCount));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "MIDI Clock activity=%s transport=%s bpm=%s%.2f ticks=%lu age=%lums start=%lu continue=%lu stop=%lu",
        s.midiActive ? "ACTIVE" : "IDLE",
        s.running ? "RUNNING" : "STOPPED",
        s.midiBpmValid ? "" : "INVALID/",
        s.midiBpm,
        static_cast<unsigned long>(s.clockTicks),
        static_cast<unsigned long>(s.lastClockAgeMs),
        static_cast<unsigned long>(s.startCount),
        static_cast<unsigned long>(s.continueCount),
        static_cast<unsigned long>(s.stopCount));

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Delay clock_mode=%s left=%s %.1fms right=%s %.1fms",
        s.delayMode == RTALDelayClockMode::Sync ? "SYNC" : "FREE",
        divisionName(s.leftDivision),
        s.leftMilliseconds,
        divisionName(s.rightDivision),
        s.rightMilliseconds);
}
