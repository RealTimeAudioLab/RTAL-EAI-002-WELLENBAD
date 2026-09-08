#include "../include/RTALDelayPresetTransition.h"
#include "../include/RTALLogger.h"
#include "../include/RTALStereoDelay.h"
#include "../include/RTALMidiClock.h"
#include "../include/RTALPresetState.h"
#include <string.h>

portMUX_TYPE RTALDelayPresetTransition::mux_ =
    portMUX_INITIALIZER_UNLOCKED;

RTALDelayPresetTransitionState
RTALDelayPresetTransition::state_ =
    RTALDelayPresetTransitionState::Idle;

RTALDelayPresetData
RTALDelayPresetTransition::pending_ = {};

uint8_t RTALDelayPresetTransition::pendingSlot_ = 0;
uint32_t RTALDelayPresetTransition::deadlineMs_ = 0;
float RTALDelayPresetTransition::restoreLevel_ = 0.0f;
bool RTALDelayPresetTransition::restoreEnabled_ = false;
RTALPresetSource RTALDelayPresetTransition::pendingSource_ = RTALPresetSource::Unknown;

RTALDelayPresetTransitionStatistics
RTALDelayPresetTransition::statistics_ = {};

RTALStatus RTALDelayPresetTransition::begin()
{
    state_ = RTALDelayPresetTransitionState::Idle;
    pending_ = {};
    pendingSlot_ = 0;
    deadlineMs_ = 0;
    restoreLevel_ = 0.0f;
    restoreEnabled_ = false;
    pendingSource_ = RTALPresetSource::Unknown;
    statistics_ = {};

    RTALLogger::printf(
        RTALLogLevel::Info,
        "PresetTransition PASS fade_out=%lums settle=%lums",
        static_cast<unsigned long>(
            RTAL_DELAY_PRESET_FADE_OUT_MS),
        static_cast<unsigned long>(
            RTAL_DELAY_PRESET_SETTLE_MS));

    return RTALStatus::OK;
}

bool RTALDelayPresetTransition::request(
    uint8_t slot,
    RTALPresetSource source)
{
    RTALDelayPresetData data = {};

    if (!RTALDelayRamPresets::exportSlot(slot, data))
    {
        portENTER_CRITICAL(&mux_);
        ++statistics_.requests;
        ++statistics_.rejected;
        portEXIT_CRITICAL(&mux_);
        return false;
    }

    portENTER_CRITICAL(&mux_);

    ++statistics_.requests;

    if (state_ != RTALDelayPresetTransitionState::Idle)
        ++statistics_.replaced;

    pending_ = data;
    pendingSlot_ = slot;
    restoreLevel_ = data.parameters.level;
    restoreEnabled_ = data.parameters.enabled;
    pendingSource_ = source;
    state_ = RTALDelayPresetTransitionState::FadeOut;
    statistics_.activeSlot = slot;
    statistics_.state = state_;
    deadlineMs_ = millis() + RTAL_DELAY_PRESET_FADE_OUT_MS;

    portEXIT_CRITICAL(&mux_);

    RTALPresetState::transitionRequested(slot, source);
    // Audio-side parameter smoothing performs the fade.
    RTALStereoDelay::setLevel(0.0f);
    return true;
}

bool RTALDelayPresetTransition::applyMuted(
    const RTALDelayPresetData& data)
{
    if (!RTALDelayRamPresets::validate(data))
        return false;

    const RTALStereoDelayParameters& p = data.parameters;

    RTALMidiClock::setDelayMode(RTALDelayClockMode::Free);
    RTALStereoDelay::setEnabled(false);
    RTALStereoDelay::setLevel(0.0f);
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

    if (!RTALMidiClock::setLeftDivision(data.leftDivision))
        return false;

    if (!RTALMidiClock::setRightDivision(data.rightDivision))
        return false;

    RTALMidiClock::setDelayMode(data.clockMode);

    // Keep the wet path muted until the settle interval has elapsed.
    RTALStereoDelay::setEnabled(restoreEnabled_);
    RTALStereoDelay::setLevel(0.0f);
    return true;
}

void RTALDelayPresetTransition::service()
{
    RTALDelayPresetTransitionState state;
    uint32_t deadline;

    portENTER_CRITICAL(&mux_);
    state = state_;
    deadline = deadlineMs_;
    portEXIT_CRITICAL(&mux_);

    if (state == RTALDelayPresetTransitionState::Idle)
        return;

    const uint32_t now = millis();

    if (state == RTALDelayPresetTransitionState::FadeOut &&
        static_cast<int32_t>(now - deadline) >= 0)
    {
        portENTER_CRITICAL(&mux_);
        state_ = RTALDelayPresetTransitionState::Apply;
        statistics_.state = state_;
        portEXIT_CRITICAL(&mux_);

        if (!applyMuted(pending_))
        {
            portENTER_CRITICAL(&mux_);
            ++statistics_.rejected;
            RTALPresetState::loadFailed(pendingSlot_, pendingSource_);
            state_ = RTALDelayPresetTransitionState::Idle;
            statistics_.state = state_;
            statistics_.activeSlot = 0;
            portEXIT_CRITICAL(&mux_);
            return;
        }

        portENTER_CRITICAL(&mux_);
        state_ = RTALDelayPresetTransitionState::Settle;
        statistics_.state = state_;
        deadlineMs_ = now + RTAL_DELAY_PRESET_SETTLE_MS;
        portEXIT_CRITICAL(&mux_);
        return;
    }

    if (state == RTALDelayPresetTransitionState::Settle &&
        static_cast<int32_t>(now - deadline) >= 0)
    {
        portENTER_CRITICAL(&mux_);
        state_ = RTALDelayPresetTransitionState::FadeIn;
        statistics_.state = state_;
        portEXIT_CRITICAL(&mux_);

        RTALStereoDelay::setLevel(restoreLevel_);

        portENTER_CRITICAL(&mux_);
        ++statistics_.completed;
        RTALPresetState::activated(pendingSlot_, pendingSource_);
        statistics_.lastCompletedSlot = pendingSlot_;
        statistics_.activeSlot = 0;
        state_ = RTALDelayPresetTransitionState::Idle;
        statistics_.state = state_;
        portEXIT_CRITICAL(&mux_);
    }
}

bool RTALDelayPresetTransition::busy()
{
    portENTER_CRITICAL(&mux_);
    const bool result =
        state_ != RTALDelayPresetTransitionState::Idle;
    portEXIT_CRITICAL(&mux_);
    return result;
}

RTALDelayPresetTransitionStatistics
RTALDelayPresetTransition::statistics(bool resetWindow)
{
    portENTER_CRITICAL(&mux_);
    const RTALDelayPresetTransitionStatistics copy =
        statistics_;

    if (resetWindow)
    {
        const uint8_t activeSlot = statistics_.activeSlot;
        const uint8_t lastCompletedSlot =
            statistics_.lastCompletedSlot;
        const RTALDelayPresetTransitionState state =
            statistics_.state;

        statistics_ = {};
        statistics_.activeSlot = activeSlot;
        statistics_.lastCompletedSlot = lastCompletedSlot;
        statistics_.state = state;
    }

    portEXIT_CRITICAL(&mux_);
    return copy;
}

const char* RTALDelayPresetTransition::stateName(
    RTALDelayPresetTransitionState state)
{
    switch (state)
    {
        case RTALDelayPresetTransitionState::Idle:
            return "IDLE";
        case RTALDelayPresetTransitionState::FadeOut:
            return "FADE_OUT";
        case RTALDelayPresetTransitionState::Apply:
            return "APPLY";
        case RTALDelayPresetTransitionState::Settle:
            return "SETTLE";
        case RTALDelayPresetTransitionState::FadeIn:
            return "FADE_IN";
    }

    return "?";
}

void RTALDelayPresetTransition::printReport()
{
    const RTALDelayPresetTransitionStatistics s =
        statistics(true);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "Preset transition state=%s requests=%lu completed=%lu rejected=%lu replaced=%lu active=%u last=%u",
        stateName(s.state),
        static_cast<unsigned long>(s.requests),
        static_cast<unsigned long>(s.completed),
        static_cast<unsigned long>(s.rejected),
        static_cast<unsigned long>(s.replaced),
        static_cast<unsigned>(s.activeSlot),
        static_cast<unsigned>(s.lastCompletedSlot));
}
