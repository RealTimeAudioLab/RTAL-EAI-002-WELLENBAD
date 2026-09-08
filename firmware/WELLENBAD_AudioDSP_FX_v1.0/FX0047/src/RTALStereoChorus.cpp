#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

#include "../include/RTALStereoChorus.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALModCore.h"
#include "../include/RTALMixLaw.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

portMUX_TYPE RTALStereoChorus::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALStereoChorusParameters RTALStereoChorus::target_ = {};
RTALStereoChorusParameters RTALStereoChorus::blockTarget_ = {};
RTALStereoChorusParameters RTALStereoChorus::current_ = {};
RTALStereoChorusStatistics RTALStereoChorus::statistics_ = {};
float* RTALStereoChorus::bufferLeft_ = nullptr;
float* RTALStereoChorus::bufferRight_ = nullptr;
uint32_t RTALStereoChorus::bufferSamples_ = 0;
uint32_t RTALStereoChorus::writeIndex_ = 0;
uint32_t RTALStereoChorus::sampleRate_ = 0;
float RTALStereoChorus::toneStateLeft_ = 0.0f;
float RTALStereoChorus::toneStateRight_ = 0.0f;
float RTALStereoChorus::toneCoeff_ = 1.0f;
float RTALStereoChorus::sampleRatePerMs_ = 0.0f;
float RTALStereoChorus::mixStep_ = 0.0f;
float RTALStereoChorus::delayStep_ = 0.0f;
float RTALStereoChorus::mixNorm_ = 1.0f;
float RTALStereoChorus::mixNormStep_ = 0.0f;
uint64_t RTALStereoChorus::localProcessedFrames_ = 0;
float RTALStereoChorus::localPeakWetLeft_ = 0.0f;
uint16_t RTALStereoChorus::localProcessedBlocks_=0;
uint8_t RTALStereoChorus::statsDecim_=0;
uint8_t RTALStereoChorus::controlDecim_=0;
float RTALStereoChorus::localPeakWetRight_ = 0.0f;

float RTALStereoChorus::clamp(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}

float RTALStereoChorus::toneCoefficient(float hz){
    hz=clamp(hz,RTAL_CHORUS_TONE_HZ_MIN,RTAL_CHORUS_TONE_HZ_MAX);
    return 1.0f-expf(-2.0f*PI*hz/(float)sampleRate_);
}

RTALStatus RTALStereoChorus::begin(uint32_t sr){
    if(sr<8000)return RTALStatus::FATAL;
    sampleRate_=sr;
    // Headroom for both taps, modulation and Hermite neighbours.
    const float maximumMs=RTAL_CHORUS_BASE_DELAY_MS_MAX+RTAL_CHORUS_MAX_MOD_MS+4.0f;
    bufferSamples_=(uint32_t)ceilf(maximumMs*(float)sampleRate_/1000.0f)+8u;
    const size_t bytes=(size_t)bufferSamples_*sizeof(float);
    bool psram=false;
    bufferLeft_=(float*)heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    bufferRight_=(float*)heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!bufferLeft_||!bufferRight_){
        if(bufferLeft_)heap_caps_free(bufferLeft_); if(bufferRight_)heap_caps_free(bufferRight_);
        bufferLeft_=(float*)heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        bufferRight_=(float*)heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        psram=true;
    }
    if(!bufferLeft_||!bufferRight_){if(bufferLeft_)heap_caps_free(bufferLeft_);if(bufferRight_)heap_caps_free(bufferRight_);bufferLeft_=bufferRight_=nullptr;return RTALStatus::FATAL;}
    memset(bufferLeft_,0,bytes); memset(bufferRight_,0,bytes);
    target_={RTAL_CHORUS_ENABLE_DEFAULT,RTAL_CHORUS_MIX_DEFAULT,RTAL_CHORUS_BASE_DELAY_MS_DEFAULT,RTAL_CHORUS_TONE_HZ_DEFAULT};
    blockTarget_=current_=target_;
    statistics_={}; statistics_.allocatedBytes=bytes*2u; statistics_.usingPsram=psram;
    writeIndex_=0; toneStateLeft_=toneStateRight_=0.0f; toneCoeff_=toneCoefficient(current_.toneHz);
    sampleRatePerMs_=(float)sampleRate_/1000.0f;
    mixNorm_=rtalConstantPowerMixNorm(current_.mix); mixNormStep_=0.0f;
    mixStep_=delayStep_=0.0f; localProcessedFrames_=0; localProcessedBlocks_=0; statsDecim_=0; controlDecim_=0; localPeakWetLeft_=localPeakWetRight_=0.0f;
    RTALLogger::printf(RTALLogLevel::Info,
        "StereoChorus ... PASS enabled=%s mix=%.2f delay=%.1fms tone=%.0fHz interp=A:HERMITE+B:LINEAR taps=2/ch bytes=%lu mem=%s",
        target_.enabled?"ON":"OFF",(double)target_.mix,(double)target_.baseDelayMs,(double)target_.toneHz,
        (unsigned long)statistics_.allocatedBytes,psram?"PSRAM":"INTERNAL");
    return RTALStatus::OK;
}

void RTALStereoChorus::reset(){
    if(!bufferLeft_||!bufferRight_)return; const size_t bytes=(size_t)bufferSamples_*sizeof(float);
    memset(bufferLeft_,0,bytes);memset(bufferRight_,0,bytes);writeIndex_=0;toneStateLeft_=toneStateRight_=0.0f;
}

void RTALStereoChorus::setEnabled(bool v){portENTER_CRITICAL(&mux_);target_.enabled=v;portEXIT_CRITICAL(&mux_);}
void RTALStereoChorus::setMix(float v){portENTER_CRITICAL(&mux_);target_.mix=clamp(v,0.0f,1.0f);portEXIT_CRITICAL(&mux_);}
void RTALStereoChorus::setBaseDelayMs(float v){portENTER_CRITICAL(&mux_);target_.baseDelayMs=clamp(v,RTAL_CHORUS_BASE_DELAY_MS_MIN,RTAL_CHORUS_BASE_DELAY_MS_MAX);portEXIT_CRITICAL(&mux_);}
void RTALStereoChorus::setToneHz(float v){portENTER_CRITICAL(&mux_);target_.toneHz=clamp(v,RTAL_CHORUS_TONE_HZ_MIN,RTAL_CHORUS_TONE_HZ_MAX);portEXIT_CRITICAL(&mux_);}
RTALStereoChorusParameters RTALStereoChorus::parameters(){portENTER_CRITICAL(&mux_);auto c=target_;portEXIT_CRITICAL(&mux_);return c;}

void RTALStereoChorus::flushLocalStatistics(){
    if(localProcessedFrames_==0 && localProcessedBlocks_==0 && localPeakWetLeft_==0.0f && localPeakWetRight_==0.0f) return;
    portENTER_CRITICAL(&mux_);
    statistics_.processedFrames += localProcessedFrames_;
    statistics_.processedBlocks += localProcessedBlocks_;
    if(localPeakWetLeft_>statistics_.peakWetLeft) statistics_.peakWetLeft=localPeakWetLeft_;
    if(localPeakWetRight_>statistics_.peakWetRight) statistics_.peakWetRight=localPeakWetRight_;
    portEXIT_CRITICAL(&mux_);
    localProcessedFrames_=0; localProcessedBlocks_=0;
    localPeakWetLeft_=localPeakWetRight_=0.0f;
}

void RTALStereoChorus::beginBlock(size_t frames){
    // Build0046c: avoid control/statistics work colliding with reverb updates.
    // Statistics are published only every 16 audio blocks.
    if((localProcessedBlocks_ & 15u)==15u) flushLocalStatistics();
    portENTER_CRITICAL(&mux_); blockTarget_=target_; portEXIT_CRITICAL(&mux_);
    ++localProcessedBlocks_;

    // ModCore control coefficients only need control-rate refresh. Updating every
    // four blocks (~11.6 ms at 44.1 kHz/128) keeps the LFO sample path unchanged
    // while removing three of four block-control critical/double calculations.
    if((controlDecim_++ & 3u)==0u) RTALModCore::prepareBlock(frames*4u);

    // expf() for the tone one-pole is required only when Tone actually changes.
    if(fabsf(blockTarget_.toneHz-current_.toneHz)>0.01f)
        toneCoeff_=toneCoefficient(blockTarget_.toneHz);

    current_.enabled=blockTarget_.enabled;
    current_.toneHz=blockTarget_.toneHz;
    if(frames){
        const float inv=1.0f/(float)frames;
        mixStep_=(blockTarget_.mix-current_.mix)*inv;
        delayStep_=(blockTarget_.baseDelayMs-current_.baseDelayMs)*inv;
        if (RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED && fabsf(blockTarget_.mix-current_.mix)>0.000001f) {
            const float targetNorm=rtalConstantPowerMixNorm(blockTarget_.mix);
            mixNormStep_=(targetNorm-mixNorm_)*inv;
        } else mixNormStep_=0.0f;
    } else {
        mixStep_=delayStep_=mixNormStep_=0.0f;
    }
}

void RTALStereoChorus::updateSmoothedParameters(){
    current_.mix += mixStep_;
    current_.baseDelayMs += delayStep_;
    mixNorm_ += mixNormStep_;
}

float RTALStereoChorus::readHermite(const float* b,uint32_t w,float d){
    float p=(float)w-d;
    while(p<0.0f)p+=(float)bufferSamples_;
    while(p>=(float)bufferSamples_)p-=(float)bufferSamples_;
    const int32_t i1=(int32_t)p;
    const float x=p-(float)i1;
    const uint32_t u1=(uint32_t)i1;
    const uint32_t u0=(u1==0)?bufferSamples_-1:u1-1;
    const uint32_t u2=(u1+1u>=bufferSamples_)?0u:u1+1u;
    const uint32_t u3=(u2+1u>=bufferSamples_)?0u:u2+1u;
    const float y0=b[u0],y1=b[u1],y2=b[u2],y3=b[u3];
    const float c0=y1;
    const float c1=0.5f*(y2-y0);
    const float c2=y0-2.5f*y1+2.0f*y2-0.5f*y3;
    const float c3=0.5f*(y3-y0)+1.5f*(y1-y2);
    return ((c3*x+c2)*x+c1)*x+c0;
}

float RTALStereoChorus::readLinear(const float* b,uint32_t w,float d){
    float p=(float)w-d;
    while(p<0.0f)p+=(float)bufferSamples_;
    while(p>=(float)bufferSamples_)p-=(float)bufferSamples_;
    const uint32_t i0=(uint32_t)p;
    const uint32_t i1=(i0+1u>=bufferSamples_)?0u:i0+1u;
    const float x=p-(float)i0;
    return b[i0]+(b[i1]-b[i0])*x;
}

void RTALStereoChorus::process(float inL,float inR,float& outL,float& outR){
    outL=inL;outR=inR;
    if(!bufferLeft_||!bufferRight_)return;
    updateSmoothedParameters();

    // Keep phase/history warm, but when disabled skip all delay interpolation,
    // tone filtering and wet-path arithmetic. This makes OFF close to Build0039 load.
    float lfoL=0.0f,lfoR=0.0f; RTALModCore::nextStereo(lfoL,lfoR);
    bufferLeft_[writeIndex_]=inL; bufferRight_[writeIndex_]=inR;
    ++localProcessedFrames_;

    if(!current_.enabled){
        if(++writeIndex_>=bufferSamples_)writeIndex_=0;
        return;
    }

    const float base=current_.baseDelayMs*sampleRatePerMs_;
    const float excursion=RTAL_CHORUS_MAX_MOD_MS*sampleRatePerMs_;
    const float d1L=base+excursion*lfoL;
    const float d1R=base+excursion*lfoR;
    // A second, asymmetrical counter-moving tap creates density without a
    // second independent oscillator (and therefore no stereo drift).
    const float d2Base=current_.baseDelayMs*0.70f*sampleRatePerMs_;
    const float d2L=d2Base-excursion*0.65f*lfoL;
    const float d2R=d2Base-excursion*0.65f*lfoR;

    // Build0041b hybrid interpolation: keep the primary tap at Hermite quality,
    // while the density tap uses linear interpolation to cut real-time cost.
    const float tapL=RTAL_CHORUS_TAP_A_GAIN*readHermite(bufferLeft_,writeIndex_,d1L)+
                     RTAL_CHORUS_TAP_B_GAIN*readLinear(bufferLeft_,writeIndex_,d2L);
    const float tapR=RTAL_CHORUS_TAP_A_GAIN*readHermite(bufferRight_,writeIndex_,d1R)+
                     RTAL_CHORUS_TAP_B_GAIN*readLinear(bufferRight_,writeIndex_,d2R);
    toneStateLeft_ += toneCoeff_*(tapL-toneStateLeft_);
    toneStateRight_ += toneCoeff_*(tapR-toneStateRight_);

    const float wetL=toneStateLeft_, wetR=toneStateRight_;
    const float m=clamp(current_.mix,0.0f,1.0f);
    const float norm=RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED?mixNorm_:1.0f;
    outL=(inL+(wetL-inL)*m)*norm;
    outR=(inR+(wetR-inR)*m)*norm;

    if((statsDecim_++ & 15u)==0u){
        const float aL=fabsf(wetL),aR=fabsf(wetR);
        if(aL>localPeakWetLeft_)localPeakWetLeft_=aL;
        if(aR>localPeakWetRight_)localPeakWetRight_=aR;
    }
    if(++writeIndex_>=bufferSamples_)writeIndex_=0;
}

RTALStereoChorusStatistics RTALStereoChorus::statistics(bool resetWindow){
    flushLocalStatistics();
    portENTER_CRITICAL(&mux_);auto c=statistics_;if(resetWindow){statistics_.processedFrames=0;statistics_.processedBlocks=0;statistics_.peakWetLeft=statistics_.peakWetRight=0.0f;}portEXIT_CRITICAL(&mux_);return c;
}
void RTALStereoChorus::printReport(){const auto p=parameters();const auto st=statistics(true);RTALLogger::printf(RTALLogLevel::Info,"Chorus enabled=%s mix=%.2f delay=%.1fms tone=%.0fHz wetPeak=%.3f/%.3f frames=%llu",p.enabled?"ON":"OFF",(double)p.mix,(double)p.baseDelayMs,(double)p.toneHz,(double)st.peakWetLeft,(double)st.peakWetRight,(unsigned long long)st.processedFrames);}
