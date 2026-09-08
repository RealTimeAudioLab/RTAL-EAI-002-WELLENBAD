#pragma once
#include <Arduino.h>
class RTALOutputLimiter
{
public:
    RTALOutputLimiter();
    void reset();
    float process(float input, uint32_t& limiterEvents, uint32_t& hardClips);
    float gain() const;
private:
    float gain_;
};
