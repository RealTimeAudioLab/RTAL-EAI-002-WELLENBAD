#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"

struct RTALMidiControlStatistics
{
    uint32_t receivedBytes;
    uint32_t channelMessages;
    uint32_t controlChanges;
    uint32_t mappedControlChanges;
    uint32_t programChanges;
    uint32_t loadedPrograms;
    uint32_t failedPrograms;
    uint32_t ignoredChannelMessages;
    uint32_t realtimeBytes;
    uint32_t serviceCalls;
    uint32_t serviceBudgetHits;
    uint32_t maximumBytesPerService;
    uint32_t coalescedControlChanges;
    uint32_t duplicateControlChanges;
    uint8_t lastChannel;
    uint8_t lastController;
    uint8_t lastValue;
    uint8_t lastProgram;
};

class RTALMidiControl
{
public:
    static RTALStatus begin();
    static void service();
    static RTALMidiControlStatistics statistics(bool resetWindow);
    static void printReport();
    static bool activityActive();
    static uint32_t lastActivityMs();

private:
    static void consumeByte(uint8_t value);
    static void dispatchChannelMessage(
        uint8_t status,
        uint8_t data1,
        uint8_t data2);
    static void handleControlChange(
        uint8_t channel,
        uint8_t controller,
        uint8_t value);
    static void queueControlChange(
        uint8_t channel,
        uint8_t controller,
        uint8_t value);
    static void flushPendingControlChanges();
    static void applyControlChange(
        uint8_t channel,
        uint8_t controller,
        uint8_t value);
    static void handleProgramChange(
        uint8_t channel,
        uint8_t program);
    static bool acceptsChannel(uint8_t channel);
    static uint8_t dataLengthForStatus(uint8_t status);
    static float mapLinear(
        uint8_t value,
        float minimum,
        float maximum);

    static HardwareSerial midiSerial_;
    static uint8_t runningStatus_;
    static uint8_t data_[2];
    static uint8_t dataCount_;
    static uint8_t expectedData_;
    static RTALMidiControlStatistics statistics_;
    static portMUX_TYPE mux_;
    static uint32_t lastActivityMs_;

    static uint8_t pendingCcValue_[128];
    static uint8_t pendingCcChannel_[128];
    static bool pendingCc_[128];
    static uint8_t lastAppliedCcValue_[128];
    static bool lastAppliedCcValid_[128];
};
