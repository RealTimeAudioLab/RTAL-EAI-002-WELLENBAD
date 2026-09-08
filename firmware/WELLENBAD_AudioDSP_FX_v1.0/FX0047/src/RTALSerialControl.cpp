#include "../include/RTALSerialControl.h"
#include "../include/RTALConfig.h"
#include "../include/RTALLogger.h"
#include "../include/RTALStereoDelay.h"
#include "../include/RTALDelayRamPresets.h"
#include "../include/RTALDelayNvsPresets.h"
#include "../include/RTALDSPKernel.h"
#include "../include/RTALMidiClock.h"
#include "../include/RTALPresetState.h"
#include "../include/RTALStartupPreset.h"
#include "../include/RTALPresetCompare.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char RTALSerialControl::buffer_[RTAL_SERIAL_COMMAND_BUFFER_SIZE] = {};
size_t RTALSerialControl::length_ = 0;
uint32_t RTALSerialControl::lastPollMs_ = 0;

RTALStatus RTALSerialControl::begin(){
 length_=0; buffer_[0]='\0'; lastPollMs_=millis();
 RTALLogger::printf(RTALLogLevel::Info,"SerialControl .. PASS buffer=%u",(unsigned)RTAL_SERIAL_COMMAND_BUFFER_SIZE);
 Serial.println(); Serial.println("RTAL Serial Live Control ready"); Serial.println("Type 'help' for commands."); Serial.print("rtal> ");
 return RTALStatus::OK;
}

void RTALSerialControl::service(){
 if(!RTAL_SERIAL_CONTROL_ENABLED) return;
 uint32_t now=millis(); if(now-lastPollMs_<RTAL_SERIAL_CONTROL_POLL_MS) return; lastPollMs_=now;
 while(Serial.available()>0){
  char c=(char)Serial.read();
  if(c=='\r'||c=='\n'){
   if(length_){ buffer_[length_]='\0'; trim(buffer_); if(buffer_[0]) execute(buffer_); length_=0; buffer_[0]='\0'; Serial.print("rtal> "); }
  } else if(c==8||c==127){ if(length_) --length_; }
  else if(isprint((unsigned char)c)){
   if(length_<RTAL_SERIAL_COMMAND_BUFFER_SIZE-1) buffer_[length_++]=c;
   else { length_=0; Serial.println("ERR command too long"); Serial.print("rtal> "); }
  }
 }
}

bool RTALSerialControl::equalsIgnoreCase(const char*a,const char*b){
 if(!a||!b) return false; while(*a&&*b){ if(tolower((unsigned char)*a)!=tolower((unsigned char)*b)) return false; ++a;++b;} return *a=='\0'&&*b=='\0';
}
void RTALSerialControl::trim(char*t){
 char*s=t; while(*s&&isspace((unsigned char)*s))++s; if(s!=t)memmove(t,s,strlen(s)+1); size_t n=strlen(t); while(n&&isspace((unsigned char)t[n-1]))t[--n]='\0';
}
bool RTALSerialControl::parseOnOff(const char*t,bool&v){
 if(equalsIgnoreCase(t,"on")||equalsIgnoreCase(t,"1")||equalsIgnoreCase(t,"true")){v=true;return true;} if(equalsIgnoreCase(t,"off")||equalsIgnoreCase(t,"0")||equalsIgnoreCase(t,"false")){v=false;return true;} return false;
}
bool RTALSerialControl::parseFloat(const char*t,float&v){
 if(!t||!*t)return false; char*e=nullptr; v=strtof(t,&e); if(e==t)return false; while(*e&&isspace((unsigned char)*e))++e; return *e=='\0';
}

void RTALSerialControl::execute(char*line){
 char*save=nullptr; char*cmd=strtok_r(line," \t",&save); if(!cmd)return;
 if(equalsIgnoreCase(cmd,"help")||equalsIgnoreCase(cmd,"?")){printHelp();return;}
 if(equalsIgnoreCase(cmd,"status")){printStatus();return;}
 if(equalsIgnoreCase(cmd,"tap")){
  bool valid=RTALMidiClock::tap();
  RTALMidiClockState s=RTALMidiClock::state();
  if(valid){Serial.print("Tap tempo=");Serial.print(s.tapBpm,2);Serial.print(" BPM taps=");Serial.println(s.tapCount);}
  else{Serial.print("Tap registered; taps=");Serial.print(s.tapCount);Serial.println(" - tap again");}
  return;
 }



 if(equalsIgnoreCase(cmd,"startup")){
  char*a=strtok_r(nullptr," \t",&save);
  if(!a||equalsIgnoreCase(a,"show")){RTALStartupPreset::print(Serial);return;}
  if(equalsIgnoreCase(a,"off")){if(!RTALStartupPreset::disable()){Serial.println("ERR startup setting write failed");return;}Serial.println("OK startup preset disabled");return;}
  if(equalsIgnoreCase(a,"preset")){char*st=strtok_r(nullptr," \t",&save);if(!st){Serial.println("ERR missing startup slot");return;}char*e=nullptr;long n=strtol(st,&e,10);if(e==st||*e!='\0'||n<1||n>RTAL_DELAY_RAM_PRESET_COUNT){Serial.print("ERR startup slot must be 1..");Serial.println(RTAL_DELAY_RAM_PRESET_COUNT);return;}if(!RTALStartupPreset::setSlot((uint8_t)n)){Serial.println("ERR startup setting write failed");return;}Serial.print("OK startup preset set to ");Serial.println(n);Serial.println("It will be applied after the next restart.");return;}
  Serial.println("ERR use startup show|preset <1..8>|off");return;
 }
 if(equalsIgnoreCase(cmd,"clock")){
  char*a=strtok_r(nullptr," \t",&save);
  if(!a||equalsIgnoreCase(a,"show")){RTALMidiClock::printStatus(Serial);return;}
  if(equalsIgnoreCase(a,"source")){
   char*v=strtok_r(nullptr," \t",&save);if(!v){Serial.println("ERR use clock source midi|internal|tap");return;}
   RTALClockSource source;
   if(equalsIgnoreCase(v,"midi"))source=RTALClockSource::Midi;
   else if(equalsIgnoreCase(v,"internal"))source=RTALClockSource::Internal;
   else if(equalsIgnoreCase(v,"tap"))source=RTALClockSource::Tap;
   else{Serial.println("ERR use clock source midi|internal|tap");return;}
   RTALMidiClock::setSource(source);
   Serial.print("OK clock source=");Serial.println(RTALMidiClock::sourceName(source));
   RTALMidiClock::printStatus(Serial);return;
  }
  if(equalsIgnoreCase(a,"internal")){
   char*v=strtok_r(nullptr," \t",&save);float bpm;
   if(!v||!parseFloat(v,bpm)){Serial.println("ERR use clock internal <30.0..300.0>");return;}
   if(!RTALMidiClock::setInternalBpm(bpm)){Serial.println("ERR internal BPM must be 30.0..300.0");return;}
   Serial.print("OK internal BPM=");Serial.println(bpm,2);
   RTALMidiClock::printStatus(Serial);return;
  }
  if(equalsIgnoreCase(a,"tapreset")){RTALMidiClock::resetTapSequence();Serial.println("OK tap sequence reset");return;}
  Serial.println("ERR use clock show|source midi/internal/tap|internal <BPM>|tapreset");return;
 }
 if(equalsIgnoreCase(cmd,"nvs")){
  char*a=strtok_r(nullptr," \t",&save);
  if(a&&equalsIgnoreCase(a,"status")){RTALPresetState::print(Serial);return;}
  if(!a||equalsIgnoreCase(a,"list")){RTALDelayNvsPresets::list(Serial);return;}
  char*st=strtok_r(nullptr," \t",&save); if(!st){Serial.println("ERR missing NVS slot");return;}
  char*e=nullptr; long n=strtol(st,&e,10);
  if(e==st||*e!='\0'||n<1||n>RTAL_DELAY_NVS_PRESET_COUNT){Serial.print("ERR NVS slot must be 1..");Serial.println(RTAL_DELAY_NVS_PRESET_COUNT);return;}
  uint8_t slot=(uint8_t)n;
  if(equalsIgnoreCase(a,"save")){
   Serial.println("NVS write requested - keep input silent");
   if(!RTALDelayNvsPresets::saveFromRam(slot)){Serial.println("ERR NVS save failed or RAM slot empty");return;}
   Serial.print("OK RAM preset written and verified in NVS: ");Serial.println(slot);return;
  }
  if(equalsIgnoreCase(a,"load")){
   if(!RTALDelayNvsPresets::loadToRam(slot)){Serial.println("ERR NVS preset empty or invalid");return;}
   Serial.print("OK NVS preset validated and copied to RAM: ");Serial.println(slot);
   Serial.println("Use preset load <slot> to apply it.");return;
  }
  if(equalsIgnoreCase(a,"delete")||equalsIgnoreCase(a,"erase")){
   Serial.println("NVS erase requested - keep input silent");
   if(!RTALDelayNvsPresets::erase(slot)){Serial.println("ERR NVS slot empty or erase failed");return;}
   Serial.print("OK NVS preset deleted: ");Serial.println(slot);return;
  }
  Serial.println("ERR use nvs list|save|load|delete");return;
 }
 if(equalsIgnoreCase(cmd,"preset")){
  char*a=strtok_r(nullptr," \t",&save);
  if(a&&equalsIgnoreCase(a,"status")){RTALPresetState::print(Serial);return;}
  if(a&&equalsIgnoreCase(a,"compare")){RTALPresetOperationResult r=RTALPresetCompare::compare(Serial);if(r!=RTALPresetOperationResult::Ok){Serial.print("ERR preset compare ");Serial.println(RTALPresetCompare::resultName(r));}return;}
  if(a&&equalsIgnoreCase(a,"revert")){RTALPresetOperationResult r=RTALPresetCompare::revert();if(r!=RTALPresetOperationResult::Ok){Serial.print("ERR preset revert ");Serial.println(RTALPresetCompare::resultName(r));return;}Serial.println("OK preset revert queued through smooth transition");return;}
  if(a&&equalsIgnoreCase(a,"commit")){RTALPresetStateSnapshot s=RTALPresetState::snapshot();RTALPresetOperationResult r=RTALPresetCompare::commit();if(r!=RTALPresetOperationResult::Ok){Serial.print("ERR preset commit ");Serial.println(RTALPresetCompare::resultName(r));return;}Serial.print("OK current delay committed to RAM preset ");Serial.println(s.activeSlot);Serial.println("Use nvs save <slot> for persistent storage.");return;}
  if(!a||equalsIgnoreCase(a,"list")){RTALDelayRamPresets::list(Serial);return;}
  if(equalsIgnoreCase(a,"clearall")){RTALDelayRamPresets::clearAll();Serial.println("OK all RAM presets cleared");return;}
  char*st=strtok_r(nullptr," \t",&save);if(!st){Serial.println("ERR missing preset slot");return;}
  char*e=nullptr;long n=strtol(st,&e,10);
  if(e==st||*e!='\0'||n<1||n>RTAL_DELAY_RAM_PRESET_COUNT){Serial.print("ERR preset slot must be 1..");Serial.println(RTAL_DELAY_RAM_PRESET_COUNT);return;}
  uint8_t slot=(uint8_t)n;
  if(equalsIgnoreCase(a,"save")){if(!RTALDelayRamPresets::save(slot)){Serial.println("ERR preset save failed");return;}Serial.print("OK RAM preset saved: ");Serial.println(slot);return;}
  if(equalsIgnoreCase(a,"load")){if(!RTALDelayRamPresets::load(slot)){RTALPresetState::loadFailed(slot,RTALPresetSource::Serial);Serial.println("ERR RAM preset empty");return;}RTALPresetState::activated(slot,RTALPresetSource::Serial);Serial.print("OK RAM preset loaded: ");Serial.println(slot);printDelay();return;}
  if(equalsIgnoreCase(a,"delete")||equalsIgnoreCase(a,"erase")){RTALDelayRamPresets::erase(slot);Serial.print("OK RAM preset deleted: ");Serial.println(slot);return;}
  if(equalsIgnoreCase(a,"show")){if(!RTALDelayRamPresets::show(slot,Serial)){Serial.println("ERR RAM preset empty");return;}return;}
  if(equalsIgnoreCase(a,"name")){
   char*name=save;while(name&&(*name==' '||*name=='\t'))++name;
   if(!name||!name[0]){Serial.println("ERR missing preset name");return;}
   trim(name);
   if(!RTALDelayRamPresets::setName(slot,name)){Serial.print("ERR name requires occupied slot and 1..");Serial.print(RTAL_DELAY_PRESET_NAME_LENGTH);Serial.println(" printable characters");return;}
   Serial.print("OK preset name ");Serial.print(slot);Serial.print(" = ");Serial.println(name);return;
  }
  if(equalsIgnoreCase(a,"origin")){
   char*v=strtok_r(nullptr," \t",&save);if(!v){Serial.println("ERR use preset origin <slot> user|factory");return;}
   RTALPresetOrigin origin;
   if(equalsIgnoreCase(v,"user"))origin=RTALPresetOrigin::User;
   else if(equalsIgnoreCase(v,"factory"))origin=RTALPresetOrigin::Factory;
   else{Serial.println("ERR use preset origin <slot> user|factory");return;}
   if(!RTALDelayRamPresets::setOrigin(slot,origin)){Serial.println("ERR RAM preset empty");return;}
   Serial.print("OK preset origin ");Serial.print(slot);Serial.print(" = ");Serial.println(RTALDelayRamPresets::originName(origin));return;
  }
  Serial.println("ERR use preset status|compare|revert|commit|list|show|save|load|delete|name|origin|clearall");return;
 }
 if(!equalsIgnoreCase(cmd,"delay")){Serial.println("ERR unknown command");return;}
 char*a=strtok_r(nullptr," \t",&save);
 if(!a||equalsIgnoreCase(a,"show")){printDelay();RTALMidiClock::printStatus(Serial);return;}
 if(equalsIgnoreCase(a,"mode")){
  char*v=strtok_r(nullptr," \t",&save); if(!v){Serial.println("ERR use delay mode free|sync");return;}
  if(equalsIgnoreCase(v,"free"))RTALMidiClock::setDelayMode(RTALDelayClockMode::Free);
  else if(equalsIgnoreCase(v,"sync"))RTALMidiClock::setDelayMode(RTALDelayClockMode::Sync);
  else {Serial.println("ERR use delay mode free|sync");return;}
  RTALPresetState::markModified();Serial.println("OK");RTALMidiClock::printStatus(Serial);return;
 }
 if(equalsIgnoreCase(a,"syncleft")||equalsIgnoreCase(a,"syncright")){
  char*v=strtok_r(nullptr," \t",&save); if(!v){Serial.println("ERR missing note value");return;}
  bool ok=equalsIgnoreCase(a,"syncleft")?RTALMidiClock::setLeftDivision(v):RTALMidiClock::setRightDivision(v);
  if(!ok){Serial.println("ERR valid: 1/1 1/2 1/4 1/8 1/16 1/32 1/4D 1/8D 1/16D 1/4T 1/8T 1/16T");return;}
  RTALPresetState::markModified();Serial.println("OK");RTALMidiClock::printStatus(Serial);return;
 }
 if(equalsIgnoreCase(a,"clear")){RTALStereoDelay::reset();RTALPresetState::markModified();Serial.println("OK delay buffer cleared");return;}
 if(equalsIgnoreCase(a,"defaults")){
  RTALStereoDelay::setEnabled(RTAL_STEREO_DELAY_ENABLED); RTALStereoDelay::setFreeze(RTAL_DELAY_FREEZE_DEFAULT); RTALStereoDelay::setCrossfeed(RTAL_DELAY_CROSSFEED_DEFAULT); RTALStereoDelay::setDuckAmount(RTAL_DELAY_DUCK_AMOUNT_DEFAULT); RTALStereoDelay::setDuckThresholdDb(RTAL_DELAY_DUCK_THRESHOLD_DB_DEFAULT); RTALStereoDelay::setDuckReleaseMs(RTAL_DELAY_DUCK_RELEASE_MS_DEFAULT);
  RTALStereoDelay::setTimeLeftMs(RTAL_DELAY_TIME_LEFT_MS_DEFAULT); RTALStereoDelay::setTimeRightMs(RTAL_DELAY_TIME_RIGHT_MS_DEFAULT);
  RTALStereoDelay::setFeedback(RTAL_DELAY_FEEDBACK_DEFAULT_BUILD0015); RTALStereoDelay::setFeedbackHighpassHz(RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_DEFAULT); RTALStereoDelay::setFeedbackSaturation(RTAL_DELAY_FEEDBACK_SATURATION_DEFAULT); RTALStereoDelay::setFeedbackLowpassHz(RTAL_DELAY_FEEDBACK_LOWPASS_HZ_DEFAULT); RTALStereoDelay::setLevel(RTAL_DELAY_LEVEL_DEFAULT);
  RTALPresetState::markModified();Serial.println("OK delay defaults restored"); printDelay(); return;
 }
 if(equalsIgnoreCase(a,"on")){RTALStereoDelay::setEnabled(true);RTALPresetState::markModified();Serial.println("OK");return;}
 if(equalsIgnoreCase(a,"off")){RTALStereoDelay::setEnabled(false);RTALPresetState::markModified();Serial.println("OK");return;}
 char*vtxt=strtok_r(nullptr," \t",&save); if(!vtxt){Serial.println("ERR missing value");return;}
 if(equalsIgnoreCase(a,"pingpong")||equalsIgnoreCase(a,"bypass")||equalsIgnoreCase(a,"freeze")){
  bool v; if(!parseOnOff(vtxt,v)){Serial.println("ERR expected on/off");return;} if(equalsIgnoreCase(a,"pingpong"))RTALStereoDelay::setPingPong(v);else if(equalsIgnoreCase(a,"freeze"))RTALStereoDelay::setFreeze(v);else RTALStereoDelay::setEnabled(!v); RTALPresetState::markModified();Serial.println("OK"); printDelay(); return;
 }
 float v; if(!parseFloat(vtxt,v)){Serial.println("ERR invalid numeric value");return;}
 if(equalsIgnoreCase(a,"left"))RTALStereoDelay::setTimeLeftMs(v);
 else if(equalsIgnoreCase(a,"right"))RTALStereoDelay::setTimeRightMs(v);
 else if(equalsIgnoreCase(a,"feedback"))RTALStereoDelay::setFeedback(v);
 else if(equalsIgnoreCase(a,"crossfeed")||equalsIgnoreCase(a,"xf"))RTALStereoDelay::setCrossfeed(v);
 else if(equalsIgnoreCase(a,"duckamount")||equalsIgnoreCase(a,"duck"))RTALStereoDelay::setDuckAmount(v);
 else if(equalsIgnoreCase(a,"duckthreshold")||equalsIgnoreCase(a,"duckthr"))RTALStereoDelay::setDuckThresholdDb(v);
 else if(equalsIgnoreCase(a,"duckrelease")||equalsIgnoreCase(a,"duckrel"))RTALStereoDelay::setDuckReleaseMs(v);
 else if(equalsIgnoreCase(a,"saturation")||equalsIgnoreCase(a,"sat"))RTALStereoDelay::setFeedbackSaturation(v);
 else if(equalsIgnoreCase(a,"highpass")||equalsIgnoreCase(a,"hpf"))RTALStereoDelay::setFeedbackHighpassHz(v);
 else if(equalsIgnoreCase(a,"lowpass")||equalsIgnoreCase(a,"lpf"))RTALStereoDelay::setFeedbackLowpassHz(v);
 else if(equalsIgnoreCase(a,"level")||equalsIgnoreCase(a,"wet"))RTALStereoDelay::setLevel(v);
 else {Serial.println("ERR unknown delay parameter");return;}
 RTALPresetState::markModified();Serial.println("OK"); printDelay();
}

void RTALSerialControl::printHelp(){
 Serial.println("help | status | delay show | delay on/off | delay bypass on/off | delay freeze on/off");
 Serial.println("delay left <1..1000> | right <1..1000> | feedback <0..0.92>");
 Serial.println("delay saturation <0..1> | highpass <20..4000> | lowpass <250..18000>");
 Serial.println("delay crossfeed <0..1> | level <0..1> | pingpong on/off (compat)");
 Serial.println("delay duckamount <0..1> | duckthreshold <-60..0> | duckrelease <50..2000>");
 Serial.println("delay clear | delay defaults");
 Serial.println("preset status | preset compare | preset revert | preset commit");
 Serial.println("preset list | preset show/save/load/delete <1..8>");
 Serial.println("preset name <slot> <text> | preset origin <slot> user/factory");
 Serial.println("preset clearall");
 Serial.println("nvs list | nvs save/load/delete <1..8>");
 Serial.println("startup show | startup preset <1..8> | startup off");
 Serial.println("clock show | delay mode free/sync");
 Serial.println("delay syncleft <note> | delay syncright <note>");
}
void RTALSerialControl::printStatus(){
 auto k=RTALDSPKernel::parameters(); Serial.println("RTAL system status"); Serial.print("FX bypass: ");Serial.println(k.fxBypass?"ON":"OFF"); Serial.print("Hard bypass: ");Serial.println(k.hardBypass?"ON":"OFF"); printDelay();
}
void RTALSerialControl::printDelay(){
 auto p=RTALStereoDelay::parameters(); Serial.println("Delay configuration");
 Serial.print("Enabled: ");Serial.println(p.enabled?"ON":"OFF"); Serial.print("Freeze: ");Serial.println(p.freeze?"ON":"OFF"); Serial.print("Crossfeed: ");Serial.println(p.crossfeed,3);
 Serial.print("Left: ");Serial.print(p.timeLeftMs,1);Serial.println(" ms"); Serial.print("Right: ");Serial.print(p.timeRightMs,1);Serial.println(" ms");
 Serial.print("Feedback: ");Serial.println(p.feedback,3); Serial.print("Saturation: ");Serial.println(p.feedbackSaturation,3); Serial.print("High-pass: ");Serial.print(p.feedbackHighpassHz,0);Serial.println(" Hz"); Serial.print("Low-pass: ");Serial.print(p.feedbackLowpassHz,0);Serial.println(" Hz"); Serial.print("Level: ");Serial.println(p.level,3);
}
