#include "../include/RTALDSPProcessor.h"
#include "../include/RTALAudioFormat.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include <string.h>
#include <math.h>

portMUX_TYPE RTALDSPProcessor::mux_ = portMUX_INITIALIZER_UNLOCKED;
RTALDSPParameters RTALDSPProcessor::target_ = {};
RTALDSPParameters RTALDSPProcessor::current_ = {};
RTALDSPStatistics RTALDSPProcessor::statistics_ = {};
RTALOutputLimiter RTALDSPProcessor::limiterLeft_;
RTALOutputLimiter RTALDSPProcessor::limiterRight_;

float RTALDSPProcessor::clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

RTALStatus RTALDSPProcessor::begin()
{
    target_.bypass = RTAL_DSP_BYPASS_DEFAULT;
    target_.dry = clamp01(RTAL_DSP_DRY_DEFAULT);
    target_.wet = clamp01(RTAL_DSP_WET_DEFAULT);
    current_ = target_;
    limiterLeft_.reset();
    limiterRight_.reset();
    portENTER_CRITICAL(&mux_);
    memset(&statistics_, 0, sizeof(statistics_));
    statistics_.limiterGainLeft = 1.0f;
    statistics_.limiterGainRight = 1.0f;
    portEXIT_CRITICAL(&mux_);
    return RTALStatus::OK;
}

void RTALDSPProcessor::setBypass(bool b) { portENTER_CRITICAL(&mux_); target_.bypass=b; portEXIT_CRITICAL(&mux_); }
void RTALDSPProcessor::setDry(float v) { portENTER_CRITICAL(&mux_); target_.dry=clamp01(v); portEXIT_CRITICAL(&mux_); }
void RTALDSPProcessor::setWet(float v) { portENTER_CRITICAL(&mux_); target_.wet=clamp01(v); portEXIT_CRITICAL(&mux_); }
RTALDSPParameters RTALDSPProcessor::parameters() { portENTER_CRITICAL(&mux_); auto c=target_; portEXIT_CRITICAL(&mux_); return c; }

void RTALDSPProcessor::updateSmoothedParameters()
{
    RTALDSPParameters t;
    portENTER_CRITICAL(&mux_); t=target_; portEXIT_CRITICAL(&mux_);
    current_.bypass=t.bypass;
    current_.dry += (t.dry-current_.dry)*RTAL_DSP_PARAMETER_SMOOTHING;
    current_.wet += (t.wet-current_.wet)*RTAL_DSP_PARAMETER_SMOOTHING;
}

int16_t RTALDSPProcessor::floatToInt16(float x, uint32_t& clips)
{
    if (x > 0.999969f) { x=0.999969f; ++clips; }
    else if (x < -1.0f) { x=-1.0f; ++clips; }
    long v=lrintf(x*RTAL_DSP_FLOAT_TO_INT16);
    if (v>32767) { v=32767; ++clips; }
    if (v<-32768) { v=-32768; ++clips; }
    return static_cast<int16_t>(v);
}

void RTALDSPProcessor::processBlock(int32_t* s, size_t frames)
{
    if (!s || !frames) return;
    RTALDSPStatistics local{};
    for (size_t i=0;i<frames;++i)
    {
        updateSmoothedParameters();
        const size_t li=i*2, ri=li+1;
        const float inL=static_cast<float>(RTALAudioFormat::containerToInt16(s[li]))*RTAL_DSP_INT16_TO_FLOAT;
        const float inR=static_cast<float>(RTALAudioFormat::containerToInt16(s[ri]))*RTAL_DSP_INT16_TO_FLOAT;
        const float aL=fabsf(inL), aR=fabsf(inR);
        if (aL>local.inputPeakLeft) local.inputPeakLeft=aL;
        if (aR>local.inputPeakRight) local.inputPeakRight=aR;

        float outL=inL, outR=inR;
        if (!current_.bypass)
        {
            const float wetL=inL; // effect insertion point
            const float wetR=inR;
            outL=current_.dry*inL + current_.wet*wetL;
            outR=current_.dry*inR + current_.wet*wetR;
        }
        if (RTAL_DSP_LIMITER_ENABLED)
        {
            outL=limiterLeft_.process(outL,local.limiterEventsLeft,local.hardClipsLeft);
            outR=limiterRight_.process(outR,local.limiterEventsRight,local.hardClipsRight);
        }
        const float oL=fabsf(outL), oR=fabsf(outR);
        if (oL>local.outputPeakLeft) local.outputPeakLeft=oL;
        if (oR>local.outputPeakRight) local.outputPeakRight=oR;
        uint32_t cL=0,cR=0;
        const int16_t yL=floatToInt16(outL,cL), yR=floatToInt16(outR,cR);
        local.hardClipsLeft+=cL; local.hardClipsRight+=cR;
        s[li]=RTALAudioFormat::int16ToContainer(yL);
        s[ri]=RTALAudioFormat::int16ToContainer(yR);
        ++local.processedFrames;
    }
    local.limiterGainLeft=limiterLeft_.gain();
    local.limiterGainRight=limiterRight_.gain();
    portENTER_CRITICAL(&mux_);
    statistics_.processedFrames += local.processedFrames;
    statistics_.limiterEventsLeft += local.limiterEventsLeft;
    statistics_.limiterEventsRight += local.limiterEventsRight;
    statistics_.hardClipsLeft += local.hardClipsLeft;
    statistics_.hardClipsRight += local.hardClipsRight;
    if (local.inputPeakLeft>statistics_.inputPeakLeft) statistics_.inputPeakLeft=local.inputPeakLeft;
    if (local.inputPeakRight>statistics_.inputPeakRight) statistics_.inputPeakRight=local.inputPeakRight;
    if (local.outputPeakLeft>statistics_.outputPeakLeft) statistics_.outputPeakLeft=local.outputPeakLeft;
    if (local.outputPeakRight>statistics_.outputPeakRight) statistics_.outputPeakRight=local.outputPeakRight;
    statistics_.limiterGainLeft=local.limiterGainLeft;
    statistics_.limiterGainRight=local.limiterGainRight;
    portEXIT_CRITICAL(&mux_);
}

RTALDSPStatistics RTALDSPProcessor::statistics(bool reset)
{
    portENTER_CRITICAL(&mux_);
    auto c=statistics_;
    if (reset)
    {
        float gl=statistics_.limiterGainLeft, gr=statistics_.limiterGainRight;
        memset(&statistics_,0,sizeof(statistics_));
        statistics_.limiterGainLeft=gl; statistics_.limiterGainRight=gr;
    }
    portEXIT_CRITICAL(&mux_);
    return c;
}

void RTALDSPProcessor::printReport()
{
    auto s=statistics(true); auto p=parameters();
    RTALLogger::printf(RTALLogLevel::Info,"DSP bypass=%s dry=%.3f wet=%.3f frames=%llu",p.bypass?"ON":"OFF",p.dry,p.wet,(unsigned long long)s.processedFrames);
    RTALLogger::printf(RTALLogLevel::Info,"DSP peak_in=%.5f/%.5f peak_out=%.5f/%.5f limiter_gain=%.5f/%.5f",s.inputPeakLeft,s.inputPeakRight,s.outputPeakLeft,s.outputPeakRight,s.limiterGainLeft,s.limiterGainRight);
    RTALLogger::printf(RTALLogLevel::Info,"DSP limiter_events=%lu/%lu hard_clips=%lu/%lu",(unsigned long)s.limiterEventsLeft,(unsigned long)s.limiterEventsRight,(unsigned long)s.hardClipsLeft,(unsigned long)s.hardClipsRight);
}
