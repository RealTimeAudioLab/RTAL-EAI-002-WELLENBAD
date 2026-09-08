#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include "RTALStatus.h"

struct RTALI2SStatistics
{
    uint32_t readCalls;
    uint32_t writeCalls;
    uint32_t noClockReads;
    uint32_t noClockWrites;
    uint32_t readErrors;
    uint32_t writeErrors;
    uint32_t bytesReceived;
    uint32_t bytesTransmitted;

    // Build0046d/0046e: split transfer anomalies and measure blocking wait time.
    // DMA count/length and legacy driver installation remain unchanged.
    uint32_t readTimeouts;
    uint32_t writeTimeouts;
    uint32_t shortReads;
    uint32_t shortWrites;
    uint32_t zeroReads;
    uint32_t zeroWrites;
    uint32_t lastReadBytes;
    uint32_t lastWriteBytes;
    uint32_t minimumReadWaitUs;
    uint32_t maximumReadWaitUs;
    uint32_t minimumWriteWaitUs;
    uint32_t maximumWriteWaitUs;
    uint64_t accumulatedReadWaitUs;
    uint64_t accumulatedWriteWaitUs;
    uint32_t readWaitSamples;
    uint32_t writeWaitSamples;
};

class RTALI2SDriver
{
public:
    static RTALStatus begin();
    static void end();

    static bool readBlock(
        void* destination,
        size_t requestedBytes,
        size_t& receivedBytes,
        TickType_t timeoutTicks);

    static bool writeBlock(
        const void* source,
        size_t requestedBytes,
        size_t& transmittedBytes,
        TickType_t timeoutTicks);

    static bool isReady();
    static RTALI2SStatistics statistics();

private:
    static RTALStatus installRx();
    static RTALStatus installTx();

    static volatile bool ready_;
    static portMUX_TYPE statisticsMux_;
    static RTALI2SStatistics statistics_;
};
