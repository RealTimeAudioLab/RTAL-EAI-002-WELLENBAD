#include "../include/RTALPresetState.h"
#include "../include/RTALLogger.h"

portMUX_TYPE RTALPresetState::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALPresetStateSnapshot RTALPresetState::state_ = {};

RTALStatus RTALPresetState::begin()
{
    portENTER_CRITICAL(&mux_);
    state_ = {};
    state_.source = RTALPresetSource::Startup;
    state_.state = RTALPresetStateValue::None;
    portEXIT_CRITICAL(&mux_);

    RTALLogger::printf(
        RTALLogLevel::Info,
        "PresetState .... PASS active=NONE source=STARTUP state=NONE");
    return RTALStatus::OK;
}

void RTALPresetState::transitionRequested(uint8_t slot, RTALPresetSource source)
{
    portENTER_CRITICAL(&mux_);
    state_.requestedSlot = slot;
    state_.source = source;
    state_.state = RTALPresetStateValue::Transition;
    portEXIT_CRITICAL(&mux_);
}

void RTALPresetState::activated(uint8_t slot, RTALPresetSource source)
{
    portENTER_CRITICAL(&mux_);
    state_.activeSlot = slot;
    state_.requestedSlot = 0;
    state_.source = source;
    state_.state = RTALPresetStateValue::Active;
    ++state_.loadCount;
    portEXIT_CRITICAL(&mux_);
}

void RTALPresetState::loadFailed(uint8_t slot, RTALPresetSource source)
{
    portENTER_CRITICAL(&mux_);
    state_.requestedSlot = slot;
    state_.source = source;
    state_.state = RTALPresetStateValue::Failed;
    ++state_.failedCount;
    portEXIT_CRITICAL(&mux_);
}

void RTALPresetState::markModified()
{
    portENTER_CRITICAL(&mux_);
    if (state_.activeSlot != 0 && state_.state != RTALPresetStateValue::Transition)
    {
        if (state_.state != RTALPresetStateValue::Modified)
            ++state_.modifiedCount;
        state_.state = RTALPresetStateValue::Modified;
    }
    portEXIT_CRITICAL(&mux_);
}

void RTALPresetState::committed(uint8_t slot, RTALPresetSource source)
{
    portENTER_CRITICAL(&mux_);
    state_.activeSlot = slot;
    state_.requestedSlot = 0;
    state_.source = source;
    state_.state = RTALPresetStateValue::Active;
    ++state_.commitCount;
    portEXIT_CRITICAL(&mux_);
}

void RTALPresetState::setLastMidiProgram(uint8_t displayedProgram)
{
    portENTER_CRITICAL(&mux_);
    state_.lastMidiProgram = displayedProgram;
    portEXIT_CRITICAL(&mux_);
}

RTALPresetStateSnapshot RTALPresetState::snapshot()
{
    portENTER_CRITICAL(&mux_);
    const RTALPresetStateSnapshot copy = state_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

const char* RTALPresetState::sourceName(RTALPresetSource source)
{
    switch (source)
    {
        case RTALPresetSource::Startup: return "STARTUP";
        case RTALPresetSource::Serial:  return "SERIAL";
        case RTALPresetSource::Midi:    return "MIDI";
        case RTALPresetSource::Unknown: return "UNKNOWN";
    }
    return "?";
}

const char* RTALPresetState::stateName(RTALPresetStateValue state)
{
    switch (state)
    {
        case RTALPresetStateValue::None:       return "NONE";
        case RTALPresetStateValue::Transition: return "TRANSITION";
        case RTALPresetStateValue::Active:     return "ACTIVE";
        case RTALPresetStateValue::Modified:   return "MODIFIED";
        case RTALPresetStateValue::Failed:     return "FAILED";
    }
    return "?";
}

void RTALPresetState::print(Stream& output)
{
    const RTALPresetStateSnapshot s = snapshot();
    output.print("Active preset=");
    if (s.activeSlot == 0) output.print("NONE"); else output.print(s.activeSlot);
    output.print(" requested=");
    if (s.requestedSlot == 0) output.print("NONE"); else output.print(s.requestedSlot);
    output.print(" source=");
    output.print(sourceName(s.source));
    output.print(" state=");
    output.print(stateName(s.state));
    output.print(" last_midi_program=");
    if (s.lastMidiProgram == 0) output.println("NONE"); else output.println(s.lastMidiProgram);
}

void RTALPresetState::printReport()
{
    const RTALPresetStateSnapshot s = snapshot();
    RTALLogger::printf(
        RTALLogLevel::Info,
        "Active preset=%u requested=%u source=%s state=%s last_pc=%u loads=%lu modified=%lu commits=%lu failed=%lu",
        static_cast<unsigned>(s.activeSlot),
        static_cast<unsigned>(s.requestedSlot),
        sourceName(s.source),
        stateName(s.state),
        static_cast<unsigned>(s.lastMidiProgram),
        static_cast<unsigned long>(s.loadCount),
        static_cast<unsigned long>(s.modifiedCount),
        static_cast<unsigned long>(s.commitCount),
        static_cast<unsigned long>(s.failedCount));
}
