#ifndef RTAL_REMOTE_H
#define RTAL_REMOTE_H

#include <Arduino.h>
#include "USBCDC.h"

extern USBCDC RTALUSBSerial;

// Central build switch. RTAL_Remote.cpp is compiled as a separate translation
// unit, so the switch must live in a header visible to every source file.
#ifndef RTAL_REMOTE_USB_ENABLED
#define RTAL_REMOTE_USB_ENABLED 1
#endif

namespace RTALRemote {

static constexpr size_t OLED_FRAME_BYTES = 128U * 64U / 8U;

enum CommandType : uint8_t {
  CMD_NONE = 0,
  CMD_BUTTON_SHORT,
  CMD_BUTTON_LONG,
  CMD_ENCODER_DELTA,
  CMD_ENCODER_SHORT,
  CMD_ENCODER_LONG,
  CMD_PARAM_SET,
  CMD_FX_SET,
  CMD_FX_REQUEST_STATE,
  CMD_SEQ_SET,
  CMD_SEQ_TABLE_MODE,
  CMD_SYSTEM_SET,
  CMD_PRESET_LOAD,
  CMD_PRESET_SAVE,
  CMD_PRESET_REFRESH,
  CMD_MULTI_SET,
  CMD_MULTI_PRESET,
  CMD_MULTI_SELECT,
  CMD_MULTI_LOAD,
  CMD_MULTI_SAVE,
  CMD_MULTI_NAME_QUERY,
  CMD_PERFORMANCE_MODE
};

struct Command {
  CommandType type;
  int32_t value;
  uint8_t paramId;
  uint8_t paramValue;
  char text[16];
};

static constexpr uint8_t REMOTE_PARAM_COUNT = 57;

struct ParamState {
  uint8_t values[REMOTE_PARAM_COUNT];
};

static constexpr uint8_t REMOTE_SEQ_STEPS = 16;

struct __attribute__((packed)) SeqState {
  uint8_t tableMode;
  uint8_t currentStep;
  uint8_t runtimePart;
  uint8_t values[REMOTE_SEQ_STEPS];
};

static constexpr uint8_t REMOTE_PRESET_NAME_LEN = 16;

struct __attribute__((packed)) PresetState {
  uint8_t currentSlot;
  uint8_t entrySlot;
  uint8_t flags; // bit0 entry valid, bit1 factory slot, bit2 SD available, bit3 multi active
  char currentName[REMOTE_PRESET_NAME_LEN];
  char entryName[REMOTE_PRESET_NAME_LEN];
};



static constexpr uint8_t REMOTE_MULTI_PARTS = 4;
static constexpr uint8_t REMOTE_MULTI_NAME_LEN = 16;

struct __attribute__((packed)) MultiPartState {
  uint8_t presetNumber;
  uint8_t midiChannel;
  uint8_t volume;
  int8_t transpose;
  uint8_t voiceReserve;
  uint8_t arpGatePct;
  uint8_t flags;        // bit0 enabled, bit1 mute
  uint8_t activeVoices;
  uint8_t keyLow;
  uint8_t keyHigh;
  uint8_t rootNote;
  int8_t pan;
  char programName[REMOTE_MULTI_NAME_LEN];
};

struct __attribute__((packed)) MultiNameState {
  uint8_t slot;
  uint8_t flags; // bit0 name/file exists, bit1 SD available
  char name[REMOTE_MULTI_NAME_LEN];
};

struct __attribute__((packed)) MultiState {
  uint8_t flags;        // bit0 Multi performance active, bit1 SD available
  uint8_t currentSlot;  // M000..M127
  uint8_t selectedPart; // 0..3
  uint8_t reserved;
  char multiName[REMOTE_MULTI_NAME_LEN];
  MultiPartState part[REMOTE_MULTI_PARTS];
};

static constexpr uint8_t FX_STATE_MAX_ENTRIES = 56;

struct __attribute__((packed)) FxStateEntry {
  uint8_t paramId;
  int32_t value;
};

struct __attribute__((packed)) FxState {
  uint8_t connected;
  uint8_t count;
  FxStateEntry entries[FX_STATE_MAX_ENTRIES];
};

struct LinkHealth {
  uint32_t uptimeMs;
  uint32_t framesSent;
  uint32_t framesDropped;
  uint32_t paramStatesSent;
  uint32_t telemetrySent;
  uint32_t txStallRecoveries;
  uint32_t commandDrops;
  uint32_t writeMaxUs;
  uint16_t sessionId;
  uint8_t protocolVersion;
  uint8_t flags;
};


struct __attribute__((packed)) SmartTelemetry {
  uint32_t audioDspLastUs;
  uint32_t audioDspMaxUs;
  uint32_t audioDspOverruns;
  uint8_t activeVoices;
  uint8_t flags; // bit0 audio-safe, bit1 smart packet suppressed by low headroom
};

struct Telemetry {
  char firmware[24];
  char program[24];
  char wavetable[24];
  uint32_t uptimeMs;
  uint32_t freeHeap;
  uint32_t freePsram;
  uint32_t largestHeapBlock;
  uint32_t audioDspLastUs;
  uint32_t audioDspMaxUs;
  uint32_t audioDspOverruns;
  uint32_t i2sShortWrites;
  uint32_t audioQueueDrops;
  uint32_t midiQueueDrops;
  uint32_t midiDeferred;
  uint32_t midiClockTicks;
  uint32_t midiClockJitterMaxUs;
  uint32_t voiceSteals;
  uint8_t activeVoices;
  uint8_t polyLoad;
  uint8_t cutoff;
  uint8_t resonance;
  uint8_t wavePosition;
  uint8_t chorus;
  uint8_t drive;
  uint8_t clockSource;
  uint16_t bpmX100;
  uint8_t midiChannel;
  uint8_t ccMode;
  uint8_t ccBank;
};

void begin();
void captureFrame(const uint8_t *frameBuffer, size_t length = OLED_FRAME_BYTES);
void publishTelemetry(const Telemetry &telemetry);
void publishParamState(const ParamState &state);
void publishFxState(const FxState &state);
void publishSeqState(const SeqState &state);
void publishPresetState(const PresetState &state);
void publishMultiState(const MultiState &state);
void publishMultiNameState(const MultiNameState &state);
bool popCommand(Command &command);
bool active();
bool clientConnected();
void setRealtimeGuard(bool enabled);
bool realtimeGuard();
uint32_t framesSent();
uint32_t framesDropped();
uint32_t writeMaxUs();
uint32_t telemetrySent();
uint32_t telemetryDropped();
uint32_t paramStatesSent();
uint32_t fxStatesSent();
uint32_t seqStatesSent();
uint32_t presetStatesSent();
uint32_t multiStatesSent();
uint32_t txStallRecoveries();
uint32_t commandDrops();
const char *transportName();

}  // namespace RTALRemote

#endif
