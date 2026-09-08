#pragma once
#include <Arduino.h>
#include "RTALStatus.h"

enum class RTALModLfoShape : uint8_t { Sine=0, Triangle=1 };

struct RTALModCoreParameters {
    bool enabled;
    RTALModLfoShape shape;
    float rateHz;
    bool sync;
    uint8_t division;
    float depth;
    float stereoPhaseDeg;
    float smoothingMs;
};

struct RTALModCoreState {
    float left;
    float right;
    float effectiveRateHz;
    uint32_t phase;
};

class RTALModCore {
public:
    static RTALStatus begin(uint32_t sampleRate);
    static void prepareBlock(size_t frames);
    static void advanceBlock(size_t frames);
    // Prepared for Chorus/Flanger/Phaser builds: one coherent stereo LFO sample.
    static void nextStereo(float& left, float& right);
    static void setEnabled(bool v);
    static void setShape(RTALModLfoShape v);
    static void setRateHz(float v);
    static void setSync(bool v);
    static void setDivision(uint8_t v);
    static void setDepth(float v);
    static void setStereoPhaseDeg(float v);
    static void setSmoothingMs(float v);
    static void setTempoBpm(float bpm);
    static RTALModCoreParameters parameters();
    static RTALModCoreState state();
private:
    static float clampf(float v,float lo,float hi);
    static float divisionRateHz(uint8_t division,float bpm);
    static float waveAt(uint32_t phase,RTALModLfoShape shape);
    static void updateControl(size_t frames);
};
