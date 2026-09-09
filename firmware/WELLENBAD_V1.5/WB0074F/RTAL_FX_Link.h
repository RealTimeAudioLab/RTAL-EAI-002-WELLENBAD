#pragma once
#include <Arduino.h>

// RTAL WELLENBAD <-> FX ESP control protocol, v1.
// Audio remains on the separate I2S connection. This UART carries control,
// clock/tempo metadata and diagnostics only.

namespace RTALFxProtocol {
constexpr uint8_t SOF1 = 0x52; // 'R'
constexpr uint8_t SOF2 = 0x46; // 'F'
constexpr uint8_t VERSION = 1;
constexpr uint32_t BAUD = 115200;
constexpr int8_t SYNTH_RX_PIN = 8;
constexpr int8_t SYNTH_TX_PIN = 5;

enum Type : uint8_t {
  HELLO = 0x01,
  HELLO_ACK = 0x02,
  PING = 0x03,
  PONG = 0x04,
  SET_PARAM = 0x10,
  GET_STATE = 0x11,
  STATE = 0x12,
  CLOCK_STATE = 0x20
};

enum Param : uint8_t {
  P_DELAY_ENABLE = 1,
  P_DELAY_FREEZE,
  P_DELAY_LEFT_MS,
  P_DELAY_RIGHT_MS,
  P_DELAY_FEEDBACK,
  P_DELAY_LEVEL,
  P_DELAY_LOWPASS_HZ,
  P_DELAY_HIGHPASS_HZ,
  P_DELAY_SATURATION,
  P_DELAY_CROSSFEED,
  P_DELAY_DUCK_AMOUNT,
  P_DELAY_DUCK_THRESHOLD_DB,
  P_DELAY_DUCK_RELEASE_MS,
  P_DELAY_MODE,
  P_DELAY_LEFT_DIV,
  P_DELAY_RIGHT_DIV,
  P_CLOCK_SOURCE = 32,
  P_CLOCK_BPM_X100,
  P_LINK_STATUS = 48,

  // Build0039: shared RTAL modulation core. Reserved range 64..79.
  P_MOD_ENABLE = 64,
  P_MOD_LFO_SHAPE,        // 0=SIN, 1=TRI
  P_MOD_RATE_HZ,          // Q16.16 Hz, Free mode
  P_MOD_SYNC,             // 0=FREE, 1=SYNC
  P_MOD_DIVISION,         // shared musical division index 0..11
  P_MOD_DEPTH,            // Q16.16 0..1
  P_MOD_STEREO_PHASE_DEG, // Q16.16 degrees 0..180
  P_MOD_SMOOTHING_MS,     // Q16.16 milliseconds 5..500
  P_MOD_LFO_LEFT,         // Q16.16 diagnostic -1..+1 (read-only)
  P_MOD_LFO_RIGHT,        // Q16.16 diagnostic -1..+1 (read-only)
  P_MOD_EFFECT_SELECT,    // Build0044: 0=OFF,1=CHORUS,2=FLANGER,3=PHASER

  // Build0041: high-quality stereo chorus. Reserved range 80..95.
  P_CHORUS_ENABLE = 80,
  P_CHORUS_MIX,            // Q16.16 0..1
  P_CHORUS_DELAY_MS,       // Q16.16 8..24 ms
  P_CHORUS_TONE_HZ,        // Q16.16 2000..18000 Hz

  // Build0042: high-quality stereo flanger. Reserved range 96..111.
  P_FLANGER_ENABLE = 96,
  P_FLANGER_MIX,            // Q16.16 0..1
  P_FLANGER_DELAY_MS,       // Q16.16 0.20..12.0 ms
  P_FLANGER_FEEDBACK,       // Q16.16 -0.95..+0.95
  P_FLANGER_HPF_HZ,         // Q16.16 20..2000 Hz
  P_FLANGER_SATURATION,     // Q16.16 0..1

  // Build0043: stereo phaser. Uses range 112..118.
  P_PHASER_ENABLE = 112,
  P_PHASER_MIX,
  P_PHASER_STAGES,           // integer 2/4 (Phaser Final)
  P_PHASER_FEEDBACK,         // Q16.16 -0.95..+0.95
  P_PHASER_HPF_HZ,           // Q16.16 20..2000 Hz
  P_PHASER_SATURATION,       // Q16.16 0..1
  P_PHASER_CENTER_HZ,        // Q16.16 200..4000 Hz

  // Build0045: final stereo width stage. IDs stay below 128 for v1 protocol cache.
  P_WIDTH_ENABLE = 120,
  P_WIDTH_AMOUNT,            // Q16.16 0.0..2.0; 1.0 = original stereo

  // Build0046: 4-line stereo FDN reverb. Uses final free v1 cache IDs 122..127.
  P_REVERB_ENABLE = 122,
  P_REVERB_MIX,
  P_REVERB_SIZE,
  P_REVERB_DECAY,
  P_REVERB_DAMPING_HZ,
  P_REVERB_PREDELAY_MS
};

struct __attribute__((packed)) Frame {
  uint8_t sof1;
  uint8_t sof2;
  uint8_t version;
  uint8_t type;
  uint8_t seq;
  uint8_t param;
  int32_t value;
  uint8_t crc;
};
}

struct RTALFxLinkStats {
  bool connected;
  uint32_t txFrames;
  uint32_t rxFrames;
  uint32_t crcErrors;
  uint32_t versionErrors;
  uint32_t timeouts;
  uint32_t lastRxAgeMs;
};

// Persistent FX preset payload. Exactly 49 musical/control parameters are
// stored. Delay Freeze is intentionally excluded because its audio buffer is
// transient; Mod-LFO outputs are diagnostics and are also excluded.
constexpr uint8_t RTAL_FX_PRESET_PARAM_COUNT = 49;
struct RTALFxPresetState {
  uint32_t magic;
  uint8_t version;
  uint8_t count;
  uint16_t reserved;
  int32_t value[RTAL_FX_PRESET_PARAM_COUNT];
};

class RTALFxLink {
public:
  static void begin();
  static void service(uint8_t clockSource, uint16_t internalBpm, uint32_t midiBpmX100);
  static bool connected();
  static RTALFxLinkStats stats();
  static void requestState();
  static void setBool(uint8_t param, bool value);
  static void setInt(uint8_t param, int32_t value);
  static void setFloat(uint8_t param, float value);
  // A011_FIX1: Chorus/Flanger/Phaser share one Mod-FX core and are mutually exclusive.
  static void selectModEffect(uint8_t effect); // 0=OFF, 1=CHORUS, 2=FLANGER, 3=PHASER
  static void setModEffectEnabled(uint8_t effect, bool enabled);
  static bool getRemoteValue(uint8_t param, int32_t &value);
  static void capturePresetState(RTALFxPresetState &state);
  static bool presetStateValid(const RTALFxPresetState &state);
  static void applyPresetState(const RTALFxPresetState &state);
  static void makePresetDefaults(RTALFxPresetState &state);
  static void applyPresetDefaults();
  static float q16ToFloat(int32_t value);
  static int32_t floatToQ16(float value);
};
