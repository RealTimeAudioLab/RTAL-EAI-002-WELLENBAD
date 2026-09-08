#include "../include/RTALOutputLimiter.h"
#include "../include/RTALConfig.h"
#include <math.h>

RTALOutputLimiter::RTALOutputLimiter() : gain_(1.0f) {}
void RTALOutputLimiter::reset() { gain_ = 1.0f; }

float RTALOutputLimiter::process(float input, uint32_t& limiterEvents, uint32_t& hardClips)
{
    const float a = fabsf(input);
    if (a > RTAL_DSP_LIMITER_THRESHOLD && a > 0.0f)
    {
        const float target = RTAL_DSP_LIMITER_THRESHOLD / a;
        if (target < gain_)
        {
            gain_ = target;
            ++limiterEvents;
        }
    }
    else
    {
        gain_ = RTAL_DSP_LIMITER_RELEASE * gain_ + (1.0f - RTAL_DSP_LIMITER_RELEASE);
        if (gain_ > 1.0f) gain_ = 1.0f;
    }

    float y = input * gain_;
    if (y > 1.0f) { y = 1.0f; ++hardClips; }
    else if (y < -1.0f) { y = -1.0f; ++hardClips; }
    return y;
}

float RTALOutputLimiter::gain() const { return gain_; }
