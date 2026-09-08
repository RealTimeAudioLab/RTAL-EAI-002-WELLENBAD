#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

struct RTALStereoWidthParameters {
    bool enabled;
    float width; // 0.0 mono, 1.0 original, 2.0 extra-wide
};

class RTALStereoWidth {
public:
    static RTALStatus begin();
    static void setEnabled(bool enabled);
    static void setWidth(float width);
    static RTALStereoWidthParameters parameters();
    static void beginBlock(size_t frames);
    static bool activeForBlock();
    static inline void process(float inL, float inR, float& outL, float& outR) {
        // Smooth the Side gain. Mid remains unity, so 100% is mathematically transparent.
        currentWidth_ += widthStep_;
        if (!blockEnabled_) { outL = inL; outR = inR; return; }
        const float mid  = 0.5f * (inL + inR);
        const float side = 0.5f * (inL - inR) * currentWidth_;
        outL = mid + side;
        outR = mid - side;
    }
private:
    static float clampWidth(float v);
    static portMUX_TYPE mux_;
    static RTALStereoWidthParameters target_;
    static bool blockEnabled_;
    static bool blockNeedsProcessing_;
    static float currentWidth_;
    static float widthStep_;
};
