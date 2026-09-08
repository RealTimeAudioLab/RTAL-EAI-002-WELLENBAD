#include "../include/RTALFxLink.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALStereoDelay.h"
#include "../include/RTALModCore.h"
#include "../include/RTALStereoChorus.h"
#include "../include/RTALStereoFlanger.h"
#include "../include/RTALStereoPhaser.h"
#include "../include/RTALStereoWidth.h"
#include "../include/RTALStereoReverb.h"
#include "../include/RTALMidiClock.h"
#include "../include/RTALPresetState.h"
#include <string.h>

using namespace RTALFxProtocol;
static HardwareSerial gLinkSerial(2);
static bool gReady=false,gConnected=false;
static uint8_t gSeq=0,gRxBuf[sizeof(Frame)]={}; static size_t gRxPos=0;
static uint32_t gTx=0,gRx=0,gCrc=0,gVer=0,gLastRx=0,gHello=0,gSet=0;
static uint8_t gClockSource=0; static uint32_t gBpmX100=12000;
// Build0046d diagnostics: correlate an observed raw-input freeze with the
// most recent mutually-exclusive modulation-effect selection change.
static uint32_t gModFxSwitches=0,gLastModFxSwitchMs=0;
static uint8_t gLastModFxFrom=0,gLastModFxTo=0;

uint8_t RTALFxLink::crc8(const uint8_t*d,size_t n){uint8_t c=0;for(size_t i=0;i<n;++i){c^=d[i];for(uint8_t b=0;b<8;++b)c=(c&0x80)?(uint8_t)((c<<1)^0x07):(uint8_t)(c<<1);}return c;}
int32_t RTALFxLink::floatToQ16(float v){return (int32_t)(v*65536.0f+(v>=0?0.5f:-0.5f));}
float RTALFxLink::q16ToFloat(int32_t v){return (float)v/65536.0f;}


static uint8_t currentModFxSelection(){
 const auto ch=RTALStereoChorus::parameters();
 const auto fl=RTALStereoFlanger::parameters();
 const auto ph=RTALStereoPhaser::parameters();
 if(ph.enabled) return 3;
 if(fl.enabled) return 2;
 if(ch.enabled) return 1;
 return 0;
}

static void applyModFxSelection(uint8_t sel){
 if(sel>3) sel=0;
 RTALStereoChorus::setEnabled(sel==1);
 RTALStereoFlanger::setEnabled(sel==2);
 RTALStereoPhaser::setEnabled(sel==3);
}

static void recordModFxSwitch(uint8_t before){
 const uint8_t after=currentModFxSelection();
 if(after==before)return;
 ++gModFxSwitches;
 gLastModFxFrom=before;
 gLastModFxTo=after;
 gLastModFxSwitchMs=millis();
}
RTALStatus RTALFxLink::begin(){
 if(!RTAL_FX_LINK_ENABLED)return RTALStatus::OK;
 gLinkSerial.begin(RTAL_FX_LINK_BAUD,SERIAL_8N1,RTAL_FX_LINK_RX_PIN,RTAL_FX_LINK_TX_PIN);
 gReady=true;
 RTALLogger::printf(RTALLogLevel::Info,"FXLink ........ PASS RX=%d TX=%d baud=%lu protocol=%u",(int)RTAL_FX_LINK_RX_PIN,(int)RTAL_FX_LINK_TX_PIN,(unsigned long)RTAL_FX_LINK_BAUD,(unsigned)VERSION);
 return RTALStatus::OK;
}

void RTALFxLink::sendFrame(uint8_t type,uint8_t param,int32_t value){if(!gReady)return;Frame f{SOF1,SOF2,VERSION,type,++gSeq,param,value,0};const uint8_t*r=(const uint8_t*)&f;f.crc=crc8(r+2,sizeof(Frame)-3);gLinkSerial.write((const uint8_t*)&f,sizeof(f));++gTx;}

void RTALFxLink::sendState(){
 const RTALStereoDelayParameters p=RTALStereoDelay::parameters();
 sendFrame(STATE,P_DELAY_ENABLE,p.enabled?1:0); sendFrame(STATE,P_DELAY_FREEZE,p.freeze?1:0);
 sendFrame(STATE,P_DELAY_LEFT_MS,floatToQ16(p.timeLeftMs)); sendFrame(STATE,P_DELAY_RIGHT_MS,floatToQ16(p.timeRightMs));
 sendFrame(STATE,P_DELAY_FEEDBACK,floatToQ16(p.feedback)); sendFrame(STATE,P_DELAY_LEVEL,floatToQ16(p.level));
 sendFrame(STATE,P_DELAY_LOWPASS_HZ,floatToQ16(p.feedbackLowpassHz)); sendFrame(STATE,P_DELAY_HIGHPASS_HZ,floatToQ16(p.feedbackHighpassHz));
 sendFrame(STATE,P_DELAY_SATURATION,floatToQ16(p.feedbackSaturation)); sendFrame(STATE,P_DELAY_CROSSFEED,floatToQ16(p.crossfeed));
 sendFrame(STATE,P_DELAY_DUCK_AMOUNT,floatToQ16(p.duckAmount)); sendFrame(STATE,P_DELAY_DUCK_THRESHOLD_DB,floatToQ16(p.duckThresholdDb));
 sendFrame(STATE,P_DELAY_DUCK_RELEASE_MS,floatToQ16(p.duckReleaseMs));
 const RTALMidiClockState c=RTALMidiClock::state();
 sendFrame(STATE,P_DELAY_MODE,(int32_t)c.delayMode); sendFrame(STATE,P_DELAY_LEFT_DIV,(int32_t)c.leftDivision); sendFrame(STATE,P_DELAY_RIGHT_DIV,(int32_t)c.rightDivision);
 sendFrame(STATE,P_CLOCK_SOURCE,gClockSource); sendFrame(STATE,P_CLOCK_BPM_X100,(int32_t)gBpmX100);
 const RTALModCoreParameters m=RTALModCore::parameters();
 const RTALModCoreState ms=RTALModCore::state();
 sendFrame(STATE,P_MOD_ENABLE,m.enabled?1:0); sendFrame(STATE,P_MOD_LFO_SHAPE,(int32_t)m.shape);
 sendFrame(STATE,P_MOD_RATE_HZ,floatToQ16(m.rateHz)); sendFrame(STATE,P_MOD_SYNC,m.sync?1:0);
 sendFrame(STATE,P_MOD_DIVISION,(int32_t)m.division); sendFrame(STATE,P_MOD_DEPTH,floatToQ16(m.depth));
 sendFrame(STATE,P_MOD_STEREO_PHASE_DEG,floatToQ16(m.stereoPhaseDeg)); sendFrame(STATE,P_MOD_SMOOTHING_MS,floatToQ16(m.smoothingMs));
 sendFrame(STATE,P_MOD_LFO_LEFT,floatToQ16(ms.left)); sendFrame(STATE,P_MOD_LFO_RIGHT,floatToQ16(ms.right));
 sendFrame(STATE,P_MOD_EFFECT_SELECT,(int32_t)currentModFxSelection());
 const RTALStereoChorusParameters ch=RTALStereoChorus::parameters();
 sendFrame(STATE,P_CHORUS_ENABLE,ch.enabled?1:0); sendFrame(STATE,P_CHORUS_MIX,floatToQ16(ch.mix));
 sendFrame(STATE,P_CHORUS_DELAY_MS,floatToQ16(ch.baseDelayMs)); sendFrame(STATE,P_CHORUS_TONE_HZ,floatToQ16(ch.toneHz));
 const RTALStereoFlangerParameters fl=RTALStereoFlanger::parameters();
 sendFrame(STATE,P_FLANGER_ENABLE,fl.enabled?1:0); sendFrame(STATE,P_FLANGER_MIX,floatToQ16(fl.mix));
 sendFrame(STATE,P_FLANGER_DELAY_MS,floatToQ16(fl.baseDelayMs)); sendFrame(STATE,P_FLANGER_FEEDBACK,floatToQ16(fl.feedback));
 sendFrame(STATE,P_FLANGER_HPF_HZ,floatToQ16(fl.feedbackHpfHz)); sendFrame(STATE,P_FLANGER_SATURATION,floatToQ16(fl.saturation));
 const RTALStereoPhaserParameters ph=RTALStereoPhaser::parameters();
 sendFrame(STATE,P_PHASER_ENABLE,ph.enabled?1:0); sendFrame(STATE,P_PHASER_MIX,floatToQ16(ph.mix));
 sendFrame(STATE,P_PHASER_STAGES,(int32_t)ph.stages); sendFrame(STATE,P_PHASER_FEEDBACK,floatToQ16(ph.feedback));
 sendFrame(STATE,P_PHASER_HPF_HZ,floatToQ16(ph.feedbackHpfHz)); sendFrame(STATE,P_PHASER_SATURATION,floatToQ16(ph.saturation));
 sendFrame(STATE,P_PHASER_CENTER_HZ,floatToQ16(ph.centerHz));
 const RTALStereoWidthParameters sw=RTALStereoWidth::parameters();
 sendFrame(STATE,P_WIDTH_ENABLE,sw.enabled?1:0); sendFrame(STATE,P_WIDTH_AMOUNT,floatToQ16(sw.width));
 const auto rv=RTALStereoReverb::parameters();
 sendFrame(STATE,P_REVERB_ENABLE,rv.enabled?1:0); sendFrame(STATE,P_REVERB_MIX,floatToQ16(rv.mix));
 sendFrame(STATE,P_REVERB_SIZE,floatToQ16(rv.size)); sendFrame(STATE,P_REVERB_DECAY,floatToQ16(rv.decay));
 sendFrame(STATE,P_REVERB_DAMPING_HZ,floatToQ16(rv.dampingHz)); sendFrame(STATE,P_REVERB_PREDELAY_MS,floatToQ16(rv.predelayMs));
}

void RTALFxLink::handleFrame(const Frame&f){
 if(f.version!=VERSION){++gVer;return;} const uint8_t*r=(const uint8_t*)&f;if(crc8(r+2,sizeof(Frame)-3)!=f.crc){++gCrc;return;}
 ++gRx;gLastRx=millis();gConnected=true;
 if(f.type==HELLO){++gHello;sendFrame(HELLO_ACK,0,0x00420001);return;}
 if(f.type==PING){sendFrame(PONG,0,f.value);return;}
 if(f.type==GET_STATE){sendState();return;}
 if(f.type==CLOCK_STATE){
   if(f.param==P_CLOCK_SOURCE){gClockSource=(uint8_t)(f.value?1:0);RTALMidiClock::setSource(RTALClockSource::Internal);}
   else if(f.param==P_CLOCK_BPM_X100){gBpmX100=(uint32_t)(f.value<3000?3000:(f.value>30000?30000:f.value));RTALMidiClock::setInternalBpm((float)gBpmX100/100.0f);RTALModCore::setTempoBpm((float)gBpmX100/100.0f);}
   return;
 }
 if(f.type!=SET_PARAM)return;
 ++gSet;
 switch(f.param){
  case P_DELAY_ENABLE: RTALStereoDelay::setEnabled(f.value!=0); break;
  case P_DELAY_FREEZE: RTALStereoDelay::setFreeze(f.value!=0); break;
  case P_DELAY_LEFT_MS: RTALStereoDelay::setTimeLeftMs(q16ToFloat(f.value)); break;
  case P_DELAY_RIGHT_MS: RTALStereoDelay::setTimeRightMs(q16ToFloat(f.value)); break;
  case P_DELAY_FEEDBACK: RTALStereoDelay::setFeedback(q16ToFloat(f.value)); break;
  case P_DELAY_LEVEL: RTALStereoDelay::setLevel(q16ToFloat(f.value)); break;
  case P_DELAY_LOWPASS_HZ: RTALStereoDelay::setFeedbackLowpassHz(q16ToFloat(f.value)); break;
  case P_DELAY_HIGHPASS_HZ: RTALStereoDelay::setFeedbackHighpassHz(q16ToFloat(f.value)); break;
  case P_DELAY_SATURATION: RTALStereoDelay::setFeedbackSaturation(q16ToFloat(f.value)); break;
  case P_DELAY_CROSSFEED: RTALStereoDelay::setCrossfeed(q16ToFloat(f.value)); break;
  case P_DELAY_DUCK_AMOUNT: RTALStereoDelay::setDuckAmount(q16ToFloat(f.value)); break;
  case P_DELAY_DUCK_THRESHOLD_DB: RTALStereoDelay::setDuckThresholdDb(q16ToFloat(f.value)); break;
  case P_DELAY_DUCK_RELEASE_MS: RTALStereoDelay::setDuckReleaseMs(q16ToFloat(f.value)); break;
  case P_DELAY_MODE: RTALMidiClock::setDelayMode(f.value?RTALDelayClockMode::Sync:RTALDelayClockMode::Free); break;
  case P_DELAY_LEFT_DIV: if(f.value>=0&&f.value<12)RTALMidiClock::setLeftDivision((RTALClockDivision)f.value); break;
  case P_DELAY_RIGHT_DIV: if(f.value>=0&&f.value<12)RTALMidiClock::setRightDivision((RTALClockDivision)f.value); break;
  case P_MOD_ENABLE: RTALModCore::setEnabled(f.value!=0); break;
  case P_MOD_LFO_SHAPE: RTALModCore::setShape(f.value==1?RTALModLfoShape::Triangle:RTALModLfoShape::Sine); break;
  case P_MOD_RATE_HZ: RTALModCore::setRateHz(q16ToFloat(f.value)); break;
  case P_MOD_SYNC: RTALModCore::setSync(f.value!=0); break;
  case P_MOD_DIVISION: if(f.value>=0&&f.value<12)RTALModCore::setDivision((uint8_t)f.value); break;
  case P_MOD_DEPTH: RTALModCore::setDepth(q16ToFloat(f.value)); break;
  case P_MOD_STEREO_PHASE_DEG: RTALModCore::setStereoPhaseDeg(q16ToFloat(f.value)); break;
  case P_MOD_SMOOTHING_MS: RTALModCore::setSmoothingMs(q16ToFloat(f.value)); break;
  case P_MOD_LFO_LEFT: case P_MOD_LFO_RIGHT: return; // diagnostics are read-only
  case P_MOD_EFFECT_SELECT: {
    const uint8_t before=currentModFxSelection();
    applyModFxSelection((uint8_t)f.value);
    recordModFxSwitch(before);
    sendFrame(STATE,P_CHORUS_ENABLE,currentModFxSelection()==1?1:0);
    sendFrame(STATE,P_FLANGER_ENABLE,currentModFxSelection()==2?1:0);
    sendFrame(STATE,P_PHASER_ENABLE,currentModFxSelection()==3?1:0);
    break;
  }
  case P_CHORUS_ENABLE: { const uint8_t before=currentModFxSelection(); RTALStereoChorus::setEnabled(f.value!=0); if(f.value){RTALStereoFlanger::setEnabled(false);RTALStereoPhaser::setEnabled(false);} recordModFxSwitch(before); sendFrame(STATE,P_MOD_EFFECT_SELECT,(int32_t)currentModFxSelection()); break; }
  case P_CHORUS_MIX: RTALStereoChorus::setMix(q16ToFloat(f.value)); break;
  case P_CHORUS_DELAY_MS: RTALStereoChorus::setBaseDelayMs(q16ToFloat(f.value)); break;
  case P_CHORUS_TONE_HZ: RTALStereoChorus::setToneHz(q16ToFloat(f.value)); break;
  case P_FLANGER_ENABLE: { const uint8_t before=currentModFxSelection(); RTALStereoFlanger::setEnabled(f.value!=0); if(f.value){RTALStereoChorus::setEnabled(false);RTALStereoPhaser::setEnabled(false);} recordModFxSwitch(before); sendFrame(STATE,P_MOD_EFFECT_SELECT,(int32_t)currentModFxSelection()); break; }
  case P_FLANGER_MIX: RTALStereoFlanger::setMix(q16ToFloat(f.value)); break;
  case P_FLANGER_DELAY_MS: RTALStereoFlanger::setBaseDelayMs(q16ToFloat(f.value)); break;
  case P_FLANGER_FEEDBACK: RTALStereoFlanger::setFeedback(q16ToFloat(f.value)); break;
  case P_FLANGER_HPF_HZ: RTALStereoFlanger::setFeedbackHpfHz(q16ToFloat(f.value)); break;
  case P_FLANGER_SATURATION: RTALStereoFlanger::setSaturation(q16ToFloat(f.value)); break;
  case P_PHASER_ENABLE: { const uint8_t before=currentModFxSelection(); RTALStereoPhaser::setEnabled(f.value!=0); if(f.value){RTALStereoChorus::setEnabled(false);RTALStereoFlanger::setEnabled(false);} recordModFxSwitch(before); sendFrame(STATE,P_MOD_EFFECT_SELECT,(int32_t)currentModFxSelection()); break; }
  case P_PHASER_MIX: RTALStereoPhaser::setMix(q16ToFloat(f.value)); break;
  case P_PHASER_STAGES: RTALStereoPhaser::setStages((uint8_t)f.value); break;
  case P_PHASER_FEEDBACK: RTALStereoPhaser::setFeedback(q16ToFloat(f.value)); break;
  case P_PHASER_HPF_HZ: RTALStereoPhaser::setFeedbackHpfHz(q16ToFloat(f.value)); break;
  case P_PHASER_SATURATION: RTALStereoPhaser::setSaturation(q16ToFloat(f.value)); break;
  case P_PHASER_CENTER_HZ: RTALStereoPhaser::setCenterHz(q16ToFloat(f.value)); break;
  case P_WIDTH_ENABLE: RTALStereoWidth::setEnabled(f.value!=0); break;
  case P_WIDTH_AMOUNT: RTALStereoWidth::setWidth(q16ToFloat(f.value)); break;
  case P_REVERB_ENABLE: RTALStereoReverb::setEnabled(f.value!=0); break;
  case P_REVERB_MIX: RTALStereoReverb::setMix(q16ToFloat(f.value)); break;
  case P_REVERB_SIZE: RTALStereoReverb::setSize(q16ToFloat(f.value)); break;
  case P_REVERB_DECAY: RTALStereoReverb::setDecay(q16ToFloat(f.value)); break;
  case P_REVERB_DAMPING_HZ: RTALStereoReverb::setDampingHz(q16ToFloat(f.value)); break;
  case P_REVERB_PREDELAY_MS: RTALStereoReverb::setPredelayMs(q16ToFloat(f.value)); break;
  default:return;
 }
 RTALPresetState::markModified(); sendFrame(STATE,f.param,f.value);
}

void RTALFxLink::parseByte(uint8_t b){if(gRxPos==0&&b!=SOF1)return;if(gRxPos==1&&b!=SOF2){gRxPos=(b==SOF1)?1:0;if(gRxPos)gRxBuf[0]=SOF1;return;}gRxBuf[gRxPos++]=b;if(gRxPos==sizeof(Frame)){Frame f;memcpy(&f,gRxBuf,sizeof(f));gRxPos=0;handleFrame(f);}}

void RTALFxLink::service(){if(!gReady)return;uint8_t n=RTAL_FX_LINK_SERVICE_MAX_BYTES;while(n--&&gLinkSerial.available()){int c=gLinkSerial.read();if(c>=0)parseByte((uint8_t)c);}if(gConnected&&gLastRx&&(millis()-gLastRx)>RTAL_FX_LINK_TIMEOUT_MS)gConnected=false;}
void RTALFxLink::printReport(){RTALLogger::printf(RTALLogLevel::Info,"FXLink connected=%s rx=%lu tx=%lu crc=%lu ver=%lu hello=%lu set=%lu age=%lu ms bpm=%.2f source=%s",gConnected?"YES":"NO",(unsigned long)gRx,(unsigned long)gTx,(unsigned long)gCrc,(unsigned long)gVer,(unsigned long)gHello,(unsigned long)gSet,gLastRx?(unsigned long)(millis()-gLastRx):0UL,(double)gBpmX100/100.0,gClockSource?"MIDI":"INT"); RTALLogger::printf(RTALLogLevel::Info,"FXSwitch count=%lu last=%u->%u age=%lu ms",(unsigned long)gModFxSwitches,(unsigned)gLastModFxFrom,(unsigned)gLastModFxTo,gLastModFxSwitchMs?(unsigned long)(millis()-gLastModFxSwitchMs):0UL); const auto m=RTALModCore::parameters(); const auto ms=RTALModCore::state(); RTALLogger::printf(RTALLogLevel::Info,"ModCore fx=%u shape=%s mode=%s rate=%.3fHz div=%u depth=%.2f stereo=%.0fdeg smooth=%.0fms lfo=%+.3f/%+.3f",(unsigned)currentModFxSelection(),m.shape==RTALModLfoShape::Triangle?"TRI":"SIN",m.sync?"SYNC":"FREE",(double)ms.effectiveRateHz,(unsigned)m.division,(double)m.depth,(double)m.stereoPhaseDeg,(double)m.smoothingMs,(double)ms.left,(double)ms.right); const auto ch=RTALStereoChorus::parameters(); RTALLogger::printf(RTALLogLevel::Info,"Chorus enabled=%s mix=%.2f delay=%.1fms tone=%.0fHz",ch.enabled?"ON":"OFF",(double)ch.mix,(double)ch.baseDelayMs,(double)ch.toneHz); const auto fl=RTALStereoFlanger::parameters(); RTALLogger::printf(RTALLogLevel::Info,"Flanger enabled=%s mix=%.2f delay=%.2fms fb=%+.2f hpf=%.0fHz sat=%.2f",fl.enabled?"ON":"OFF",(double)fl.mix,(double)fl.baseDelayMs,(double)fl.feedback,(double)fl.feedbackHpfHz,(double)fl.saturation); const auto ph=RTALStereoPhaser::parameters(); RTALLogger::printf(RTALLogLevel::Info,"Phaser enabled=%s mix=%.2f stages=%u fb=%+.2f hpf=%.0fHz sat=%.2f center=%.0fHz",ph.enabled?"ON":"OFF",(double)ph.mix,(unsigned)ph.stages,(double)ph.feedback,(double)ph.feedbackHpfHz,(double)ph.saturation,(double)ph.centerHz); const auto sw=RTALStereoWidth::parameters(); RTALLogger::printf(RTALLogLevel::Info,"StereoWidth enabled=%s width=%.2f (%.0f%%) mode=M/S position=FINAL",sw.enabled?"ON":"OFF",(double)sw.width,(double)(sw.width*100.0f)); const auto rv=RTALStereoReverb::parameters(); RTALLogger::printf(RTALLogLevel::Info,"Reverb enabled=%s mix=%.2f size=%.2f decay=%.2f damp=%.0fHz pre=%.0fms",rv.enabled?"ON":"OFF",(double)rv.mix,(double)rv.size,(double)rv.decay,(double)rv.dampingHz,(double)rv.predelayMs); }
