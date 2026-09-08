#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

#include "../include/RTALStereoReverb.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALMixLaw.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

// Build0046c: Peak-load smoothing on top of Build0046b O3/fast-math.
// Expensive derived/control math is now change-driven instead of every block,
// and statistics critical sections are decimated away from the audio hot path.
// - FDN lines live in INTERNAL RAM (hot random-access working set).
// - Predelay remains in PSRAM (large, sequential access).
// - One shared FDN write pointer; all four lines have equal buffer size.
// - No variable modulo/division in the per-sample predelay path.
// - Size/decay/predelay/damping-derived values are prepared once per block.
// - Wet-peak statistics are decimated; audio math is unchanged in topology.

float* RTALStereoReverb::line_[4]={nullptr,nullptr,nullptr,nullptr};
float* RTALStereoReverb::preL_=nullptr; float* RTALStereoReverb::preR_=nullptr;
uint32_t RTALStereoReverb::fdnWrite_=0; uint32_t RTALStereoReverb::preWrite_=0;
uint32_t RTALStereoReverb::preSamples_=0,RTALStereoReverb::predelayReadOffset_=0,RTALStereoReverb::sampleRate_=0;
uint32_t RTALStereoReverb::delaySamples_[4]={0,0,0,0};
float RTALStereoReverb::dampState_[4]={0,0,0,0},RTALStereoReverb::dampCoeff_=0,RTALStereoReverb::feedbackGain_=0;
float RTALStereoReverb::currentMix_=0,RTALStereoReverb::mixStep_=0,RTALStereoReverb::currentSize_=0;
float RTALStereoReverb::mixNorm_=1.0f,RTALStereoReverb::mixNormStep_=0.0f;
float RTALStereoReverb::currentDecay_=0,RTALStereoReverb::currentPredelayMs_=0,RTALStereoReverb::currentDampingHz_=0;
bool RTALStereoReverb::blockEnabled_=false;
portMUX_TYPE RTALStereoReverb::mux_=portMUX_INITIALIZER_UNLOCKED;
RTALStereoReverbParameters RTALStereoReverb::target_={},RTALStereoReverb::blockTarget_={};
RTALStereoReverbStatistics RTALStereoReverb::statistics_={};
uint64_t RTALStereoReverb::localFrames_=0; uint16_t RTALStereoReverb::localBlocks_=0; float RTALStereoReverb::localPeakL_=0,RTALStereoReverb::localPeakR_=0;
uint8_t RTALStereoReverb::statsDecim_=0;

float RTALStereoReverb::clamp(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}

RTALStatus RTALStereoReverb::begin(uint32_t sr){
 sampleRate_=sr;
 const size_t lineBytes=(size_t)RTAL_REVERB_LINE_BUFFER_SAMPLES*sizeof(float);
 // Hot FDN buffers: internal 8-bit RAM is much faster than PSRAM for four
 // independent reads + four writes on every audio sample.
 for(int i=0;i<4;i++){
   line_[i]=(float*)heap_caps_malloc(lineBytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
   if(!line_[i]) return RTALStatus::FATAL;
   memset(line_[i],0,lineBytes);
 }
 preSamples_=(uint32_t)ceilf(RTAL_REVERB_PREDELAY_MS_MAX*(float)sr/1000.0f)+2;
 const size_t preBytes=(size_t)preSamples_*sizeof(float);
 preL_=(float*)heap_caps_malloc(preBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 preR_=(float*)heap_caps_malloc(preBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!preL_||!preR_) return RTALStatus::FATAL;
 memset(preL_,0,preBytes); memset(preR_,0,preBytes);
 target_={RTAL_REVERB_ENABLE_DEFAULT,RTAL_REVERB_MIX_DEFAULT,RTAL_REVERB_SIZE_DEFAULT,RTAL_REVERB_DECAY_DEFAULT,RTAL_REVERB_DAMPING_HZ_DEFAULT,RTAL_REVERB_PREDELAY_MS_DEFAULT};
 blockTarget_=target_; currentMix_=target_.mix; mixNorm_=rtalConstantPowerMixNorm(currentMix_); mixNormStep_=0.0f; currentSize_=target_.size; currentDecay_=target_.decay; currentPredelayMs_=target_.predelayMs; currentDampingHz_=target_.dampingHz;
 reset(); updateDerived();
 RTALLogger::printf(RTALLogLevel::Info,"StereoReverb ... PASS enabled=%s mix=%.2f size=%.2f decay=%.2f damp=%.0fHz pre=%.0fms FDN=4 lines=INTERNAL pre=PSRAM",target_.enabled?"ON":"OFF",(double)target_.mix,(double)target_.size,(double)target_.decay,(double)target_.dampingHz,(double)target_.predelayMs);
 return RTALStatus::OK;
}
void RTALStereoReverb::reset(){
 fdnWrite_=0; preWrite_=0; statsDecim_=0;
 for(int i=0;i<4;i++){dampState_[i]=0;if(line_[i])memset(line_[i],0,(size_t)RTAL_REVERB_LINE_BUFFER_SAMPLES*sizeof(float));}
 if(preL_)memset(preL_,0,(size_t)preSamples_*sizeof(float));if(preR_)memset(preR_,0,(size_t)preSamples_*sizeof(float));
}
void RTALStereoReverb::setEnabled(bool v){portENTER_CRITICAL(&mux_);target_.enabled=v;portEXIT_CRITICAL(&mux_);}
void RTALStereoReverb::setMix(float v){portENTER_CRITICAL(&mux_);target_.mix=clamp(v,0,1);portEXIT_CRITICAL(&mux_);}
void RTALStereoReverb::setSize(float v){portENTER_CRITICAL(&mux_);target_.size=clamp(v,0,1);portEXIT_CRITICAL(&mux_);}
void RTALStereoReverb::setDecay(float v){portENTER_CRITICAL(&mux_);target_.decay=clamp(v,0,1);portEXIT_CRITICAL(&mux_);}
void RTALStereoReverb::setDampingHz(float v){portENTER_CRITICAL(&mux_);target_.dampingHz=clamp(v,RTAL_REVERB_DAMPING_HZ_MIN,RTAL_REVERB_DAMPING_HZ_MAX);portEXIT_CRITICAL(&mux_);}
void RTALStereoReverb::setPredelayMs(float v){portENTER_CRITICAL(&mux_);target_.predelayMs=clamp(v,0,RTAL_REVERB_PREDELAY_MS_MAX);portEXIT_CRITICAL(&mux_);}
RTALStereoReverbParameters RTALStereoReverb::parameters(){portENTER_CRITICAL(&mux_);auto c=target_;portEXIT_CRITICAL(&mux_);return c;}
void RTALStereoReverb::updateDerived(){
 static const uint16_t base[4]={2179,2633,3163,3761};
 const float scale=0.45f+0.55f*currentSize_;
 for(int i=0;i<4;i++){uint32_t d=(uint32_t)(base[i]*scale);if(d<127)d=127;if(d>=RTAL_REVERB_LINE_BUFFER_SAMPLES)d=RTAL_REVERB_LINE_BUFFER_SAMPLES-1;delaySamples_[i]=d;}
 feedbackGain_=0.50f+0.43f*currentDecay_;
 const float fc=clamp(currentDampingHz_,RTAL_REVERB_DAMPING_HZ_MIN,RTAL_REVERB_DAMPING_HZ_MAX);
 dampCoeff_=1.0f-expf(-2.0f*PI*fc/(float)sampleRate_);
 uint32_t pd=(uint32_t)(currentPredelayMs_*(float)sampleRate_/1000.0f);
 if(pd>=preSamples_)pd=preSamples_-1;
 predelayReadOffset_=pd;
}
void RTALStereoReverb::flushStats(){
 if(!localFrames_&&!localBlocks_&&!localPeakL_&&!localPeakR_)return;
 portENTER_CRITICAL(&mux_);
 statistics_.processedFrames+=localFrames_;
 statistics_.processedBlocks+=localBlocks_;
 if(localPeakL_>statistics_.peakWetLeft)statistics_.peakWetLeft=localPeakL_;
 if(localPeakR_>statistics_.peakWetRight)statistics_.peakWetRight=localPeakR_;
 portEXIT_CRITICAL(&mux_);
 localFrames_=0; localBlocks_=0; localPeakL_=localPeakR_=0;
}
void RTALStereoReverb::beginBlock(size_t frames){
 // Build0046c: publish diagnostics only every 16 blocks, not every block.
 if((localBlocks_ & 15u)==15u) flushStats();
 portENTER_CRITICAL(&mux_); blockTarget_=target_; portEXIT_CRITICAL(&mux_);
 ++localBlocks_;
 blockEnabled_=blockTarget_.enabled;
 if(!blockEnabled_){mixStep_=0;mixNormStep_=0;return;}

 if(frames){const float inv=1.0f/(float)frames;mixStep_=(blockTarget_.mix-currentMix_)*inv;if(RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED&&fabsf(blockTarget_.mix-currentMix_)>0.000001f){const float targetNorm=rtalConstantPowerMixNorm(blockTarget_.mix);mixNormStep_=(targetNorm-mixNorm_)*inv;}else mixNormStep_=0.0f;}else mixStep_=mixNormStep_=0;

 // Expensive derived math (including expf for damping) is change-driven.
 // Static effect settings therefore add no control spike to every audio block.
 bool derivedDirty=false;
 const float k=0.35f;
 float d=blockTarget_.size-currentSize_;
 if(fabsf(d)>0.0001f){currentSize_+=d*k; derivedDirty=true;} else currentSize_=blockTarget_.size;
 d=blockTarget_.decay-currentDecay_;
 if(fabsf(d)>0.0001f){currentDecay_+=d*k; derivedDirty=true;} else currentDecay_=blockTarget_.decay;
 d=blockTarget_.predelayMs-currentPredelayMs_;
 if(fabsf(d)>0.01f){currentPredelayMs_+=d*k; derivedDirty=true;} else currentPredelayMs_=blockTarget_.predelayMs;
 if(fabsf(blockTarget_.dampingHz-currentDampingHz_)>0.5f){currentDampingHz_=blockTarget_.dampingHz; derivedDirty=true;}
 if(derivedDirty) updateDerived();
}
bool RTALStereoReverb::activeForBlock(){return blockEnabled_;}
void RTALStereoReverb::process(float inL,float inR,float& outL,float& outR){
 currentMix_+=mixStep_; mixNorm_+=mixNormStep_;

 // Predelay: no '%' with a runtime divisor in the sample loop.
 preL_[preWrite_]=inL; preR_[preWrite_]=inR;
 const uint32_t pd=predelayReadOffset_;
 const uint32_t pr=(preWrite_>=pd)?(preWrite_-pd):(preWrite_+preSamples_-pd);
 const float pL=preL_[pr],pR=preR_[pr];
 if(++preWrite_>=preSamples_)preWrite_=0;

 // 4096 is a power of two. A mask is cheaper and explicit on ESP32-S3.
 constexpr uint32_t mask=RTAL_REVERB_LINE_BUFFER_SAMPLES-1u;
 const uint32_t w=fdnWrite_;
 const uint32_t r0=(w-delaySamples_[0])&mask;
 const uint32_t r1=(w-delaySamples_[1])&mask;
 const uint32_t r2=(w-delaySamples_[2])&mask;
 const uint32_t r3=(w-delaySamples_[3])&mask;
 float d0=line_[0][r0]; float d1=line_[1][r1]; float d2=line_[2][r2]; float d3=line_[3][r3];
 dampState_[0]+=dampCoeff_*(d0-dampState_[0]); d0=dampState_[0];
 dampState_[1]+=dampCoeff_*(d1-dampState_[1]); d1=dampState_[1];
 dampState_[2]+=dampCoeff_*(d2-dampState_[2]); d2=dampState_[2];
 dampState_[3]+=dampCoeff_*(d3-dampState_[3]); d3=dampState_[3];

 const float half=0.5f*(d0+d1+d2+d3);
 const float h0=d0-half, h1=d1-half, h2=d2-half, h3=d3-half;
 const float sumLR=pL+pR, diffLR=pL-pR;
 const float fb=feedbackGain_;
 line_[0][w]=0.36f*sumLR+fb*h0;
 line_[1][w]=0.36f*diffLR+fb*h1;
 line_[2][w]=-0.36f*diffLR+fb*h2;
 line_[3][w]=-0.36f*sumLR+fb*h3;
 fdnWrite_=(w+1u)&mask;

 const float wetL=0.50f*( d0+d1-d2-d3);
 const float wetR=0.50f*( d0-d1+d2-d3);
 const float m=currentMix_;
 const float norm=RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED?mixNorm_:1.0f;
 outL=(inL+(wetL-inL)*m)*norm; outR=(inR+(wetR-inR)*m)*norm;

 // Peak statistics do not need sample-rate resolution. 1/16 decimation keeps
 // the diagnostic useful while removing fabs/max work from 15 of 16 samples.
 if((statsDecim_++&15u)==0u){const float aL=fabsf(wetL),aR=fabsf(wetR);if(aL>localPeakL_)localPeakL_=aL;if(aR>localPeakR_)localPeakR_=aR;}
 ++localFrames_;
}
RTALStereoReverbStatistics RTALStereoReverb::statistics(bool resetWindow){flushStats();portENTER_CRITICAL(&mux_);auto c=statistics_;if(resetWindow)statistics_={};portEXIT_CRITICAL(&mux_);return c;}
void RTALStereoReverb::printReport(){auto p=parameters();auto st=statistics(true);RTALLogger::printf(RTALLogLevel::Info,"Reverb enabled=%s mix=%.2f size=%.2f decay=%.2f damp=%.0fHz pre=%.0fms FDN=4 wetPeak=%.3f/%.3f frames=%llu lines=INTERNAL",p.enabled?"ON":"OFF",(double)p.mix,(double)p.size,(double)p.decay,(double)p.dampingHz,(double)p.predelayMs,(double)st.peakWetLeft,(double)st.peakWetRight,(unsigned long long)st.processedFrames);}
