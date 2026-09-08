#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

#include "../include/RTALStereoFlanger.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALModCore.h"
#include "../include/RTALMixLaw.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

portMUX_TYPE RTALStereoFlanger::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALStereoFlangerParameters RTALStereoFlanger::target_ = {};
RTALStereoFlangerParameters RTALStereoFlanger::blockTarget_ = {};
RTALStereoFlangerParameters RTALStereoFlanger::current_ = {};
RTALStereoFlangerStatistics RTALStereoFlanger::statistics_ = {};
float* RTALStereoFlanger::bufferLeft_ = nullptr;
float* RTALStereoFlanger::bufferRight_ = nullptr;
uint32_t RTALStereoFlanger::bufferSamples_ = 0;
uint32_t RTALStereoFlanger::writeIndex_ = 0;
uint32_t RTALStereoFlanger::sampleRate_ = 0;
float RTALStereoFlanger::sampleRatePerMs_ = 0.0f;
float RTALStereoFlanger::mixStep_ = 0.0f;
float RTALStereoFlanger::delayStep_ = 0.0f;
float RTALStereoFlanger::feedbackStep_ = 0.0f;
float RTALStereoFlanger::saturationStep_ = 0.0f;
float RTALStereoFlanger::mixNorm_ = 1.0f;
float RTALStereoFlanger::mixNormStep_ = 0.0f;
float RTALStereoFlanger::hpfCoeff_ = 0.0f;
float RTALStereoFlanger::hpfInLeft_ = 0.0f;
float RTALStereoFlanger::hpfInRight_ = 0.0f;
float RTALStereoFlanger::hpfOutLeft_ = 0.0f;
float RTALStereoFlanger::hpfOutRight_ = 0.0f;
uint64_t RTALStereoFlanger::localProcessedFrames_ = 0;
float RTALStereoFlanger::localPeakWetLeft_ = 0.0f;
float RTALStereoFlanger::localPeakWetRight_ = 0.0f;

float RTALStereoFlanger::clamp(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}

RTALStatus RTALStereoFlanger::begin(uint32_t sr){
    if(sr<8000)return RTALStatus::FATAL;
    sampleRate_=sr;
    const float maximumMs=RTAL_FLANGER_DELAY_MS_MAX+RTAL_FLANGER_MAX_MOD_MS+2.0f;
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
    target_={RTAL_FLANGER_ENABLE_DEFAULT,RTAL_FLANGER_MIX_DEFAULT,RTAL_FLANGER_DELAY_MS_DEFAULT,
             RTAL_FLANGER_FEEDBACK_DEFAULT,RTAL_FLANGER_FEEDBACK_HPF_HZ_DEFAULT,RTAL_FLANGER_SATURATION_DEFAULT};
    blockTarget_=current_=target_;
    mixNorm_=rtalConstantPowerMixNorm(current_.mix); mixNormStep_=0.0f;
    statistics_={}; statistics_.allocatedBytes=bytes*2u; statistics_.usingPsram=psram;
    writeIndex_=0; sampleRatePerMs_=(float)sampleRate_/1000.0f;
    hpfInLeft_=hpfInRight_=hpfOutLeft_=hpfOutRight_=0.0f;
    localProcessedFrames_=0;localPeakWetLeft_=localPeakWetRight_=0.0f;
    RTALLogger::printf(RTALLogLevel::Info,
        "StereoFlanger .. PASS enabled=%s mix=%.2f delay=%.2fms fb=%+.2f hpf=%.0fHz sat=%.2f interp=HERMITE bytes=%lu mem=%s",
        target_.enabled?"ON":"OFF",(double)target_.mix,(double)target_.baseDelayMs,(double)target_.feedback,
        (double)target_.feedbackHpfHz,(double)target_.saturation,(unsigned long)statistics_.allocatedBytes,psram?"PSRAM":"INTERNAL");
    return RTALStatus::OK;
}

void RTALStereoFlanger::reset(){
    if(!bufferLeft_||!bufferRight_)return;
    const size_t bytes=(size_t)bufferSamples_*sizeof(float);memset(bufferLeft_,0,bytes);memset(bufferRight_,0,bytes);
    writeIndex_=0;hpfInLeft_=hpfInRight_=hpfOutLeft_=hpfOutRight_=0.0f;
}

void RTALStereoFlanger::setEnabled(bool v){portENTER_CRITICAL(&mux_);target_.enabled=v;portEXIT_CRITICAL(&mux_);}
void RTALStereoFlanger::setMix(float v){portENTER_CRITICAL(&mux_);target_.mix=clamp(v,0.0f,1.0f);portEXIT_CRITICAL(&mux_);}
void RTALStereoFlanger::setBaseDelayMs(float v){portENTER_CRITICAL(&mux_);target_.baseDelayMs=clamp(v,RTAL_FLANGER_DELAY_MS_MIN,RTAL_FLANGER_DELAY_MS_MAX);portEXIT_CRITICAL(&mux_);}
void RTALStereoFlanger::setFeedback(float v){portENTER_CRITICAL(&mux_);target_.feedback=clamp(v,-0.95f,0.95f);portEXIT_CRITICAL(&mux_);}
void RTALStereoFlanger::setFeedbackHpfHz(float v){portENTER_CRITICAL(&mux_);target_.feedbackHpfHz=clamp(v,RTAL_FLANGER_FEEDBACK_HPF_HZ_MIN,RTAL_FLANGER_FEEDBACK_HPF_HZ_MAX);portEXIT_CRITICAL(&mux_);}
void RTALStereoFlanger::setSaturation(float v){portENTER_CRITICAL(&mux_);target_.saturation=clamp(v,0.0f,1.0f);portEXIT_CRITICAL(&mux_);}
RTALStereoFlangerParameters RTALStereoFlanger::parameters(){portENTER_CRITICAL(&mux_);auto c=target_;portEXIT_CRITICAL(&mux_);return c;}

void RTALStereoFlanger::flushLocalStatistics(){
    if(localProcessedFrames_==0&&localPeakWetLeft_==0.0f&&localPeakWetRight_==0.0f)return;
    portENTER_CRITICAL(&mux_);statistics_.processedFrames+=localProcessedFrames_;
    if(localPeakWetLeft_>statistics_.peakWetLeft)statistics_.peakWetLeft=localPeakWetLeft_;
    if(localPeakWetRight_>statistics_.peakWetRight)statistics_.peakWetRight=localPeakWetRight_;portEXIT_CRITICAL(&mux_);
    localProcessedFrames_=0;localPeakWetLeft_=localPeakWetRight_=0.0f;
}

void RTALStereoFlanger::beginBlock(size_t frames){
    flushLocalStatistics();
    portENTER_CRITICAL(&mux_);blockTarget_=target_;++statistics_.processedBlocks;portEXIT_CRITICAL(&mux_);
    RTALModCore::prepareBlock(frames);
    current_.enabled=blockTarget_.enabled;
    if(frames){
        const float inv=1.0f/(float)frames;
        mixStep_=(blockTarget_.mix-current_.mix)*inv;
        delayStep_=(blockTarget_.baseDelayMs-current_.baseDelayMs)*inv;
        feedbackStep_=(blockTarget_.feedback-current_.feedback)*inv;
        saturationStep_=(blockTarget_.saturation-current_.saturation)*inv;
        if (RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED && fabsf(blockTarget_.mix-current_.mix)>0.000001f) {
            const float targetNorm=rtalConstantPowerMixNorm(blockTarget_.mix);
            mixNormStep_=(targetNorm-mixNorm_)*inv;
        } else mixNormStep_=0.0f;
    }else mixStep_=delayStep_=feedbackStep_=saturationStep_=mixNormStep_=0.0f;
    current_.feedbackHpfHz=blockTarget_.feedbackHpfHz;
    hpfCoeff_=expf(-2.0f*PI*clamp(current_.feedbackHpfHz,RTAL_FLANGER_FEEDBACK_HPF_HZ_MIN,RTAL_FLANGER_FEEDBACK_HPF_HZ_MAX)/(float)sampleRate_);
}

float RTALStereoFlanger::readHermite(const float* b,uint32_t w,float d){
    float p=(float)w-d;while(p<0.0f)p+=(float)bufferSamples_;while(p>=(float)bufferSamples_)p-=(float)bufferSamples_;
    const int32_t i1=(int32_t)p;const float x=p-(float)i1;const uint32_t u1=(uint32_t)i1;
    const uint32_t u0=(u1==0)?bufferSamples_-1:u1-1;const uint32_t u2=(u1+1u>=bufferSamples_)?0u:u1+1u;const uint32_t u3=(u2+1u>=bufferSamples_)?0u:u2+1u;
    const float y0=b[u0],y1=b[u1],y2=b[u2],y3=b[u3];const float c0=y1;const float c1=0.5f*(y2-y0);
    const float c2=y0-2.5f*y1+2.0f*y2-0.5f*y3;const float c3=0.5f*(y3-y0)+1.5f*(y1-y2);
    return ((c3*x+c2)*x+c1)*x+c0;
}

float RTALStereoFlanger::softSaturate(float x,float amount){
    if(amount<=0.0001f)return x;
    const float y=clamp(x,-1.5f,1.5f);const float cubic=y-(y*y*y)*(1.0f/6.0f);
    return y+(cubic-y)*amount;
}

void RTALStereoFlanger::process(float inL,float inR,float& outL,float& outR){
    outL=inL;outR=inR;if(!bufferLeft_||!bufferRight_)return;
    current_.mix+=mixStep_;current_.baseDelayMs+=delayStep_;current_.feedback+=feedbackStep_;
    current_.saturation+=saturationStep_;
    mixNorm_+=mixNormStep_;
    float lfoL=0.0f,lfoR=0.0f;RTALModCore::nextStereo(lfoL,lfoR);
    ++localProcessedFrames_;
    if(!current_.enabled){bufferLeft_[writeIndex_]=inL;bufferRight_[writeIndex_]=inR;if(++writeIndex_>=bufferSamples_)writeIndex_=0;return;}

    // RTAL ModCore depth is already applied to lfoL/lfoR. Map -1..+1 around base delay.
    const float excursion=RTAL_FLANGER_MAX_MOD_MS*sampleRatePerMs_;
    const float base=current_.baseDelayMs*sampleRatePerMs_;
    const float minSamples=RTAL_FLANGER_DELAY_MS_MIN*sampleRatePerMs_;
    const float maxSamples=(RTAL_FLANGER_DELAY_MS_MAX+RTAL_FLANGER_MAX_MOD_MS)*sampleRatePerMs_;
    const float dL=clamp(base+excursion*lfoL,minSamples,maxSamples);
    const float dR=clamp(base+excursion*lfoR,minSamples,maxSamples);
    const float wetL=readHermite(bufferLeft_,writeIndex_,dL);
    const float wetR=readHermite(bufferRight_,writeIndex_,dR);

    // One-pole feedback HPF: y[n] = a * (y[n-1] + x[n] - x[n-1]).
    const float fbHpfL=hpfCoeff_*(hpfOutLeft_+wetL-hpfInLeft_);
    const float fbHpfR=hpfCoeff_*(hpfOutRight_+wetR-hpfInRight_);
    hpfInLeft_=wetL;hpfInRight_=wetR;hpfOutLeft_=fbHpfL;hpfOutRight_=fbHpfR;
    const float fbL=softSaturate(fbHpfL*current_.feedback,current_.saturation);
    const float fbR=softSaturate(fbHpfR*current_.feedback,current_.saturation);
    bufferLeft_[writeIndex_]=clamp(inL+fbL,-1.5f,1.5f);
    bufferRight_[writeIndex_]=clamp(inR+fbR,-1.5f,1.5f);

    const float m=clamp(current_.mix,0.0f,1.0f);
    const float norm=RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED?mixNorm_:1.0f;
    outL=(inL+(wetL-inL)*m)*norm;outR=(inR+(wetR-inR)*m)*norm;
    const float aL=fabsf(wetL),aR=fabsf(wetR);if(aL>localPeakWetLeft_)localPeakWetLeft_=aL;if(aR>localPeakWetRight_)localPeakWetRight_=aR;
    if(++writeIndex_>=bufferSamples_)writeIndex_=0;
}

RTALStereoFlangerStatistics RTALStereoFlanger::statistics(bool resetWindow){flushLocalStatistics();portENTER_CRITICAL(&mux_);auto c=statistics_;if(resetWindow){statistics_.processedFrames=0;statistics_.processedBlocks=0;statistics_.peakWetLeft=statistics_.peakWetRight=0.0f;}portEXIT_CRITICAL(&mux_);return c;}
void RTALStereoFlanger::printReport(){const auto p=parameters();const auto s=statistics(true);RTALLogger::printf(RTALLogLevel::Info,"Flanger enabled=%s mix=%.2f delay=%.2fms fb=%+.2f hpf=%.0fHz sat=%.2f wetPeak=%.3f/%.3f frames=%llu",p.enabled?"ON":"OFF",(double)p.mix,(double)p.baseDelayMs,(double)p.feedback,(double)p.feedbackHpfHz,(double)p.saturation,(double)s.peakWetLeft,(double)s.peakWetRight,(unsigned long long)s.processedFrames);}
