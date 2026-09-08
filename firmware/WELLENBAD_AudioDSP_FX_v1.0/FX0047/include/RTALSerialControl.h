#pragma once
#include <Arduino.h>
#include "RTALConfig.h"
#include "RTALStatus.h"
class RTALSerialControl {
public:
 static RTALStatus begin();
 static void service();
private:
 static void execute(char* line);
 static void printHelp();
 static void printStatus();
 static void printDelay();
 static bool equalsIgnoreCase(const char* a,const char* b);
 static bool parseOnOff(const char* text,bool& value);
 static bool parseFloat(const char* text,float& value);
 static void trim(char* text);
 static char buffer_[RTAL_SERIAL_COMMAND_BUFFER_SIZE];
 static size_t length_;
 static uint32_t lastPollMs_;
};
