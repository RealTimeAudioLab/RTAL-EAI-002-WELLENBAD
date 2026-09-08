#pragma once
#include <Arduino.h>

class RTALDCBlocker
{
public:
    RTALDCBlocker();
    void reset();
    int16_t process(int16_t input);

private:
    float previousInput_;
    float previousOutput_;
};
