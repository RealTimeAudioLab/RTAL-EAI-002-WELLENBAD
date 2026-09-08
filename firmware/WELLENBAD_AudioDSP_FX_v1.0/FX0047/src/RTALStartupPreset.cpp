#include "../include/RTALStartupPreset.h"
#include "../include/RTALDelayPresetTransition.h"
#include "../include/RTALLogger.h"
#include "../include/RTALPresetState.h"
#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "rtal-sys";
    constexpr const char* kEnabledKey = "st_en";
    constexpr const char* kSlotKey = "st_slot";
}

portMUX_TYPE RTALStartupPreset::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALStartupPresetState RTALStartupPreset::state_ = {};

bool RTALStartupPreset::validSlot(uint8_t slot)
{
    return slot >= 1 && slot <= RTAL_DELAY_RAM_PRESET_COUNT;
}

bool RTALStartupPreset::loadConfiguration()
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    const bool enabled = preferences.getBool(kEnabledKey, false);
    const uint8_t slot = preferences.getUChar(kSlotKey, 0);
    preferences.end();
    portENTER_CRITICAL(&mux_);
    state_.enabled = enabled && validSlot(slot);
    state_.slot = validSlot(slot) ? slot : 0;
    portEXIT_CRITICAL(&mux_);
    return true;
}

bool RTALStartupPreset::saveConfiguration()
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    RTALStartupPresetState snapshot;
    portENTER_CRITICAL(&mux_); snapshot = state_; portEXIT_CRITICAL(&mux_);
    const size_t a = preferences.putBool(kEnabledKey, snapshot.enabled);
    const size_t b = preferences.putUChar(kSlotKey, snapshot.slot);
    preferences.end();
    return a > 0 && b > 0;
}

RTALStatus RTALStartupPreset::begin()
{
    state_ = {};
    if (!RTAL_STARTUP_PRESET_ENABLED)
    {
        RTALLogger::printf(RTALLogLevel::Info, "StartupPreset .. PASS feature=DISABLED");
        return RTALStatus::OK;
    }
    if (!loadConfiguration())
        RTALLogger::printf(RTALLogLevel::Warning, "StartupPreset .. WARN configuration read failed; using OFF");
    portENTER_CRITICAL(&mux_);
    if (state_.enabled && validSlot(state_.slot))
    {
        state_.pending = true;
        state_.applyAtMs = millis() + RTAL_STARTUP_PRESET_DELAY_MS;
    }
    const RTALStartupPresetState snapshot = state_;
    portEXIT_CRITICAL(&mux_);
    RTALLogger::printf(RTALLogLevel::Info,
        "StartupPreset .. PASS enabled=%s slot=%u pending=%s delay=%lums",
        snapshot.enabled ? "YES" : "NO", static_cast<unsigned>(snapshot.slot),
        snapshot.pending ? "YES" : "NO",
        static_cast<unsigned long>(RTAL_STARTUP_PRESET_DELAY_MS));
    return RTALStatus::OK;
}

void RTALStartupPreset::service()
{
    RTALStartupPresetState snapshot;
    portENTER_CRITICAL(&mux_); snapshot = state_; portEXIT_CRITICAL(&mux_);
    if (!snapshot.pending || snapshot.attempted ||
        static_cast<int32_t>(millis() - snapshot.applyAtMs) < 0) return;
    portENTER_CRITICAL(&mux_);
    state_.attempted = true; state_.pending = false;
    portEXIT_CRITICAL(&mux_);
    const bool queued = RTALDelayPresetTransition::request(snapshot.slot, RTALPresetSource::Startup);
    portENTER_CRITICAL(&mux_); state_.queued = queued; portEXIT_CRITICAL(&mux_);
    if (queued)
        RTALLogger::printf(RTALLogLevel::Info, "Startup preset queued slot=%u", static_cast<unsigned>(snapshot.slot));
    else
    {
        RTALPresetState::loadFailed(snapshot.slot, RTALPresetSource::Startup);
        RTALLogger::printf(RTALLogLevel::Warning,
            "Startup preset skipped slot=%u reason=EMPTY_OR_INVALID",
            static_cast<unsigned>(snapshot.slot));
    }
}

bool RTALStartupPreset::setSlot(uint8_t slot)
{
    if (!validSlot(slot)) return false;
    portENTER_CRITICAL(&mux_);
    state_.enabled=true; state_.slot=slot; state_.pending=false;
    state_.attempted=false; state_.queued=false; state_.applyAtMs=0;
    portEXIT_CRITICAL(&mux_);
    return saveConfiguration();
}

bool RTALStartupPreset::disable()
{
    portENTER_CRITICAL(&mux_);
    state_.enabled=false; state_.slot=0; state_.pending=false;
    state_.attempted=false; state_.queued=false; state_.applyAtMs=0;
    portEXIT_CRITICAL(&mux_);
    return saveConfiguration();
}

RTALStartupPresetState RTALStartupPreset::state()
{
    portENTER_CRITICAL(&mux_); const RTALStartupPresetState copy=state_; portEXIT_CRITICAL(&mux_); return copy;
}

void RTALStartupPreset::print(Stream& output)
{
    const RTALStartupPresetState s=state();
    output.print("Startup preset="); if(s.enabled) output.print(s.slot); else output.print("OFF");
    output.print(" enabled="); output.print(s.enabled?"YES":"NO");
    output.print(" pending="); output.print(s.pending?"YES":"NO");
    output.print(" attempted="); output.print(s.attempted?"YES":"NO");
    output.print(" queued="); output.println(s.queued?"YES":"NO");
}

void RTALStartupPreset::printReport()
{
    const RTALStartupPresetState s=state();
    RTALLogger::printf(RTALLogLevel::Info,
        "Startup preset enabled=%s slot=%u pending=%s attempted=%s queued=%s",
        s.enabled?"YES":"NO", static_cast<unsigned>(s.slot),
        s.pending?"YES":"NO", s.attempted?"YES":"NO", s.queued?"YES":"NO");
}
