#pragma once
#include <Arduino.h>
#include "RTALConfig.h"

class RTALAudioFormat
{
public:
    static inline int16_t containerToInt16(int32_t sample32)
    {
        return static_cast<int16_t>(sample32 >> RTAL_INPUT_PAYLOAD_SHIFT);
    }

    static inline int32_t int16ToContainer(int16_t sample16)
    {
        return static_cast<int32_t>(sample16) << RTAL_OUTPUT_PAYLOAD_SHIFT;
    }
};
