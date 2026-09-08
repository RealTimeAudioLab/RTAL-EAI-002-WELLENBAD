#include "../include/RTALDCBlocker.h"
#include "../include/RTALConfig.h"
#include <math.h>

RTALDCBlocker::RTALDCBlocker()
: previousInput_(0.0f), previousOutput_(0.0f)
{
}

void RTALDCBlocker::reset()
{
    previousInput_ = 0.0f;
    previousOutput_ = 0.0f;
}

int16_t RTALDCBlocker::process(int16_t input)
{
    const float x = static_cast<float>(input);
    const float y = x - previousInput_ + RTAL_DC_BLOCKER_R * previousOutput_;

    previousInput_ = x;
    previousOutput_ = y;

    float clipped = y;
    if (clipped > 32767.0f) clipped = 32767.0f;
    if (clipped < -32768.0f) clipped = -32768.0f;

    return static_cast<int16_t>(lrintf(clipped));
}
