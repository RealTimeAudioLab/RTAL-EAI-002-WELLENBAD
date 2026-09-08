#include "../include/RTALI2SDriver.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALEventLog.h"
#include <limits.h>

volatile bool RTALI2SDriver::ready_ = false;
portMUX_TYPE RTALI2SDriver::statisticsMux_ = portMUX_INITIALIZER_UNLOCKED;
RTALI2SStatistics RTALI2SDriver::statistics_ = {};

RTALStatus RTALI2SDriver::installRx()
{
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_SLAVE | I2S_MODE_RX);
    config.sample_rate = RTAL_AUDIO_SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = RTAL_I2S_DMA_BUFFER_COUNT;
    config.dma_buf_len = RTAL_I2S_DMA_BUFFER_FRAMES;
    config.use_apll = false;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;

    esp_err_t error = i2s_driver_install(
        static_cast<i2s_port_t>(RTAL_I2S_RX_PORT),
        &config,
        0,
        nullptr);

    if (error != ESP_OK)
    {
        RTALLogger::printf(
            RTALLogLevel::Error,
            "I2S RX install failed err=%d",
            static_cast<int>(error));
        return RTALStatus::FATAL;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = RTAL_I2S_BCLK_PIN;
    pins.ws_io_num = RTAL_I2S_LRCK_PIN;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = RTAL_I2S_DATA_IN_PIN;

    error = i2s_set_pin(
        static_cast<i2s_port_t>(RTAL_I2S_RX_PORT),
        &pins);

    if (error != ESP_OK)
    {
        i2s_driver_uninstall(static_cast<i2s_port_t>(RTAL_I2S_RX_PORT));
        return RTALStatus::FATAL;
    }

    i2s_zero_dma_buffer(static_cast<i2s_port_t>(RTAL_I2S_RX_PORT));
    return RTALStatus::OK;
}

RTALStatus RTALI2SDriver::installTx()
{
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_SLAVE | I2S_MODE_TX);
    config.sample_rate = RTAL_AUDIO_SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = RTAL_I2S_DMA_BUFFER_COUNT;
    config.dma_buf_len = RTAL_I2S_DMA_BUFFER_FRAMES;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    esp_err_t error = i2s_driver_install(
        static_cast<i2s_port_t>(RTAL_I2S_TX_PORT),
        &config,
        0,
        nullptr);

    if (error != ESP_OK)
    {
        RTALLogger::printf(
            RTALLogLevel::Error,
            "I2S TX install failed err=%d",
            static_cast<int>(error));
        return RTALStatus::FATAL;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = RTAL_I2S_BCLK_PIN;
    pins.ws_io_num = RTAL_I2S_LRCK_PIN;
    pins.data_out_num = RTAL_I2S_DATA_OUT_PIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    error = i2s_set_pin(
        static_cast<i2s_port_t>(RTAL_I2S_TX_PORT),
        &pins);

    if (error != ESP_OK)
    {
        i2s_driver_uninstall(static_cast<i2s_port_t>(RTAL_I2S_TX_PORT));
        return RTALStatus::FATAL;
    }

    i2s_zero_dma_buffer(static_cast<i2s_port_t>(RTAL_I2S_TX_PORT));
    return RTALStatus::OK;
}

RTALStatus RTALI2SDriver::begin()
{
    statistics_ = {};
    statistics_.minimumReadWaitUs = UINT32_MAX;
    statistics_.minimumWriteWaitUs = UINT32_MAX;

    pinMode(RTAL_I2S_BCLK_PIN, INPUT_PULLDOWN);
    pinMode(RTAL_I2S_LRCK_PIN, INPUT_PULLDOWN);
    pinMode(RTAL_I2S_DATA_IN_PIN, INPUT_PULLDOWN);

    RTALStatus status = installRx();
    if (status != RTALStatus::OK) return status;

    status = installTx();
    if (status != RTALStatus::OK)
    {
        i2s_driver_uninstall(static_cast<i2s_port_t>(RTAL_I2S_RX_PORT));
        return status;
    }

    ready_ = true;

    RTALLogger::printf(
        RTALLogLevel::Info,
        "I2S slave RX%d/TX%d %lu Hz 32-bit stereo block=%u timeout=%lu ms",
        RTAL_I2S_RX_PORT,
        RTAL_I2S_TX_PORT,
        static_cast<unsigned long>(RTAL_AUDIO_SAMPLE_RATE),
        static_cast<unsigned>(RTAL_AUDIO_BLOCK_FRAMES),
        static_cast<unsigned long>(RTAL_I2S_IO_TIMEOUT_MS));

    RTALEventLog::add(RTALStatus::OK, "I2S", "Driver initialized");
    return RTALStatus::OK;
}

void RTALI2SDriver::end()
{
    ready_ = false;
    i2s_driver_uninstall(static_cast<i2s_port_t>(RTAL_I2S_RX_PORT));
    i2s_driver_uninstall(static_cast<i2s_port_t>(RTAL_I2S_TX_PORT));
}

bool RTALI2SDriver::readBlock(
    void* destination,
    size_t requestedBytes,
    size_t& receivedBytes,
    TickType_t timeoutTicks)
{
    receivedBytes = 0;

    const uint32_t waitStartedUs = RTAL_I2S_WAIT_TIMING_ENABLED ? micros() : 0;
    const esp_err_t error = i2s_read(
        static_cast<i2s_port_t>(RTAL_I2S_RX_PORT),
        destination,
        requestedBytes,
        &receivedBytes,
        timeoutTicks);
    const uint32_t waitUs = RTAL_I2S_WAIT_TIMING_ENABLED
        ? (micros() - waitStartedUs)
        : 0;

    portENTER_CRITICAL(&statisticsMux_);
    ++statistics_.readCalls;
    statistics_.bytesReceived += receivedBytes;
    statistics_.lastReadBytes = static_cast<uint32_t>(receivedBytes);

    if (error != ESP_OK && error != ESP_ERR_TIMEOUT) ++statistics_.readErrors;
    if (error == ESP_ERR_TIMEOUT) ++statistics_.readTimeouts;
    if (receivedBytes == 0) ++statistics_.zeroReads;
    else if (receivedBytes != requestedBytes) ++statistics_.shortReads;
    if (error == ESP_ERR_TIMEOUT || receivedBytes != requestedBytes)
        ++statistics_.noClockReads;

    if (RTAL_I2S_WAIT_TIMING_ENABLED)
    {
        statistics_.accumulatedReadWaitUs += waitUs;
        ++statistics_.readWaitSamples;
        if (waitUs > statistics_.maximumReadWaitUs) statistics_.maximumReadWaitUs = waitUs;
        if (waitUs < statistics_.minimumReadWaitUs) statistics_.minimumReadWaitUs = waitUs;
    }

    portEXIT_CRITICAL(&statisticsMux_);

    return error == ESP_OK && receivedBytes == requestedBytes;
}

bool RTALI2SDriver::writeBlock(
    const void* source,
    size_t requestedBytes,
    size_t& transmittedBytes,
    TickType_t timeoutTicks)
{
    transmittedBytes = 0;

    const uint32_t waitStartedUs = RTAL_I2S_WAIT_TIMING_ENABLED ? micros() : 0;
    const esp_err_t error = i2s_write(
        static_cast<i2s_port_t>(RTAL_I2S_TX_PORT),
        source,
        requestedBytes,
        &transmittedBytes,
        timeoutTicks);
    const uint32_t waitUs = RTAL_I2S_WAIT_TIMING_ENABLED
        ? (micros() - waitStartedUs)
        : 0;

    portENTER_CRITICAL(&statisticsMux_);
    ++statistics_.writeCalls;
    statistics_.bytesTransmitted += transmittedBytes;
    statistics_.lastWriteBytes = static_cast<uint32_t>(transmittedBytes);

    if (error != ESP_OK && error != ESP_ERR_TIMEOUT) ++statistics_.writeErrors;
    if (error == ESP_ERR_TIMEOUT) ++statistics_.writeTimeouts;
    if (transmittedBytes == 0) ++statistics_.zeroWrites;
    else if (transmittedBytes != requestedBytes) ++statistics_.shortWrites;
    if (error == ESP_ERR_TIMEOUT || transmittedBytes != requestedBytes)
        ++statistics_.noClockWrites;

    if (RTAL_I2S_WAIT_TIMING_ENABLED)
    {
        statistics_.accumulatedWriteWaitUs += waitUs;
        ++statistics_.writeWaitSamples;
        if (waitUs > statistics_.maximumWriteWaitUs) statistics_.maximumWriteWaitUs = waitUs;
        if (waitUs < statistics_.minimumWriteWaitUs) statistics_.minimumWriteWaitUs = waitUs;
    }

    portEXIT_CRITICAL(&statisticsMux_);

    return error == ESP_OK && transmittedBytes == requestedBytes;
}

bool RTALI2SDriver::isReady()
{
    return ready_;
}

RTALI2SStatistics RTALI2SDriver::statistics()
{
    portENTER_CRITICAL(&statisticsMux_);
    const RTALI2SStatistics copy = statistics_;

    // Build0046d wait-time statistics are report-window values. Cumulative
    // transfer/error counters stay untouched so increases remain visible.
    statistics_.accumulatedReadWaitUs = 0;
    statistics_.accumulatedWriteWaitUs = 0;
    statistics_.readWaitSamples = 0;
    statistics_.writeWaitSamples = 0;
    statistics_.minimumReadWaitUs = UINT32_MAX;
    statistics_.maximumReadWaitUs = 0;
    statistics_.minimumWriteWaitUs = UINT32_MAX;
    statistics_.maximumWriteWaitUs = 0;

    portEXIT_CRITICAL(&statisticsMux_);
    return copy;
}
