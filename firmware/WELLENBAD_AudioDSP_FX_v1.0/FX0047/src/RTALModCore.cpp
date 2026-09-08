#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

#include "../include/RTALModCore.h"
#include <math.h>

namespace {
portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
RTALModCoreParameters gTarget{true,RTALModLfoShape::Sine,0.25f,false,3,0.50f,90.0f,50.0f};
RTALModCoreParameters gCurrent=gTarget;
uint32_t gSampleRate=44100;
float gTempoBpm=120.0f;
uint32_t gPhase=0;
uint32_t gIncrement=0;
uint32_t gStereoOffset=0x40000000UL; // 90 degrees
float gLastL=0.0f,gLastR=0.0f,gEffectiveRate=0.25f;
constexpr uint16_t LUT_BITS=10;
constexpr uint16_t LUT_SIZE=1u<<LUT_BITS;
float gSine[LUT_SIZE+1];
bool gLutReady=false;
}

float RTALModCore::clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}

float RTALModCore::divisionRateHz(uint8_t d,float bpm){
    // note length in quarter-note units; one complete LFO cycle per division
    static const float q[12]={4.0f,2.0f,1.0f,0.5f,0.25f,0.125f,1.5f,0.75f,0.375f,0.6666667f,0.3333333f,0.1666667f};
    if(d>11)d=11; bpm=clampf(bpm,30.0f,300.0f);
    return (bpm/60.0f)/q[d];
}

RTALStatus RTALModCore::begin(uint32_t sr){
    if(sr<8000)return RTALStatus::FATAL; gSampleRate=sr;
    for(uint16_t i=0;i<=LUT_SIZE;++i)gSine[i]=sinf(2.0f*PI*(float)i/(float)LUT_SIZE);
    gLutReady=true; gPhase=0; updateControl(1); return RTALStatus::OK;
}

float RTALModCore::waveAt(uint32_t p,RTALModLfoShape shape){
    if(shape==RTALModLfoShape::Triangle){
        const float x=(float)p*(1.0f/4294967296.0f);
        return 1.0f-4.0f*fabsf(x-0.5f);
    }
    if(!gLutReady)return 0.0f;
    const uint32_t idx=p>>(32-LUT_BITS);
    const uint32_t fracMask=(1UL<<(32-LUT_BITS))-1UL;
    const float frac=(float)(p&fracMask)/(float)(1UL<<(32-LUT_BITS));
    return gSine[idx]+(gSine[idx+1]-gSine[idx])*frac;
}

void RTALModCore::updateControl(size_t frames){
    RTALModCoreParameters t; float bpm;
    portENTER_CRITICAL(&gMux); t=gTarget; bpm=gTempoBpm; portEXIT_CRITICAL(&gMux);
    const float blockMs=1000.0f*(float)frames/(float)gSampleRate;
    const float tau=clampf(t.smoothingMs,5.0f,500.0f);
    const float a=clampf(blockMs/tau,0.0f,1.0f); // cheap monotonic control smoothing
    gCurrent.rateHz += (t.rateHz-gCurrent.rateHz)*a;
    gCurrent.depth += (t.depth-gCurrent.depth)*a;
    gCurrent.stereoPhaseDeg += (t.stereoPhaseDeg-gCurrent.stereoPhaseDeg)*a;
    gCurrent.smoothingMs=t.smoothingMs; gCurrent.shape=t.shape; gCurrent.sync=t.sync; gCurrent.division=t.division; gCurrent.enabled=t.enabled;
    gEffectiveRate=gCurrent.sync?divisionRateHz(gCurrent.division,bpm):clampf(gCurrent.rateHz,0.02f,10.0f);
    const double inc=(double)gEffectiveRate*4294967296.0/(double)gSampleRate;
    gIncrement=(uint32_t)(inc<1.0?1.0:inc);
    gStereoOffset=(uint32_t)((double)clampf(gCurrent.stereoPhaseDeg,0.0f,180.0f)*(4294967296.0/360.0));
}

void RTALModCore::nextStereo(float& l,float& r){
    const float d=gCurrent.enabled?clampf(gCurrent.depth,0.0f,1.0f):0.0f;
    l=waveAt(gPhase,gCurrent.shape)*d;
    r=waveAt(gPhase+gStereoOffset,gCurrent.shape)*d;
    gPhase+=gIncrement; gLastL=l;gLastR=r;
}

void RTALModCore::prepareBlock(size_t frames){
    if(!frames)return;
    updateControl(frames);
}

void RTALModCore::advanceBlock(size_t frames){
    if(!frames)return; updateControl(frames);
    // Infrastructure build: advance phase without per-sample DSP cost. The same
    // nextStereo() function will be called per sample when the first MOD FX is added.
    const uint64_t adv=(uint64_t)gIncrement*(uint64_t)frames;
    gPhase+=(uint32_t)adv;
    const float d=gCurrent.enabled?clampf(gCurrent.depth,0.0f,1.0f):0.0f;
    gLastL=waveAt(gPhase,gCurrent.shape)*d;
    gLastR=waveAt(gPhase+gStereoOffset,gCurrent.shape)*d;
}

#define SETTER(name,field,expr) void RTALModCore::name(decltype(gTarget.field) v){portENTER_CRITICAL(&gMux);gTarget.field=(expr);portEXIT_CRITICAL(&gMux);}
void RTALModCore::setEnabled(bool v){portENTER_CRITICAL(&gMux);gTarget.enabled=v;portEXIT_CRITICAL(&gMux);}
void RTALModCore::setShape(RTALModLfoShape v){portENTER_CRITICAL(&gMux);gTarget.shape=(v==RTALModLfoShape::Triangle)?v:RTALModLfoShape::Sine;portEXIT_CRITICAL(&gMux);}
void RTALModCore::setRateHz(float v){portENTER_CRITICAL(&gMux);gTarget.rateHz=clampf(v,0.02f,10.0f);portEXIT_CRITICAL(&gMux);}
void RTALModCore::setSync(bool v){portENTER_CRITICAL(&gMux);gTarget.sync=v;portEXIT_CRITICAL(&gMux);}
void RTALModCore::setDivision(uint8_t v){portENTER_CRITICAL(&gMux);gTarget.division=v>11?11:v;portEXIT_CRITICAL(&gMux);}
void RTALModCore::setDepth(float v){portENTER_CRITICAL(&gMux);gTarget.depth=clampf(v,0.0f,1.0f);portEXIT_CRITICAL(&gMux);}
void RTALModCore::setStereoPhaseDeg(float v){portENTER_CRITICAL(&gMux);gTarget.stereoPhaseDeg=clampf(v,0.0f,180.0f);portEXIT_CRITICAL(&gMux);}
void RTALModCore::setSmoothingMs(float v){portENTER_CRITICAL(&gMux);gTarget.smoothingMs=clampf(v,5.0f,500.0f);portEXIT_CRITICAL(&gMux);}
void RTALModCore::setTempoBpm(float v){portENTER_CRITICAL(&gMux);gTempoBpm=clampf(v,30.0f,300.0f);portEXIT_CRITICAL(&gMux);}
RTALModCoreParameters RTALModCore::parameters(){portENTER_CRITICAL(&gMux);auto c=gTarget;portEXIT_CRITICAL(&gMux);return c;}
RTALModCoreState RTALModCore::state(){RTALModCoreState s{gLastL,gLastR,gEffectiveRate,gPhase};return s;}
