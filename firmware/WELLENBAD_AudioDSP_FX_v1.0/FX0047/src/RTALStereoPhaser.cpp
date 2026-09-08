#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")

#include "../include/RTALStereoPhaser.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALModCore.h"
#include "../include/RTALMixLaw.h"
#include <math.h>
#include <string.h>

portMUX_TYPE RTALStereoPhaser::mux_=portMUX_INITIALIZER_UNLOCKED;
RTALStereoPhaserParameters RTALStereoPhaser::target_={};
RTALStereoPhaserParameters RTALStereoPhaser::blockTarget_={};
RTALStereoPhaserParameters RTALStereoPhaser::current_={};
RTALStereoPhaserStatistics RTALStereoPhaser::statistics_={};
uint32_t RTALStereoPhaser::sampleRate_=0;
float RTALStereoPhaser::mixStep_=0,RTALStereoPhaser::feedbackStep_=0,RTALStereoPhaser::saturationStep_=0,RTALStereoPhaser::centerStep_=0;
float RTALStereoPhaser::mixNorm_=1.0f,RTALStereoPhaser::mixNormStep_=0.0f;
float RTALStereoPhaser::hpfCoeff_=0;
float RTALStereoPhaser::hpfInLeft_=0,RTALStereoPhaser::hpfInRight_=0,RTALStereoPhaser::hpfOutLeft_=0,RTALStereoPhaser::hpfOutRight_=0;
float RTALStereoPhaser::stateLeft_[4]={},RTALStereoPhaser::stateRight_[4]={};
float RTALStereoPhaser::coeffLeft_[4]={},RTALStereoPhaser::coeffRight_[4]={};
float RTALStereoPhaser::coeffStepLeft_[4]={},RTALStereoPhaser::coeffStepRight_[4]={};
uint8_t RTALStereoPhaser::coeffPhase_=0;
uint64_t RTALStereoPhaser::localProcessedFrames_=0;
float RTALStereoPhaser::localPeakWetLeft_=0,RTALStereoPhaser::localPeakWetRight_=0;

float RTALStereoPhaser::clamp(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
float RTALStereoPhaser::softSaturate(float x,float amount){if(amount<=0.0001f)return x;const float y=clamp(x,-1.5f,1.5f);const float c=y-(y*y*y)*(1.0f/6.0f);return y+(c-y)*amount;}
float RTALStereoPhaser::processAllpass(float x,float a,float& z){const float y=z-a*x;z=x+a*y;return y;}

RTALStatus RTALStereoPhaser::begin(uint32_t sr){
 if(sr<8000)return RTALStatus::FATAL;sampleRate_=sr;
 target_={RTAL_PHASER_ENABLE_DEFAULT,RTAL_PHASER_MIX_DEFAULT,RTAL_PHASER_STAGES_DEFAULT,RTAL_PHASER_FEEDBACK_DEFAULT,RTAL_PHASER_FEEDBACK_HPF_HZ_DEFAULT,RTAL_PHASER_SATURATION_DEFAULT,RTAL_PHASER_CENTER_HZ_DEFAULT};
 blockTarget_=current_=target_; mixNorm_=rtalConstantPowerMixNorm(current_.mix); mixNormStep_=0.0f;statistics_={};reset();
 RTALLogger::printf(RTALLogLevel::Info,"StereoPhaser ... PASS enabled=%s mix=%.2f stages=%u fb=%+.2f hpf=%.0fHz sat=%.2f center=%.0fHz",target_.enabled?"ON":"OFF",(double)target_.mix,(unsigned)target_.stages,(double)target_.feedback,(double)target_.feedbackHpfHz,(double)target_.saturation,(double)target_.centerHz);
 return RTALStatus::OK;
}
void RTALStereoPhaser::reset(){
 memset(stateLeft_,0,sizeof(stateLeft_));memset(stateRight_,0,sizeof(stateRight_));
 memset(coeffLeft_,0,sizeof(coeffLeft_));memset(coeffRight_,0,sizeof(coeffRight_));
 memset(coeffStepLeft_,0,sizeof(coeffStepLeft_));memset(coeffStepRight_,0,sizeof(coeffStepRight_));
 coeffPhase_=0;hpfInLeft_=hpfInRight_=hpfOutLeft_=hpfOutRight_=0;
}
void RTALStereoPhaser::setEnabled(bool v){portENTER_CRITICAL(&mux_);target_.enabled=v;portEXIT_CRITICAL(&mux_);}
void RTALStereoPhaser::setMix(float v){portENTER_CRITICAL(&mux_);target_.mix=clamp(v,0,1);portEXIT_CRITICAL(&mux_);}
void RTALStereoPhaser::setStages(uint8_t v){
 // Build 0043b: final HQ phaser intentionally exposes only 2 or 4 stages.
 // Any legacy 6/8-stage request is safely mapped to 4 stages.
 v=(v<=2)?2:4;
 portENTER_CRITICAL(&mux_);target_.stages=v;portEXIT_CRITICAL(&mux_);
}
void RTALStereoPhaser::setFeedback(float v){portENTER_CRITICAL(&mux_);target_.feedback=clamp(v,-0.95f,0.95f);portEXIT_CRITICAL(&mux_);}
void RTALStereoPhaser::setFeedbackHpfHz(float v){portENTER_CRITICAL(&mux_);target_.feedbackHpfHz=clamp(v,RTAL_PHASER_FEEDBACK_HPF_HZ_MIN,RTAL_PHASER_FEEDBACK_HPF_HZ_MAX);portEXIT_CRITICAL(&mux_);}
void RTALStereoPhaser::setSaturation(float v){portENTER_CRITICAL(&mux_);target_.saturation=clamp(v,0,1);portEXIT_CRITICAL(&mux_);}
void RTALStereoPhaser::setCenterHz(float v){portENTER_CRITICAL(&mux_);target_.centerHz=clamp(v,RTAL_PHASER_CENTER_HZ_MIN,RTAL_PHASER_CENTER_HZ_MAX);portEXIT_CRITICAL(&mux_);}
RTALStereoPhaserParameters RTALStereoPhaser::parameters(){portENTER_CRITICAL(&mux_);auto c=target_;portEXIT_CRITICAL(&mux_);return c;}
void RTALStereoPhaser::flushLocalStatistics(){if(!localProcessedFrames_&&!localPeakWetLeft_&&!localPeakWetRight_)return;portENTER_CRITICAL(&mux_);statistics_.processedFrames+=localProcessedFrames_;if(localPeakWetLeft_>statistics_.peakWetLeft)statistics_.peakWetLeft=localPeakWetLeft_;if(localPeakWetRight_>statistics_.peakWetRight)statistics_.peakWetRight=localPeakWetRight_;portEXIT_CRITICAL(&mux_);localProcessedFrames_=0;localPeakWetLeft_=localPeakWetRight_=0;}
void RTALStereoPhaser::beginBlock(size_t frames){flushLocalStatistics();portENTER_CRITICAL(&mux_);blockTarget_=target_;++statistics_.processedBlocks;portEXIT_CRITICAL(&mux_);RTALModCore::prepareBlock(frames);current_.enabled=blockTarget_.enabled;current_.stages=blockTarget_.stages;if(frames){float inv=1.0f/(float)frames;mixStep_=(blockTarget_.mix-current_.mix)*inv;feedbackStep_=(blockTarget_.feedback-current_.feedback)*inv;saturationStep_=(blockTarget_.saturation-current_.saturation)*inv;centerStep_=(blockTarget_.centerHz-current_.centerHz)*inv;if(RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED&&fabsf(blockTarget_.mix-current_.mix)>0.000001f){const float targetNorm=rtalConstantPowerMixNorm(blockTarget_.mix);mixNormStep_=(targetNorm-mixNorm_)*inv;}else mixNormStep_=0.0f;}else mixStep_=feedbackStep_=saturationStep_=centerStep_=mixNormStep_=0;current_.feedbackHpfHz=blockTarget_.feedbackHpfHz;hpfCoeff_=expf(-2.0f*PI*clamp(current_.feedbackHpfHz,RTAL_PHASER_FEEDBACK_HPF_HZ_MIN,RTAL_PHASER_FEEDBACK_HPF_HZ_MAX)/(float)sampleRate_);}
void RTALStereoPhaser::process(float inL,float inR,float& outL,float& outR){
 outL=inL;outR=inR;
 current_.mix+=mixStep_;current_.feedback+=feedbackStep_;current_.saturation+=saturationStep_;current_.centerHz+=centerStep_;mixNorm_+=mixNormStep_;
 float lfoL=0,lfoR=0;RTALModCore::nextStereo(lfoL,lfoR);++localProcessedFrames_;
 if(!current_.enabled)return;

 // Build the allpass coefficient bank only every four samples.  Between
 // control points the coefficients are linearly interpolated.  At 44.1 kHz
 // this is an 11.025 kHz coefficient control rate, far above the 0.02-10 Hz
 // modulation bandwidth, but removes clamp/offset work from every stage on
 // every audio sample.
 if(coeffPhase_==0){
   const float centerNorm=clamp((current_.centerHz-RTAL_PHASER_CENTER_HZ_MIN)/(RTAL_PHASER_CENTER_HZ_MAX-RTAL_PHASER_CENTER_HZ_MIN),0,1);
   const float base=0.82f-centerNorm*0.50f;
   const float aL0=clamp(base-lfoL*0.24f,0.08f,0.92f);
   const float aR0=clamp(base-lfoR*0.24f,0.08f,0.92f);
   // Build 0043b: musically centered coefficient spreads for the two final HQ modes.
   // The former 4-stage path used the first four values of an 8-stage bank, placing
   // all stages on one side of the center.  The new symmetric distribution produces
   // a fuller, more balanced notch sweep while also preparing only the active stages.
   static const float offs2[2]={-0.075f,0.075f};
   static const float offs4[4]={-0.135f,-0.045f,0.045f,0.135f};
   const float* offs=(current_.stages==2)?offs2:offs4;
   const uint8_t activeStages=(current_.stages==2)?2:4;
   for(uint8_t i=0;i<activeStages;++i){
     const float targetL=clamp(aL0+offs[i],0.04f,0.96f);
     const float targetR=clamp(aR0+offs[i],0.04f,0.96f);
     coeffStepLeft_[i]=(targetL-coeffLeft_[i])*(1.0f/(float)kCoeffControlDiv);
     coeffStepRight_[i]=(targetR-coeffRight_[i])*(1.0f/(float)kCoeffControlDiv);
   }
 }
 const uint8_t activeStages=(current_.stages==2)?2:4;
 for(uint8_t i=0;i<activeStages;++i){coeffLeft_[i]+=coeffStepLeft_[i];coeffRight_[i]+=coeffStepRight_[i];}
 coeffPhase_=(uint8_t)((coeffPhase_+1U)&(kCoeffControlDiv-1U));

 const uint8_t last=(uint8_t)(current_.stages-1U);
 const float fbLraw=stateLeft_[last],fbRraw=stateRight_[last];
 const float hpL=hpfCoeff_*(hpfOutLeft_+fbLraw-hpfInLeft_);hpfInLeft_=fbLraw;hpfOutLeft_=hpL;
 const float hpR=hpfCoeff_*(hpfOutRight_+fbRraw-hpfInRight_);hpfInRight_=fbRraw;hpfOutRight_=hpR;
 const float fbL=hpL*current_.feedback,fbR=hpR*current_.feedback;
 float xL=inL+(current_.saturation>0.0001f?softSaturate(fbL,current_.saturation):fbL);
 float xR=inR+(current_.saturation>0.0001f?softSaturate(fbR,current_.saturation):fbR);

 // Fixed render paths let the compiler optimize the exact stage counts and
 // avoid a variable loop/control test in the hottest part of the phaser.
 #define RTAL_PH_STAGE(N) do{ xL=processAllpass(xL,coeffLeft_[N],stateLeft_[N]); xR=processAllpass(xR,coeffRight_[N],stateRight_[N]); }while(0)
 if(current_.stages==4){
   RTAL_PH_STAGE(0);RTAL_PH_STAGE(1);RTAL_PH_STAGE(2);RTAL_PH_STAGE(3);
 }else{
   RTAL_PH_STAGE(0);RTAL_PH_STAGE(1);
 }
 #undef RTAL_PH_STAGE

 const float wetL=xL,wetR=xR;
 const float m=current_.mix<0.0f?0.0f:(current_.mix>1.0f?1.0f:current_.mix);
 const float norm=RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED?mixNorm_:1.0f;
 outL=(inL+(wetL-inL)*m)*norm;outR=(inR+(wetR-inR)*m)*norm;
 float p=fabsf(wetL);if(p>localPeakWetLeft_)localPeakWetLeft_=p;p=fabsf(wetR);if(p>localPeakWetRight_)localPeakWetRight_=p;
}
RTALStereoPhaserStatistics RTALStereoPhaser::statistics(bool resetWindow){flushLocalStatistics();portENTER_CRITICAL(&mux_);auto c=statistics_;if(resetWindow){statistics_.processedFrames=0;statistics_.processedBlocks=0;statistics_.peakWetLeft=statistics_.peakWetRight=0;}portEXIT_CRITICAL(&mux_);return c;}
void RTALStereoPhaser::printReport(){auto p=parameters();auto s=statistics(true);RTALLogger::printf(RTALLogLevel::Info,"Phaser enabled=%s mix=%.2f stages=%u fb=%+.2f hpf=%.0fHz sat=%.2f center=%.0fHz wetPeak=%.3f/%.3f frames=%llu",p.enabled?"ON":"OFF",(double)p.mix,(unsigned)p.stages,(double)p.feedback,(double)p.feedbackHpfHz,(double)p.saturation,(double)p.centerHz,(double)s.peakWetLeft,(double)s.peakWetRight,(unsigned long long)s.processedFrames);}
