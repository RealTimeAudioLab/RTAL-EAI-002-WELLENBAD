#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"

enum class RTALDelayClockMode : uint8_t
{
    Free = 0,
    Sync = 1
};

enum class RTALClockSource : uint8_t
{
    Midi = 0,
    Internal,
    Tap
};

enum class RTALClockDivision : uint8_t
{
    Whole = 0,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    ThirtySecond,
    QuarterDotted,
    EighthDotted,
    SixteenthDotted,
    QuarterTriplet,
    EighthTriplet,
    SixteenthTriplet
};

struct RTALMidiClockState
{
    bool active;
    bool running;
    bool bpmValid;
    bool midiActive;
    bool midiBpmValid;
    bool tapBpmValid;
    RTALClockSource source;
    RTALDelayClockMode delayMode;
    RTALClockDivision leftDivision;
    RTALClockDivision rightDivision;
    float bpm;
    float midiBpm;
    float internalBpm;
    float tapBpm;
    float leftMilliseconds;
    float rightMilliseconds;
    uint8_t tapCount;
    uint32_t clockTicks;
    uint32_t startCount;
    uint32_t continueCount;
    uint32_t stopCount;
    uint32_t lastClockAgeMs;
};

class RTALMidiClock
{
public:
    static RTALStatus begin();
    static void handleRealtime(uint8_t value);
    static void service();

    static void setDelayMode(RTALDelayClockMode mode);
    static RTALDelayClockMode delayMode();

    static bool setSource(RTALClockSource source);
    static RTALClockSource source();
    static bool setInternalBpm(float bpm);
    static float internalBpm();
    static bool tap();
    static void resetTapSequence();

    static bool setLeftDivision(const char* text);
    static bool setRightDivision(const char* text);
    static bool setLeftDivision(RTALClockDivision division);
    static bool setRightDivision(RTALClockDivision division);
    static RTALClockDivision divisionFromMidiValue(uint8_t value);
    static void setLeftDivisionFromMidi(uint8_t value);
    static void setRightDivisionFromMidi(uint8_t value);

    static RTALClockDivision leftDivision();
    static RTALClockDivision rightDivision();

    static RTALMidiClockState state();
    static void printReport();
    static void printStatus(Stream& output);

    static const char* sourceName(RTALClockSource source);
    static const char* divisionName(RTALClockDivision division);

private:
    static bool parseDivision(
        const char* text,
        RTALClockDivision& division);
    static float divisionQuarterFactor(
        RTALClockDivision division);
    static float divisionMilliseconds(
        RTALClockDivision division,
        float bpm);
    static void updateDelayTimes(bool force = false);
    static float quantizeMilliseconds(float value);
    static void resetMidiTimingWindow();
    static void effectiveClock(
        bool& active,
        bool& valid,
        float& bpm);

    static portMUX_TYPE mux_;
    static bool midiActive_;
    static bool running_;
    static bool midiBpmValid_;
    static bool tapBpmValid_;
    static RTALClockSource source_;
    static RTALDelayClockMode delayMode_;
    static RTALClockDivision leftDivision_;
    static RTALClockDivision rightDivision_;

    static uint32_t lastClockUs_;
    static uint32_t lastClockMs_;
    static uint32_t tickIntervals_[RTAL_MIDI_CLOCK_SMOOTHING_TICKS];
    static uint8_t intervalIndex_;
    static uint8_t intervalCount_;
    static uint64_t intervalSumUs_;

    static float midiBpm_;
    static float internalBpm_;
    static float tapBpm_;
    static float lastStableMidiBpm_;
    static float tempoCandidateBpm_;
    static uint8_t tempoCandidateWindows_;
    static float lastAppliedLeftMs_;
    static float lastAppliedRightMs_;

    static uint32_t tapIntervalsMs_[RTAL_TAP_TEMPO_INTERVAL_COUNT];
    static uint8_t tapIntervalIndex_;
    static uint8_t tapIntervalCount_;
    static uint32_t tapIntervalSumMs_;
    static uint32_t lastTapMs_;
    static uint8_t tapCount_;

    static uint32_t clockTicks_;
    static uint32_t startCount_;
    static uint32_t continueCount_;
    static uint32_t stopCount_;
    static bool delayUpdatePending_;
};
