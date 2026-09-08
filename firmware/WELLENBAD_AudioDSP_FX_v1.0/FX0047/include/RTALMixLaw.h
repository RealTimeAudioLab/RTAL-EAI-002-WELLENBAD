#pragma once
#include <math.h>

// Preserve the legacy linear dry/wet ratio and normalize its RMS power:
//   y = ((1-m) * dry + m * wet) * N
//   N = 1 / sqrt((1-m)^2 + m^2)
// Endpoints remain unity and 50/50 is compensated by sqrt(2).
// This helper is called only at control/block rate when Mix changes.
static inline float rtalConstantPowerMixNorm(float mix)
{
    if (mix <= 0.0f || mix >= 1.0f) return 1.0f;
    const float dry = 1.0f - mix;
    const float power = dry * dry + mix * mix;
    return 1.0f / sqrtf(power);
}
