#include "RTAL_Remote.h"

#if RTAL_REMOTE_USB_ENABLED

// Diagnostics are defined in the main sketch at global scope.
// Keep these declarations outside the anonymous namespace so the linker
// resolves the real global symbols instead of namespace-local ones.
extern volatile uint8_t activeVoicesLast;
extern volatile uint32_t audioDspLastMicros;
extern volatile uint32_t audioDspMaxMicros;
extern volatile uint32_t audioDspOverruns;

namespace {
portMUX_TYPE remoteMux = portMUX_INITIALIZER_UNLOCKED;

uint8_t mirrorFrame[RTALRemote::OLED_FRAME_BYTES] = {0};
RTALRemote::Telemetry mirrorTelemetry = {};
RTALRemote::ParamState mirrorParamState = {};
RTALRemote::FxState mirrorFxState = {};
RTALRemote::SeqState mirrorSeqState = {};
RTALRemote::PresetState mirrorPresetState = {};
RTALRemote::MultiState mirrorMultiState = {};
RTALRemote::MultiNameState mirrorMultiNameState = {};

volatile uint32_t frameGeneration = 0;
volatile uint32_t telemetryGeneration = 0;
volatile uint32_t paramStateGeneration = 0;
volatile uint32_t fxStateGeneration = 0;
volatile uint32_t seqStateGeneration = 0;
volatile uint32_t presetStateGeneration = 0;
volatile uint32_t multiStateGeneration = 0;
volatile uint32_t multiNameStateGeneration = 0;

TaskHandle_t remoteTaskHandle = nullptr;
bool remoteActive = false;
volatile bool gRealtimeGuard = false;

static constexpr uint8_t COMMAND_QUEUE_SIZE = 24;
RTALRemote::Command commandQueue[COMMAND_QUEUE_SIZE] = {};
volatile uint8_t commandHead = 0;
volatile uint8_t commandTail = 0;

volatile uint32_t gFramesSent = 0;
volatile uint32_t gFramesDropped = 0;
volatile uint32_t gTelemetrySent = 0;
volatile uint32_t gTelemetryDropped = 0;
volatile uint32_t gParamStatesSent = 0;
volatile uint32_t gFxStatesSent = 0;
volatile uint32_t gSeqStatesSent = 0;
volatile uint32_t gPresetStatesSent = 0;
volatile uint32_t gMultiStatesSent = 0;
volatile uint32_t gMultiNameStatesSent = 0;
volatile uint32_t gCommandDrops = 0;
volatile uint32_t gWriteMaxUs = 0;
volatile uint32_t gTxStallRecoveries = 0;
volatile uint32_t gResyncRequests = 0;

char rxLine[40] = {};
uint8_t rxLineLen = 0;
uint32_t rxLastByteMs = 0;

bool frameInitialized = false;
bool paramStateInitialized = false;
bool fxStateInitialized = false;
bool seqStateInitialized = false;
bool presetStateInitialized = false;
bool multiStateInitialized = false;
bool multiNameStateInitialized = false;
bool telemetryInitialized = false;

static constexpr uint8_t PACKET_VERSION = 1;
static constexpr uint8_t TYPE_OLED = 1;
static constexpr uint8_t TYPE_TELEMETRY = 2;
static constexpr uint8_t TYPE_PARAM_STATE = 3;
static constexpr uint8_t TYPE_LINK_HEALTH = 4;
static constexpr uint8_t TYPE_FX_STATE = 5;
static constexpr uint8_t TYPE_SEQ_STATE = 6;
static constexpr uint8_t TYPE_PRESET_STATE = 7;
static constexpr uint8_t TYPE_MULTI_STATE = 8;
static constexpr uint8_t TYPE_MULTI_NAME_STATE = 9;
static constexpr uint8_t TYPE_SMART_TELEMETRY = 10;

void updateMax(volatile uint32_t &dst, uint32_t v) {
  uint32_t cur = dst;
  if (v > cur) dst = v;
}

bool cdcConnected() {
  return (bool)RTALUSBSerial;
}

bool enqueueCommand(RTALRemote::CommandType type, int16_t value) {
  bool ok = false;

  portENTER_CRITICAL(&remoteMux);
  uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);

  if (next != commandTail) {
    commandQueue[commandHead].type = type;
    commandQueue[commandHead].value = value;
    commandQueue[commandHead].paramId = 0;
    commandQueue[commandHead].paramValue = 0;
    commandQueue[commandHead].text[0] = 0;
    commandHead = next;
    ok = true;
  } else {
    ++gCommandDrops;
  }

  portEXIT_CRITICAL(&remoteMux);
  return ok;
}

bool enqueueParamCommand(uint8_t paramId, uint8_t paramValue) {
  bool ok = false;

  portENTER_CRITICAL(&remoteMux);
  uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);

  if (next != commandTail) {
    commandQueue[commandHead].type = RTALRemote::CMD_PARAM_SET;
    commandQueue[commandHead].value = 0;
    commandQueue[commandHead].paramId = paramId;
    commandQueue[commandHead].paramValue = paramValue;
    commandQueue[commandHead].text[0] = 0;
    commandHead = next;
    ok = true;
  } else {
    ++gCommandDrops;
  }

  portEXIT_CRITICAL(&remoteMux);
  return ok;
}

bool enqueueFxCommand(uint8_t paramId, int32_t rawValue) {
  bool ok = false;
  portENTER_CRITICAL(&remoteMux);
  uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
  if (next != commandTail) {
    commandQueue[commandHead].type = RTALRemote::CMD_FX_SET;
    commandQueue[commandHead].value = rawValue;
    commandQueue[commandHead].paramId = paramId;
    commandQueue[commandHead].paramValue = 0;
    commandQueue[commandHead].text[0] = 0;
    commandHead = next;
    ok = true;
  } else {
    ++gCommandDrops;
  }
  portEXIT_CRITICAL(&remoteMux);
  return ok;
}


void resetRxParser() {
  rxLineLen = 0;
  rxLine[0] = 0;
  rxLastByteMs = 0;
}

void parsePcCommands() {
  // Explicit protocol:
  // !RTAL,B,1
  // !RTAL,L,1
  // !RTAL,E,-1
  // !RTAL,P,1
  // !RTAL,Q,1
  // !RTAL,S,<paramId>,<value>
  // !RTAL,F,<fxParamId>,<rawInt32>
  // !RTAL,G,1    request fresh FX state from FX processor
  // !RTAL,X,<step>,<value>   set sequencer step 0..15 to 0..127
  // !RTAL,Y,<mode>           set sequencer Table Mode: 0=ABS, 1=REL
  // !RTAL,Z,<item>,<value>   set system item: 0=MIDI channel, 1=CC mode, 2=CC bank
  // !RTAL,R,<slot>           load preset slot 0..127
  // !RTAL,W,<slot>,<name>    save current sound to user slot 30..127
  // !RTAL,N,1                refresh preset-name cache
  // !RTAL,M,<part>,<item>,<value>  Multi part edit (part 0..3)
  // !RTAL,J,<part>,<slot>          load Program slot into Multi part
  // !RTAL,K,<part>                 select Multi part in device UI
  // !RTAL,O,<slot>                 load Multi setup M000..M127
  // !RTAL,V,<slot>,<name>          save current Multi to M000..M127
  // !RTAL,I,<slot>                 query stored Multi name without loading it
    // !RTAL,D,<mode>                 performance mode: 0=SINGLE, 1=MULTI
  //
  // A partial command is discarded after 250 ms so a lost newline cannot
  // poison all following commands.
  const uint32_t now = millis();
  if (rxLineLen && (uint32_t)(now - rxLastByteMs) > 250U) {
    resetRxParser();
  }

  uint8_t rxBudget = 32;

  while (rxBudget-- && RTALUSBSerial.available() > 0) {
    const char c = (char)RTALUSBSerial.read();
    rxLastByteMs = millis();

    if (c == '\r') continue;

    if (c == '\n') {
      rxLine[rxLineLen] = 0;

      if (rxLineLen >= 8 && strncmp(rxLine, "!RTAL,", 6) == 0) {
        const char kind = rxLine[6];

        if (kind == 'H') {
          ++gResyncRequests;
        } else if (kind == 'G') {
          enqueueCommand(RTALRemote::CMD_FX_REQUEST_STATE, 1);
        } else if (kind == 'X') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int step = atoi(p);
            const int stepValue = atoi(comma + 1);
            if (step >= 0 && step < RTALRemote::REMOTE_SEQ_STEPS &&
                stepValue >= 0 && stepValue <= 127) {
              bool ok = false;
              portENTER_CRITICAL(&remoteMux);
              uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
              if (next != commandTail) {
                commandQueue[commandHead].type = RTALRemote::CMD_SEQ_SET;
                commandQueue[commandHead].value = stepValue;
                commandQueue[commandHead].paramId = (uint8_t)step;
                commandQueue[commandHead].paramValue = (uint8_t)stepValue;
                commandHead = next;
                ok = true;
              } else ++gCommandDrops;
              portEXIT_CRITICAL(&remoteMux);
              (void)ok;
            }
          }
        } else if (kind == 'Y') {
          const int mode = atoi(rxLine + 8);
          if (mode == 0 || mode == 1) {
            bool ok = false;
            portENTER_CRITICAL(&remoteMux);
            uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
            if (next != commandTail) {
              commandQueue[commandHead].type = RTALRemote::CMD_SEQ_TABLE_MODE;
              commandQueue[commandHead].value = mode;
              commandQueue[commandHead].paramId = 0;
              commandQueue[commandHead].paramValue = (uint8_t)mode;
              commandHead = next;
              ok = true;
            } else ++gCommandDrops;
            portEXIT_CRITICAL(&remoteMux);
            (void)ok;
          }
        } else if (kind == 'R') {
          const int slot = atoi(rxLine + 8);
          if (slot >= 0 && slot < 128) {
            bool ok = false;
            portENTER_CRITICAL(&remoteMux);
            uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
            if (next != commandTail) {
              commandQueue[commandHead].type = RTALRemote::CMD_PRESET_LOAD;
              commandQueue[commandHead].value = slot;
              commandQueue[commandHead].paramId = (uint8_t)slot;
              commandQueue[commandHead].paramValue = 0;
              commandQueue[commandHead].text[0] = 0;
              commandHead = next; ok = true;
            } else ++gCommandDrops;
            portEXIT_CRITICAL(&remoteMux); (void)ok;
          }
        } else if (kind == 'W') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int slot = atoi(p);
            const char *name = comma + 1;
            if (slot >= 30 && slot < 128 && name[0]) {
              bool ok = false;
              portENTER_CRITICAL(&remoteMux);
              uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
              if (next != commandTail) {
                commandQueue[commandHead].type = RTALRemote::CMD_PRESET_SAVE;
                commandQueue[commandHead].value = slot;
                commandQueue[commandHead].paramId = (uint8_t)slot;
                commandQueue[commandHead].paramValue = 0;
                strncpy(commandQueue[commandHead].text, name, sizeof(commandQueue[commandHead].text)-1);
                commandQueue[commandHead].text[sizeof(commandQueue[commandHead].text)-1] = 0;
                commandHead = next; ok = true;
              } else ++gCommandDrops;
              portEXIT_CRITICAL(&remoteMux); (void)ok;
            }
          }
        } else if (kind == 'N') {
          enqueueCommand(RTALRemote::CMD_PRESET_REFRESH, 1);
        } else if (kind == 'Z') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int item = atoi(p);
            const int itemValue = atoi(comma + 1);
            bool valid = (item == 0 && itemValue >= 1 && itemValue <= 16) ||
                         (item == 1 && itemValue >= 0 && itemValue <= 1) ||
                         (item == 2 && itemValue >= 0 && itemValue <= 2);
            if (valid) {
              bool ok = false;
              portENTER_CRITICAL(&remoteMux);
              uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
              if (next != commandTail) {
                commandQueue[commandHead].type = RTALRemote::CMD_SYSTEM_SET;
                commandQueue[commandHead].value = itemValue;
                commandQueue[commandHead].paramId = (uint8_t)item;
                commandQueue[commandHead].paramValue = (uint8_t)itemValue;
                commandHead = next;
                ok = true;
              } else ++gCommandDrops;
              portEXIT_CRITICAL(&remoteMux);
              (void)ok;
            }
          }
        } else if (kind == 'M') {
          char *p = rxLine + 8;
          char *c1 = strchr(p, ',');
          if (c1) {
            *c1 = 0;
            char *c2 = strchr(c1 + 1, ',');
            if (c2) {
              *c2 = 0;
              const int part = atoi(p);
              const int item = atoi(c1 + 1);
              const int itemValue = atoi(c2 + 1);
              bool valid = part >= 0 && part < 4 && item >= 0 && item <= 10;
              if (valid) {
                bool ok = false;
                portENTER_CRITICAL(&remoteMux);
                uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
                if (next != commandTail) {
                  commandQueue[commandHead].type = RTALRemote::CMD_MULTI_SET;
                  commandQueue[commandHead].value = itemValue;
                  commandQueue[commandHead].paramId = (uint8_t)part;
                  commandQueue[commandHead].paramValue = (uint8_t)item;
                  commandQueue[commandHead].text[0] = 0;
                  commandHead = next; ok = true;
                } else ++gCommandDrops;
                portEXIT_CRITICAL(&remoteMux); (void)ok;
              }
            }
          }
        } else if (kind == 'J') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int part = atoi(p);
            const int slot = atoi(comma + 1);
            if (part >= 0 && part < 4 && slot >= 0 && slot < 128) {
              bool ok = false;
              portENTER_CRITICAL(&remoteMux);
              uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
              if (next != commandTail) {
                commandQueue[commandHead].type = RTALRemote::CMD_MULTI_PRESET;
                commandQueue[commandHead].value = slot;
                commandQueue[commandHead].paramId = (uint8_t)part;
                commandQueue[commandHead].paramValue = (uint8_t)slot;
                commandQueue[commandHead].text[0] = 0;
                commandHead = next; ok = true;
              } else ++gCommandDrops;
              portEXIT_CRITICAL(&remoteMux); (void)ok;
            }
          }
        } else if (kind == 'K') {
          const int part = atoi(rxLine + 8);
          if (part >= 0 && part < 4) {
            bool ok = false;
            portENTER_CRITICAL(&remoteMux);
            uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
            if (next != commandTail) {
              commandQueue[commandHead].type = RTALRemote::CMD_MULTI_SELECT;
              commandQueue[commandHead].value = part;
              commandQueue[commandHead].paramId = (uint8_t)part;
              commandQueue[commandHead].paramValue = 0;
              commandQueue[commandHead].text[0] = 0;
              commandHead = next; ok = true;
            } else ++gCommandDrops;
            portEXIT_CRITICAL(&remoteMux); (void)ok;
          }
        } else if (kind == 'O') {
          const int slot = atoi(rxLine + 8);
          if (slot >= 0 && slot < 128) {
            bool ok = false;
            portENTER_CRITICAL(&remoteMux);
            uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
            if (next != commandTail) {
              commandQueue[commandHead].type = RTALRemote::CMD_MULTI_LOAD;
              commandQueue[commandHead].value = slot;
              commandQueue[commandHead].paramId = (uint8_t)slot;
              commandQueue[commandHead].paramValue = 0;
              commandQueue[commandHead].text[0] = 0;
              commandHead = next; ok = true;
            } else ++gCommandDrops;
            portEXIT_CRITICAL(&remoteMux); (void)ok;
          }
        } else if (kind == 'I') {
          const int slot = atoi(rxLine + 8);
          if (slot >= 0 && slot < 128) {
            bool ok = false;
            portENTER_CRITICAL(&remoteMux);
            uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
            if (next != commandTail) {
              commandQueue[commandHead].type = RTALRemote::CMD_MULTI_NAME_QUERY;
              commandQueue[commandHead].value = slot;
              commandQueue[commandHead].paramId = (uint8_t)slot;
              commandQueue[commandHead].paramValue = 0;
              commandQueue[commandHead].text[0] = 0;
              commandHead = next; ok = true;
            } else ++gCommandDrops;
            portEXIT_CRITICAL(&remoteMux); (void)ok;
          }
        } else if (kind == 'V') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int slot = atoi(p);
            const char *name = comma + 1;
            if (slot >= 0 && slot < 128 && name[0]) {
              bool ok = false;
              portENTER_CRITICAL(&remoteMux);
              uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
              if (next != commandTail) {
                commandQueue[commandHead].type = RTALRemote::CMD_MULTI_SAVE;
                commandQueue[commandHead].value = slot;
                commandQueue[commandHead].paramId = (uint8_t)slot;
                commandQueue[commandHead].paramValue = 0;
                strncpy(commandQueue[commandHead].text, name, sizeof(commandQueue[commandHead].text)-1);
                commandQueue[commandHead].text[sizeof(commandQueue[commandHead].text)-1] = 0;
                commandHead = next; ok = true;
              } else ++gCommandDrops;
              portEXIT_CRITICAL(&remoteMux); (void)ok;
            }
          }
        } else if (kind == 'D') {
          const int mode = atoi(rxLine + 8);
          if (mode == 0 || mode == 1) {
            bool ok = false;
            portENTER_CRITICAL(&remoteMux);
            uint8_t next = (uint8_t)((commandHead + 1U) % COMMAND_QUEUE_SIZE);
            if (next != commandTail) {
              commandQueue[commandHead].type = RTALRemote::CMD_PERFORMANCE_MODE;
              commandQueue[commandHead].value = mode;
              commandQueue[commandHead].paramId = (uint8_t)mode;
              commandQueue[commandHead].paramValue = 0;
              commandQueue[commandHead].text[0] = 0;
              commandHead = next; ok = true;
            } else ++gCommandDrops;
            portEXIT_CRITICAL(&remoteMux); (void)ok;
          }
        } else if (kind == 'F') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int paramId = atoi(p);
            const int32_t rawValue = (int32_t)strtol(comma + 1, nullptr, 10);
            if (paramId >= 0 && paramId < 128) enqueueFxCommand((uint8_t)paramId, rawValue);
          }
        } else if (kind == 'S') {
          char *p = rxLine + 8;
          char *comma = strchr(p, ',');
          if (comma) {
            *comma = 0;
            const int paramId = atoi(p);
            const int paramValue = atoi(comma + 1);

            if (paramId >= 0 && paramId < RTALRemote::REMOTE_PARAM_COUNT &&
                paramValue >= 0 && paramValue <= 127) {
              enqueueParamCommand((uint8_t)paramId, (uint8_t)paramValue);
            }
          }
        } else {
          const int value = atoi(rxLine + 8);
          RTALRemote::CommandType type = RTALRemote::CMD_NONE;

          if (kind == 'B') type = RTALRemote::CMD_BUTTON_SHORT;
          else if (kind == 'L') type = RTALRemote::CMD_BUTTON_LONG;
          else if (kind == 'E') type = RTALRemote::CMD_ENCODER_DELTA;
          else if (kind == 'P') type = RTALRemote::CMD_ENCODER_SHORT;
          else if (kind == 'Q') type = RTALRemote::CMD_ENCODER_LONG;

          if (type != RTALRemote::CMD_NONE) {
            if (type == RTALRemote::CMD_BUTTON_SHORT ||
                type == RTALRemote::CMD_BUTTON_LONG) {
              if (value >= 1 && value <= 8) enqueueCommand(type, (int16_t)value);
            } else {
              enqueueCommand(type, (int16_t)value);
            }
          }
        }
      }

      resetRxParser();
    } else if (rxLineLen < sizeof(rxLine) - 1U) {
      rxLine[rxLineLen++] = c;
    } else {
      resetRxParser();
    }
  }
}

// SAFE_A003F: chunked, non-blocking TX.
// Arduino-ESP32/TinyUSB CDC may expose a TX window much smaller than the
// complete 1036-byte OLED packet. Requiring availableForWrite() >= the whole
// packet therefore caused every OLED frame to be discarded.
//
// We keep one pending packet and advance it only by the bytes that CDC says
// can be accepted immediately. There is never a wait for TX space.
static uint8_t txPacket[12 + RTALRemote::OLED_FRAME_BYTES];
static size_t txLength = 0;
static size_t txOffset = 0;
static uint8_t txType = 0;
static uint32_t txLastProgressMs = 0;

bool txBusy() {
  return txOffset < txLength;
}

bool queuePacket(uint8_t type, uint32_t seq, const uint8_t *payload, uint16_t len) {
  if (txBusy()) return false;
  if ((size_t)len + 12U > sizeof(txPacket)) return false;

  size_t p = 0;
  txPacket[p++] = 'R';
  txPacket[p++] = 'T';
  txPacket[p++] = 'A';
  txPacket[p++] = 'L';
  txPacket[p++] = PACKET_VERSION;
  txPacket[p++] = type;
  txPacket[p++] = (uint8_t)(len & 0xFF);
  txPacket[p++] = (uint8_t)(len >> 8);
  txPacket[p++] = (uint8_t)(seq & 0xFF);
  txPacket[p++] = (uint8_t)((seq >> 8) & 0xFF);
  txPacket[p++] = (uint8_t)((seq >> 16) & 0xFF);
  txPacket[p++] = (uint8_t)((seq >> 24) & 0xFF);
  memcpy(txPacket + p, payload, len);
  p += len;

  txLength = p;
  txOffset = 0;
  txType = type;
  txLastProgressMs = millis();
  return true;
}

void resetTxPacket(bool countDrop) {
  if (countDrop && txBusy()) {
    if (txType == TYPE_OLED) ++gFramesDropped;
    else if (txType == TYPE_TELEMETRY) ++gTelemetryDropped;
  }
  txLength = 0;
  txOffset = 0;
  txType = 0;
  txLastProgressMs = 0;
}

void serviceTx() {
  if (!txBusy()) return;

  // WB0070 CDC SMART SAFE1: while audio is active, only the compact Smart
  // Telemetry packet is allowed. All larger packets are aborted immediately.
  if (gRealtimeGuard && txType != TYPE_SMART_TELEMETRY) {
    resetTxPacket(true);
    return;
  }

  // If TinyUSB/host stops accepting bytes, never leave Remote permanently
  // stuck in txBusy(). Drop this non-critical UI packet after 250 ms.
  if ((uint32_t)(millis() - txLastProgressMs) > 250U) {
    ++gTxStallRecoveries;
    resetTxPacket(true);
    return;
  }

  const int writable = RTALUSBSerial.availableForWrite();
  if (writable <= 0) return;

  size_t remaining = txLength - txOffset;
  size_t chunk = remaining;

  if (chunk > 64U) chunk = 64U;
  if (chunk > (size_t)writable) chunk = (size_t)writable;
  if (chunk == 0) return;

  const uint32_t t0 = micros();
  const size_t written = RTALUSBSerial.write(txPacket + txOffset, chunk);
  const uint32_t dt = micros() - t0;
  updateMax(gWriteMaxUs, dt);

  if (written == 0) return;

  txOffset += written;
  txLastProgressMs = millis();

  if (txOffset >= txLength) {
    if (txType == TYPE_OLED) ++gFramesSent;
    else if (txType == TYPE_TELEMETRY) ++gTelemetrySent;
    else if (txType == TYPE_PARAM_STATE) ++gParamStatesSent;
    else if (txType == TYPE_FX_STATE) ++gFxStatesSent;
    else if (txType == TYPE_SEQ_STATE) ++gSeqStatesSent;
    else if (txType == TYPE_PRESET_STATE) ++gPresetStatesSent;
    else if (txType == TYPE_MULTI_STATE) ++gMultiStatesSent;
    resetTxPacket(false);
  }
}

void remoteTask(void *) {
  uint32_t sentFrameGeneration = 0;
  uint32_t sentTelemetryGeneration = 0;
  uint32_t sentParamStateGeneration = 0;
  uint32_t sentFxStateGeneration = 0;
  uint32_t sentSeqStateGeneration = 0;
  uint32_t sentPresetStateGeneration = 0;
  uint32_t sentMultiStateGeneration = 0;
  uint32_t sentMultiNameStateGeneration = 0;

  uint32_t frameSeq = 0;
  uint32_t telemetrySeq = 0;
  uint32_t paramSeq = 0;
  uint32_t fxSeq = 0;
  uint32_t seqStateSeq = 0;
  uint32_t presetStateSeq = 0;
  uint32_t multiStateSeq = 0;
  uint32_t multiNameStateSeq = 0;
  uint32_t healthSeq = 0;
  uint32_t smartSeq = 0;
  uint32_t lastSmartMs = 0;
  uint32_t lastHealthMs = 0;
  uint32_t seenResyncRequests = 0;
  uint16_t sessionId = 0;

  uint8_t frame[RTALRemote::OLED_FRAME_BYTES];
  RTALRemote::Telemetry telemetry;
  RTALRemote::ParamState paramState;
  RTALRemote::FxState fxState;
  RTALRemote::SeqState seqState;
  RTALRemote::PresetState presetState;
  RTALRemote::MultiState multiState;
  RTALRemote::MultiNameState multiNameState;

  bool wasConnected = false;
  bool wasRealtimeGuarded = false;

  for (;;) {
    const bool connectedNow = cdcConnected();

    if (!connectedNow) {
      if (wasConnected) {
        resetTxPacket(false);
        resetRxParser();
      }
      wasConnected = false;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    if (!wasConnected) {
      // Fresh host session: discard any stale partial transport state.
      resetTxPacket(false);
      resetRxParser();
      sentFrameGeneration = 0;
      sentTelemetryGeneration = 0;
      sentParamStateGeneration = 0;
      sentFxStateGeneration = 0;
      sentSeqStateGeneration = 0;
      sentPresetStateGeneration = 0;
      sentMultiStateGeneration = 0;
      sentMultiNameStateGeneration = 0;
      ++sessionId;
      lastHealthMs = 0;
      wasConnected = true;
    }

    parsePcCommands();

    // WB0070 CDC SMART SAFE1: during sounding audio the normal mirror/state
    // channels remain silent. A single compact packet (14-byte payload, 26 bytes
    // including RTAL header) may be sent at most once per second, and only while
    // the latest DSP block still has >= ~400 us of headroom. This keeps Voices,
    // DSP last/max and Overruns visible without reintroducing OLED/full-state
    // bursts or multi-chunk USB writes.
    const bool guardNow = gRealtimeGuard;
    if (guardNow) {
      seenResyncRequests = gResyncRequests;
      if (!wasRealtimeGuarded) {
        resetTxPacket(true);
        lastSmartMs = 0;
        wasRealtimeGuarded = true;
      }

      serviceTx();
      const uint32_t smartNow = millis();
      if (!txBusy() && (uint32_t)(smartNow - lastSmartMs) >= 1000U) {
        const uint32_t dspLast = audioDspLastMicros;
        // 2902 us nominal budget at 44.1 kHz / 128. Keep about 400 us reserve.
        if (dspLast <= 2500U) {
          RTALRemote::SmartTelemetry st = {};
          st.audioDspLastUs = dspLast;
          st.audioDspMaxUs = audioDspMaxMicros;
          st.audioDspOverruns = audioDspOverruns;
          st.activeVoices = activeVoicesLast;
          st.flags = 0x01;
          if (queuePacket(TYPE_SMART_TELEMETRY, ++smartSeq,
                          (const uint8_t *)&st, sizeof(st))) {
            lastSmartMs = smartNow;
          }
        }
      }

      // The smart packet always fits into one <=64-byte USB write. Service it
      // immediately, then give Core 0 a long quiet interval.
      serviceTx();
      vTaskDelay(pdMS_TO_TICKS(12));
      continue;
    }

    if (wasRealtimeGuarded) {
      // Guard just cleared after >=500 ms of true audio silence. Force one clean
      // full resync now; there are no active voices, so this traffic cannot
      // steal realtime headroom from sounding audio.
      if (frameInitialized) sentFrameGeneration = frameGeneration ^ 0xFFFFFFFFU;
      if (paramStateInitialized) sentParamStateGeneration = paramStateGeneration ^ 0xFFFFFFFFU;
      if (fxStateInitialized) sentFxStateGeneration = fxStateGeneration ^ 0xFFFFFFFFU;
      if (seqStateInitialized) sentSeqStateGeneration = seqStateGeneration ^ 0xFFFFFFFFU;
      if (presetStateInitialized) sentPresetStateGeneration = presetStateGeneration ^ 0xFFFFFFFFU;
      if (multiStateInitialized) sentMultiStateGeneration = multiStateGeneration ^ 0xFFFFFFFFU;
      if (multiNameStateInitialized) sentMultiNameStateGeneration = multiNameStateGeneration ^ 0xFFFFFFFFU;
      if (telemetryInitialized) sentTelemetryGeneration = telemetryGeneration ^ 0xFFFFFFFFU;
      lastHealthMs = 0;
      wasRealtimeGuarded = false;
    }

    // A008: explicit host resync request. Force all initialized snapshots to be
    // retransmitted without touching synth state or the ControlTask.
    const uint32_t resyncNow = gResyncRequests;
    if (resyncNow != seenResyncRequests) {
      seenResyncRequests = resyncNow;
      if (frameInitialized) sentFrameGeneration = frameGeneration ^ 0xFFFFFFFFU;
      if (paramStateInitialized) sentParamStateGeneration = paramStateGeneration ^ 0xFFFFFFFFU;
      if (fxStateInitialized) sentFxStateGeneration = fxStateGeneration ^ 0xFFFFFFFFU;
      if (seqStateInitialized) sentSeqStateGeneration = seqStateGeneration ^ 0xFFFFFFFFU;
      if (presetStateInitialized) sentPresetStateGeneration = presetStateGeneration ^ 0xFFFFFFFFU;
      if (multiStateInitialized) sentMultiStateGeneration = multiStateGeneration ^ 0xFFFFFFFFU;
      if (multiNameStateInitialized) sentMultiNameStateGeneration = multiNameStateGeneration ^ 0xFFFFFFFFU;
      if (telemetryInitialized) sentTelemetryGeneration = telemetryGeneration ^ 0xFFFFFFFFU;
      lastHealthMs = 0;
    }

    // At most one <=64-byte TX chunk per task pass.
    serviceTx();

    uint32_t fg, tg, pg, xg, sg, rg, mg, ng;
    portENTER_CRITICAL(&remoteMux);
    fg = frameGeneration;
    tg = telemetryGeneration;
    pg = paramStateGeneration;
    xg = fxStateGeneration;
    sg = seqStateGeneration;
    rg = presetStateGeneration;
    mg = multiStateGeneration;
    ng = multiNameStateGeneration;
    portEXIT_CRITICAL(&remoteMux);

    if (!txBusy()) {
      // Fast parameter state gets first priority because its 57-byte payload is compact
      // and directly affects perceived UI responsiveness.
      if (pg != sentParamStateGeneration) {
        portENTER_CRITICAL(&remoteMux);
        paramState = mirrorParamState;
        sentParamStateGeneration = paramStateGeneration;
        portEXIT_CRITICAL(&remoteMux);

        queuePacket(TYPE_PARAM_STATE, ++paramSeq,
                    (const uint8_t *)&paramState, sizeof(paramState));
      }
      else if (sg != sentSeqStateGeneration) {
        portENTER_CRITICAL(&remoteMux);
        seqState = mirrorSeqState;
        sentSeqStateGeneration = seqStateGeneration;
        portEXIT_CRITICAL(&remoteMux);
        queuePacket(TYPE_SEQ_STATE, ++seqStateSeq,
                    (const uint8_t *)&seqState, sizeof(seqState));
      }
      else if (mg != sentMultiStateGeneration) {
        portENTER_CRITICAL(&remoteMux);
        multiState = mirrorMultiState;
        sentMultiStateGeneration = multiStateGeneration;
        portEXIT_CRITICAL(&remoteMux);
        queuePacket(TYPE_MULTI_STATE, ++multiStateSeq,
                    (const uint8_t *)&multiState, sizeof(multiState));
      }
      else if (ng != sentMultiNameStateGeneration) {
        portENTER_CRITICAL(&remoteMux);
        multiNameState = mirrorMultiNameState;
        sentMultiNameStateGeneration = multiNameStateGeneration;
        portEXIT_CRITICAL(&remoteMux);
        queuePacket(TYPE_MULTI_NAME_STATE, ++multiNameStateSeq,
                    (const uint8_t *)&multiNameState, sizeof(multiNameState));
        ++gMultiNameStatesSent;
      }
      else if (rg != sentPresetStateGeneration) {
        portENTER_CRITICAL(&remoteMux);
        presetState = mirrorPresetState;
        sentPresetStateGeneration = presetStateGeneration;
        portEXIT_CRITICAL(&remoteMux);
        queuePacket(TYPE_PRESET_STATE, ++presetStateSeq,
                    (const uint8_t *)&presetState, sizeof(presetState));
      }
      else if (xg != sentFxStateGeneration) {
        portENTER_CRITICAL(&remoteMux);
        fxState = mirrorFxState;
        sentFxStateGeneration = fxStateGeneration;
        portEXIT_CRITICAL(&remoteMux);
        queuePacket(TYPE_FX_STATE, ++fxSeq,
                    (const uint8_t *)&fxState, sizeof(fxState));
      }
      else if (!gRealtimeGuard && fg != sentFrameGeneration) {
        portENTER_CRITICAL(&remoteMux);
        memcpy(frame, mirrorFrame, sizeof(frame));
        sentFrameGeneration = frameGeneration;
        portEXIT_CRITICAL(&remoteMux);

        if (!queuePacket(TYPE_OLED, ++frameSeq, frame, sizeof(frame))) {
          ++gFramesDropped;
        }
      }
      else if (tg != sentTelemetryGeneration) {
        portENTER_CRITICAL(&remoteMux);
        telemetry = mirrorTelemetry;
        sentTelemetryGeneration = telemetryGeneration;
        portEXIT_CRITICAL(&remoteMux);

        if (!queuePacket(TYPE_TELEMETRY, ++telemetrySeq,
                         (const uint8_t *)&telemetry, sizeof(telemetry))) {
          ++gTelemetryDropped;
        }
      }
      else if ((uint32_t)(millis() - lastHealthMs) >= 1000U) {
        RTALRemote::LinkHealth h = {};
        h.uptimeMs = millis();
        h.framesSent = gFramesSent;
        h.framesDropped = gFramesDropped;
        h.paramStatesSent = gParamStatesSent;
        h.telemetrySent = gTelemetrySent;
        h.txStallRecoveries = gTxStallRecoveries;
        h.commandDrops = gCommandDrops;
        h.writeMaxUs = gWriteMaxUs;
        h.sessionId = sessionId;
        h.protocolVersion = PACKET_VERSION;
        h.flags = 0;
        if (connectedNow) h.flags |= 0x01;
        if (frameInitialized) h.flags |= 0x02;
        if (paramStateInitialized) h.flags |= 0x04;
        if (fxStateInitialized) h.flags |= 0x08;
        if (seqStateInitialized) h.flags |= 0x10;
        if (presetStateInitialized) h.flags |= 0x20;
        if (multiStateInitialized) h.flags |= 0x40;
        if (gRealtimeGuard) h.flags |= 0x80;
        if (queuePacket(TYPE_LINK_HEALTH, ++healthSeq,
                        (const uint8_t *)&h, sizeof(h))) {
          lastHealthMs = millis();
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(gRealtimeGuard ? 8 : 4));
  }
}

} // namespace
#endif

namespace RTALRemote {

void begin() {
#if RTAL_REMOTE_USB_ENABLED
  if (remoteActive) return;

  remoteActive = true;

  xTaskCreatePinnedToCore(
    remoteTask,
    "RTALRemoteCDC_SAFE",
    3072,
    nullptr,
    1,
    &remoteTaskHandle,
    0
  );

  Serial.println(F("RTAL REMOTE CDC SAFE_A014A MULTI_CORE1: enabled"));
  Serial.println(F("RTAL REMOTE CDC SAFE_A014A MULTI_CORE1: preset + FX preset + Multi Core + link health"));
#else
  Serial.println(F("RTAL REMOTE CDC SAFE_A003: disabled"));
#endif
}

void captureFrame(const uint8_t *frameBuffer, size_t length) {
#if RTAL_REMOTE_USB_ENABLED
  if (!frameBuffer || length < OLED_FRAME_BYTES) return;
  if (!clientConnected() || gRealtimeGuard) return;

  static uint32_t lastFramePublishMs = 0;
  const uint32_t now = millis();

  portENTER_CRITICAL(&remoteMux);
  const bool changed = !frameInitialized ||
                       memcmp(mirrorFrame, frameBuffer, OLED_FRAME_BYTES) != 0;
  const bool keepAlive = frameInitialized &&
                         (uint32_t)(now - lastFramePublishMs) >= 1000U;
  if (changed || keepAlive) {
    if (changed) memcpy(mirrorFrame, frameBuffer, OLED_FRAME_BYTES);
    ++frameGeneration;
    frameInitialized = true;
    lastFramePublishMs = now;
  }
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)frameBuffer;
  (void)length;
#endif
}

void publishTelemetry(const Telemetry &telemetry) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;

  portENTER_CRITICAL(&remoteMux);
  mirrorTelemetry = telemetry;
  ++telemetryGeneration;
  telemetryInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)telemetry;
#endif
}

void publishParamState(const ParamState &state) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;

  portENTER_CRITICAL(&remoteMux);
  // A008 LINK_HEALTH1: publish the compact state on every 20 Hz producer tick.
  // This doubles as a parameter-channel heartbeat, so an unchanged synth state
  // can still be distinguished from a stalled parameter stream.
  mirrorParamState = state;
  ++paramStateGeneration;
  paramStateInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)state;
#endif
}

void publishFxState(const FxState &state) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;
  portENTER_CRITICAL(&remoteMux);
  mirrorFxState = state;
  ++fxStateGeneration;
  fxStateInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)state;
#endif
}

void publishSeqState(const SeqState &state) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;
  portENTER_CRITICAL(&remoteMux);
  mirrorSeqState = state;
  ++seqStateGeneration;
  seqStateInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)state;
#endif
}

void publishPresetState(const PresetState &state) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;
  portENTER_CRITICAL(&remoteMux);
  mirrorPresetState = state;
  ++presetStateGeneration;
  presetStateInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)state;
#endif
}

void publishMultiState(const MultiState &state) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;
  portENTER_CRITICAL(&remoteMux);
  mirrorMultiState = state;
  ++multiStateGeneration;
  multiStateInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)state;
#endif
}

void publishMultiNameState(const MultiNameState &state) {
#if RTAL_REMOTE_USB_ENABLED
  if (!clientConnected() || gRealtimeGuard) return;
  portENTER_CRITICAL(&remoteMux);
  mirrorMultiNameState = state;
  ++multiNameStateGeneration;
  multiNameStateInitialized = true;
  portEXIT_CRITICAL(&remoteMux);
#else
  (void)state;
#endif
}

bool popCommand(Command &command) {
#if RTAL_REMOTE_USB_ENABLED
  bool ok = false;

  portENTER_CRITICAL(&remoteMux);

  if (commandTail != commandHead) {
    command = commandQueue[commandTail];
    commandTail = (uint8_t)((commandTail + 1U) % COMMAND_QUEUE_SIZE);
    ok = true;
  }

  portEXIT_CRITICAL(&remoteMux);
  return ok;
#else
  (void)command;
  return false;
#endif
}

bool active() {
#if RTAL_REMOTE_USB_ENABLED
  return remoteActive;
#else
  return false;
#endif
}

bool clientConnected() {
#if RTAL_REMOTE_USB_ENABLED
  return (bool)RTALUSBSerial;
#else
  return false;
#endif
}

void setRealtimeGuard(bool enabled) {
#if RTAL_REMOTE_USB_ENABLED
  gRealtimeGuard = enabled;
#else
  (void)enabled;
#endif
}

bool realtimeGuard() {
#if RTAL_REMOTE_USB_ENABLED
  return gRealtimeGuard;
#else
  return false;
#endif
}

uint32_t framesSent() {
#if RTAL_REMOTE_USB_ENABLED
  return gFramesSent;
#else
  return 0;
#endif
}

uint32_t framesDropped() {
#if RTAL_REMOTE_USB_ENABLED
  return gFramesDropped;
#else
  return 0;
#endif
}

uint32_t writeMaxUs() {
#if RTAL_REMOTE_USB_ENABLED
  return gWriteMaxUs;
#else
  return 0;
#endif
}

uint32_t telemetrySent() {
#if RTAL_REMOTE_USB_ENABLED
  return gTelemetrySent;
#else
  return 0;
#endif
}

uint32_t telemetryDropped() {
#if RTAL_REMOTE_USB_ENABLED
  return gTelemetryDropped;
#else
  return 0;
#endif
}

uint32_t paramStatesSent() {
#if RTAL_REMOTE_USB_ENABLED
  return gParamStatesSent;
#else
  return 0;
#endif
}

uint32_t fxStatesSent() {
#if RTAL_REMOTE_USB_ENABLED
  return gFxStatesSent;
#else
  return 0;
#endif
}

uint32_t seqStatesSent() {
#if RTAL_REMOTE_USB_ENABLED
  return gSeqStatesSent;
#else
  return 0;
#endif
}

uint32_t presetStatesSent() {
#if RTAL_REMOTE_USB_ENABLED
  return gPresetStatesSent;
#else
  return 0;
#endif
}

uint32_t multiStatesSent() {
#if RTAL_REMOTE_USB_ENABLED
  return gMultiStatesSent;
#else
  return 0;
#endif
}

uint32_t txStallRecoveries() {
#if RTAL_REMOTE_USB_ENABLED
  return gTxStallRecoveries;
#else
  return 0;
#endif
}

uint32_t commandDrops() {
#if RTAL_REMOTE_USB_ENABLED
  return gCommandDrops;
#else
  return 0;
#endif
}

const char *transportName() {
#if RTAL_REMOTE_USB_ENABLED
  return "Native USB CDC SAFE A010 GRAPHICAL_SEQ1";
#else
  return "disabled";
#endif
}

} // namespace RTALRemote
