#pragma once
#include <Arduino.h>

// Console
constexpr uint32_t RTAL_LOG_BAUD = 115200;
constexpr uint32_t RTAL_SERIAL_WAIT_MS = 1500;

// Audio format
constexpr uint32_t RTAL_AUDIO_SAMPLE_RATE = 44100;
constexpr size_t RTAL_AUDIO_BLOCK_FRAMES = 128;
constexpr size_t RTAL_AUDIO_CHANNELS = 2;
constexpr size_t RTAL_AUDIO_BYTES_PER_SAMPLE = 4;
constexpr size_t RTAL_AUDIO_BLOCK_BYTES =
    RTAL_AUDIO_BLOCK_FRAMES * RTAL_AUDIO_CHANNELS * RTAL_AUDIO_BYTES_PER_SAMPLE;

// I2S pins and ports
constexpr int RTAL_I2S_BCLK_PIN = 18;
constexpr int RTAL_I2S_LRCK_PIN = 16;
constexpr int RTAL_I2S_DATA_IN_PIN = 5;
constexpr int RTAL_I2S_DATA_OUT_PIN = 17;

constexpr int RTAL_I2S_RX_PORT = 0;
constexpr int RTAL_I2S_TX_PORT = 1;

constexpr int RTAL_I2S_DMA_BUFFER_COUNT = 8;
constexpr int RTAL_I2S_DMA_BUFFER_FRAMES = RTAL_AUDIO_BLOCK_FRAMES;

// Short I2S timeout keeps the task alive while no external master is connected.
constexpr uint32_t RTAL_I2S_IO_TIMEOUT_MS = 20;

// Clock-state qualification
constexpr uint32_t RTAL_CLOCK_REQUIRED_GOOD_BLOCKS = 8;
constexpr uint32_t RTAL_CLOCK_MIN_BLOCK_INTERVAL_US = 1500;
constexpr uint32_t RTAL_CLOCK_MAX_BLOCK_INTERVAL_US = 8000;
constexpr uint32_t RTAL_CLOCK_LOST_TIMEOUT_MS = 250;

// Tasks
constexpr BaseType_t RTAL_SYSTEM_TASK_CORE = 0;
constexpr UBaseType_t RTAL_SYSTEM_TASK_PRIORITY = 2;
constexpr uint32_t RTAL_SYSTEM_TASK_STACK_WORDS = 6144; // 0046g: forensic reporting headroom

constexpr BaseType_t RTAL_WATCHDOG_TASK_CORE = 0;
constexpr UBaseType_t RTAL_WATCHDOG_TASK_PRIORITY = 4;
constexpr uint32_t RTAL_WATCHDOG_TASK_STACK_WORDS = 4096;

constexpr BaseType_t RTAL_AUDIO_TASK_CORE = 1;
constexpr UBaseType_t RTAL_AUDIO_TASK_PRIORITY = 24;
constexpr uint32_t RTAL_AUDIO_TASK_STACK_WORDS = 4096;

// Diagnostics timing
constexpr uint32_t RTAL_SYSTEM_HEARTBEAT_INTERVAL_MS = 500;
constexpr uint32_t RTAL_SYSTEM_REPORT_INTERVAL_MS = 5000;
constexpr uint32_t RTAL_WATCHDOG_CHECK_INTERVAL_MS = 250;

constexpr uint32_t RTAL_SYSTEM_TASK_TIMEOUT_MS = 3000;
constexpr uint32_t RTAL_AUDIO_TASK_TIMEOUT_MS = 1000;

constexpr UBaseType_t RTAL_STACK_WARNING_WORDS = 512;
constexpr UBaseType_t RTAL_STACK_CRITICAL_WORDS = 256;

constexpr size_t RTAL_MIN_FREE_INTERNAL_HEAP_BYTES = 100000;
constexpr size_t RTAL_MIN_PSRAM_BYTES = 4UL * 1024UL * 1024UL;

constexpr size_t RTAL_MAX_REGISTERED_TASKS = 8;
constexpr size_t RTAL_EVENT_LOG_CAPACITY = 16;


// Build0046i production diagnostics profile. The stable candidate keeps only
// low-cost health telemetry in the realtime path. Deep analyzers remain available
// here for dedicated diagnostic builds but are OFF by default.
constexpr bool RTAL_PRODUCTION_PROFILE = true;

// Audio-format diagnostics
constexpr bool RTAL_AUDIO_FORMAT_ANALYSIS_ENABLED = false;
constexpr uint32_t RTAL_AUDIO_ANALYSIS_REPORT_INTERVAL_MS = 5000;

// Build0046e realtime optimization. Audio/DSP algorithms and effect parameters
// remain unchanged. Expensive diagnostics are decimated or disabled in the
// realtime task while deadline and I2S transfer timing counters remain active.
constexpr uint32_t RTAL_AUDIO_ANALYSIS_FRAMES_PER_BLOCK = 4; // 4 evenly spaced frames of 128
constexpr uint32_t RTAL_AUDIO_PEAK_BLOCK_DECIMATION = 32;     // lightweight input peak 1 of 32 blocks
constexpr bool RTAL_RAW_INPUT_DIAGNOSTICS_ENABLED = false;    // 0046d freeze scanner removed from hot path
constexpr uint32_t RTAL_RAW_FREEZE_THRESHOLD_FRAMES = 4096;   // retained for optional diagnostics
constexpr uint32_t RTAL_RAW_REPEAT_BLOCK_THRESHOLD = 32;      // retained for optional diagnostics
constexpr bool RTAL_I2S_WAIT_TIMING_ENABLED = false;          // production: error/byte counters retained, micro-timing OFF

// Build0046f Peak Forensics. These counters do not alter DSP parameters or
// routing. CPU cycle reads are used around existing block/sample stages; only
// exceptional blocks are copied into the small internal-RAM event rings.
constexpr bool RTAL_PEAK_FORENSICS_ENABLED = false;
constexpr uint32_t RTAL_PEAK_FORENSICS_TRIGGER_US = 2800;
constexpr uint8_t RTAL_PEAK_FORENSICS_RING_SIZE = 12;
constexpr uint32_t RTAL_PEAK_HIST_2600_US = 2600;
constexpr uint32_t RTAL_PEAK_HIST_2700_US = 2700;
constexpr uint32_t RTAL_PEAK_HIST_2800_US = 2800;
constexpr uint32_t RTAL_PEAK_HIST_3000_US = 3000;
constexpr uint32_t RTAL_PEAK_HIST_3200_US = 3200;
constexpr uint32_t RTAL_CPU_CYCLES_PER_US = 240; // ESP32-S3 fixed at 240 MHz in this project

// Build0046h Delay Coefficient Optimization. The 0046g substage profiler is
// removed from the realtime hot path. Parameter smoothing remains sample-accurate,
// while derived HPF/LPF coefficients are refreshed at most once per 16 samples
// and only while either cutoff is actually moving.
constexpr bool RTAL_DELAY_FORENSICS_ENABLED = false;
constexpr uint32_t RTAL_DELAY_FORENSICS_SAMPLE_DECIMATION = 16;
constexpr uint32_t RTAL_DELAY_COEFFICIENT_UPDATE_INTERVAL = 16;

// Build0046j loudness-stable effect mixing. Chorus, Flanger, Phaser and
// Reverb retain their existing linear Dry/Wet ratio, then normalize the
// crossfade to constant power. The normalization is change-driven and
// linearly interpolated per block; no sqrtf() is executed in the sample loop.
// Stereo Delay remains additive/send-style and is intentionally unchanged.
constexpr bool RTAL_CONSTANT_POWER_EFFECT_MIX_ENABLED = true;

// Samples are analyzed exactly as received in the 32-bit I2S container.
// These masks help identify active payload-bit regions.
constexpr uint32_t RTAL_ANALYSIS_LOW_8_MASK   = 0x000000FFUL;
constexpr uint32_t RTAL_ANALYSIS_LOW_16_MASK  = 0x0000FFFFUL;
constexpr uint32_t RTAL_ANALYSIS_LOW_24_MASK  = 0x00FFFFFFUL;
constexpr uint32_t RTAL_ANALYSIS_HIGH_24_MASK = 0xFFFFFF00UL;


// Normalized internal audio path
constexpr bool RTAL_NORMALIZED_AUDIO_PATH_ENABLED = true;
constexpr uint8_t RTAL_INPUT_PAYLOAD_SHIFT = 16;
constexpr bool RTAL_DC_BLOCKER_ENABLED = true;
constexpr float RTAL_DC_BLOCKER_R = 0.995f;
constexpr uint8_t RTAL_OUTPUT_PAYLOAD_SHIFT = 16;


// DSP framework – Build 0012
constexpr bool RTAL_DSP_FRAMEWORK_ENABLED = true;
constexpr bool RTAL_DSP_BYPASS_DEFAULT = false;
constexpr float RTAL_DSP_DRY_DEFAULT = 1.0f;
constexpr float RTAL_DSP_WET_DEFAULT = 0.0f;
constexpr float RTAL_DSP_PARAMETER_SMOOTHING = 0.0025f;
constexpr bool RTAL_DSP_LIMITER_ENABLED = true;
constexpr float RTAL_DSP_LIMITER_THRESHOLD = 0.985f;
constexpr float RTAL_DSP_LIMITER_RELEASE = 0.9995f;
constexpr float RTAL_DSP_INT16_TO_FLOAT = 1.0f / 32768.0f;
constexpr float RTAL_DSP_FLOAT_TO_INT16 = 32768.0f;


// Optimized single-pass DSP kernel – Build 0013
constexpr bool RTAL_SINGLE_PASS_DSP_ENABLED = true;
constexpr bool RTAL_FX_BYPASS_DEFAULT = false;
constexpr bool RTAL_HARD_BYPASS_DEFAULT = false;
constexpr float RTAL_DSP_SMOOTHING_COEFFICIENT = 0.0025f;
constexpr bool RTAL_DSP_STAGE_TIMING_ENABLED = false; // Build0046e: disable stage sampler in realtime build


// Low-overhead profiler – Build 0013b
constexpr bool RTAL_DSP_BLOCK_PROFILER_ENABLED = true;
constexpr bool RTAL_DSP_RMS_DIAGNOSTICS_ENABLED = false;

// Clock loss after qualification is based on read failures/timeouts,
// not on processing-dependent full-block return intervals.
constexpr bool RTAL_CLOCK_VALIDATE_INTERVAL_AFTER_PRESENT = false;


// Production DSP kernel – Build 0013c
constexpr uint8_t RTAL_DSP_RMS_DECIMATION = 64; // Build0046e: diagnostics only, lower realtime cost
constexpr bool RTAL_DSP_FAST_UNITY_DRY_PATH = true;
constexpr bool RTAL_DSP_FAST_LIMITER_PATH = true;
constexpr float RTAL_DSP_UNITY_EPSILON = 0.00001f;


// Stereo Delay – Build 0014
constexpr bool RTAL_STEREO_DELAY_ENABLED = true;
constexpr float RTAL_DELAY_TIME_LEFT_MS_DEFAULT = 250.0f;
constexpr float RTAL_DELAY_TIME_RIGHT_MS_DEFAULT = 375.0f;
constexpr float RTAL_DELAY_FEEDBACK_DEFAULT = 0.0f;
constexpr float RTAL_DELAY_LEVEL_DEFAULT = 0.25f;
constexpr float RTAL_DELAY_MAX_MS = 1000.0f;
constexpr float RTAL_DELAY_SMOOTHING = 0.0015f;


// Extended Stereo Delay – Build 0015
constexpr float RTAL_DELAY_FEEDBACK_DEFAULT_BUILD0015 = 0.35f;
constexpr float RTAL_DELAY_FEEDBACK_LOWPASS_HZ_DEFAULT = 6000.0f;
constexpr float RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MIN = 250.0f;
constexpr float RTAL_DELAY_FEEDBACK_LOWPASS_HZ_MAX = 18000.0f;
constexpr bool RTAL_DELAY_PINGPONG_DEFAULT = false;
constexpr float RTAL_DELAY_PARAMETER_SMOOTHING_BUILD0015 = 0.0020f;


// Serial Live Control – Build 0016
constexpr bool RTAL_SERIAL_CONTROL_ENABLED = true;
constexpr size_t RTAL_SERIAL_COMMAND_BUFFER_SIZE = 128;
constexpr uint32_t RTAL_SERIAL_CONTROL_POLL_MS = 2;

// RAM Delay Presets – Build 0017b
constexpr bool RTAL_DELAY_RAM_PRESETS_ENABLED = true;
constexpr uint8_t RTAL_DELAY_RAM_PRESET_COUNT = 8;


// Safe NVS Delay Presets – Build 0018
constexpr bool RTAL_DELAY_NVS_PRESETS_ENABLED = true;
constexpr uint8_t RTAL_DELAY_NVS_PRESET_COUNT = 8;
constexpr uint16_t RTAL_DELAY_NVS_FORMAT_VERSION = 8;


// MIDI CC Control – Build 0019
constexpr bool RTAL_MIDI_CONTROL_ENABLED = false; // Build0036: UART2 reassigned to WELLENBAD FX link
constexpr int8_t RTAL_MIDI_RX_PIN = 40;
constexpr int8_t RTAL_MIDI_TX_PIN = 39;
constexpr uint32_t RTAL_MIDI_BAUD = 31250;
constexpr uint8_t RTAL_MIDI_CHANNEL = 1; // 1..16, 0 = omni

constexpr uint8_t RTAL_MIDI_CC_DELAY_LEVEL = 118;
constexpr uint8_t RTAL_MIDI_CC_DELAY_FEEDBACK = 76;
constexpr uint8_t RTAL_MIDI_CC_DELAY_LEFT_TIME = 119;
constexpr uint8_t RTAL_MIDI_CC_DELAY_RIGHT_TIME = 75;
constexpr uint8_t RTAL_MIDI_CC_DELAY_LOWPASS = 86;
constexpr uint8_t RTAL_MIDI_CC_DELAY_PINGPONG = 89;
constexpr uint8_t RTAL_MIDI_CC_DELAY_ENABLE = 115;


// MIDI Live Monitor – Build 0019b
constexpr bool RTAL_MIDI_LIVE_MONITOR_ENABLED = false;
constexpr bool RTAL_MIDI_RAW_BYTE_MONITOR_ENABLED = false;
constexpr uint32_t RTAL_MIDI_ACTIVITY_HOLD_MS = 1000;


// MIDI Clock Sync – Build 0020
constexpr bool RTAL_MIDI_CLOCK_SYNC_ENABLED = true;
constexpr uint32_t RTAL_MIDI_CLOCK_LOST_TIMEOUT_MS = 750;
constexpr uint8_t RTAL_MIDI_CLOCK_SMOOTHING_TICKS = 24;
constexpr float RTAL_MIDI_CLOCK_MIN_BPM = 20.0f;
constexpr float RTAL_MIDI_CLOCK_MAX_BPM = 300.0f;
constexpr bool RTAL_DELAY_SYNC_DEFAULT = false;


// Stable MIDI Clock Sync – Build 0020a
constexpr uint8_t RTAL_MIDI_CLOCK_APPLY_EVERY_TICKS = 24;
constexpr float RTAL_MIDI_CLOCK_DELAY_HYSTERESIS_MS = 4.0f;
constexpr float RTAL_MIDI_CLOCK_DELAY_QUANTIZE_MS = 0.5f;
constexpr float RTAL_MIDI_CLOCK_BPM_HYSTERESIS = 2.00f;
// Stable Sync Delay – Build 0046l
constexpr uint8_t RTAL_MIDI_CLOCK_TEMPO_CONFIRM_WINDOWS = 3;
constexpr float RTAL_MIDI_CLOCK_TEMPO_CONFIRM_TOLERANCE_BPM = 0.75f;
constexpr float RTAL_DELAY_SYNC_CROSSFADE_MS = 24.0f;


// MIDI Program Change – Build 0023
constexpr bool RTAL_MIDI_PROGRAM_CHANGE_ENABLED = true;
constexpr uint8_t RTAL_MIDI_PROGRAM_FIRST = 0;
constexpr uint8_t RTAL_MIDI_PROGRAM_COUNT = 8;


// NVS boot import – Build 0024
constexpr bool RTAL_DELAY_NVS_BOOT_IMPORT_ENABLED = true;


// Smooth MIDI Program Change – Build 0025
constexpr bool RTAL_DELAY_SMOOTH_PROGRAM_CHANGE_ENABLED = true;
constexpr uint32_t RTAL_DELAY_PRESET_FADE_OUT_MS = 45;
constexpr uint32_t RTAL_DELAY_PRESET_SETTLE_MS = 15;


// Startup Preset – Build 0027
constexpr bool RTAL_STARTUP_PRESET_ENABLED = true;
constexpr uint32_t RTAL_STARTUP_PRESET_DELAY_MS = 350;


// Preset Names & Metadata – Build 0028
constexpr size_t RTAL_DELAY_PRESET_NAME_LENGTH = 20;
constexpr uint16_t RTAL_DELAY_PRESET_METADATA_VERSION = 1;


// Tap Tempo & Internal Clock – Build 0030
constexpr float RTAL_INTERNAL_CLOCK_BPM_DEFAULT = 120.0f;
constexpr float RTAL_INTERNAL_CLOCK_BPM_MIN = 30.0f;
constexpr float RTAL_INTERNAL_CLOCK_BPM_MAX = 300.0f;
constexpr uint8_t RTAL_TAP_TEMPO_INTERVAL_COUNT = 4;
constexpr uint32_t RTAL_TAP_TEMPO_TIMEOUT_MS = 2500;
constexpr float RTAL_TAP_TEMPO_OUTLIER_RATIO = 0.35f;


// Feedback Highpass – Build 0031
constexpr float RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_DEFAULT = 20.0f;
constexpr float RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MIN = 20.0f;
constexpr float RTAL_DELAY_FEEDBACK_HIGHPASS_HZ_MAX = 4000.0f;
constexpr uint8_t RTAL_MIDI_CC_DELAY_HIGHPASS = 90;


// Feedback Saturation – Build 0032
constexpr float RTAL_DELAY_FEEDBACK_SATURATION_DEFAULT = 0.0f;
constexpr float RTAL_DELAY_FEEDBACK_SATURATION_MIN = 0.0f;
constexpr float RTAL_DELAY_FEEDBACK_SATURATION_MAX = 1.0f;
constexpr float RTAL_DELAY_FEEDBACK_SATURATION_THRESHOLD_MIN = 0.20f;
constexpr uint8_t RTAL_MIDI_CC_DELAY_SATURATION = 77;


// Continuous Stereo Crossfeed – Build 0033
constexpr float RTAL_DELAY_CROSSFEED_DEFAULT =
    RTAL_DELAY_PINGPONG_DEFAULT ? 1.0f : 0.0f;
constexpr float RTAL_DELAY_CROSSFEED_MIN = 0.0f;
constexpr float RTAL_DELAY_CROSSFEED_MAX = 1.0f;
// CC89, formerly Ping-Pong switch, is now continuous Crossfeed.
constexpr uint8_t RTAL_MIDI_CC_DELAY_CROSSFEED =
    RTAL_MIDI_CC_DELAY_PINGPONG;


// Ducking Delay – Build 0034
constexpr float RTAL_DELAY_DUCK_AMOUNT_DEFAULT = 0.0f;
constexpr float RTAL_DELAY_DUCK_AMOUNT_MIN = 0.0f;
constexpr float RTAL_DELAY_DUCK_AMOUNT_MAX = 1.0f;
constexpr float RTAL_DELAY_DUCK_THRESHOLD_DB_DEFAULT = -24.0f;
constexpr float RTAL_DELAY_DUCK_THRESHOLD_DB_MIN = -60.0f;
constexpr float RTAL_DELAY_DUCK_THRESHOLD_DB_MAX = 0.0f;
constexpr float RTAL_DELAY_DUCK_RELEASE_MS_DEFAULT = 350.0f;
constexpr float RTAL_DELAY_DUCK_RELEASE_MS_MIN = 50.0f;
constexpr float RTAL_DELAY_DUCK_RELEASE_MS_MAX = 2000.0f;

constexpr uint8_t RTAL_MIDI_CC_DELAY_DUCK_AMOUNT = 22;
constexpr uint8_t RTAL_MIDI_CC_DELAY_DUCK_THRESHOLD = 23;
constexpr uint8_t RTAL_MIDI_CC_DELAY_DUCK_RELEASE = 30;


// MIDI CC Safe Control – Build 0034b
constexpr uint8_t RTAL_MIDI_SERVICE_MAX_BYTES = 32;
constexpr bool RTAL_MIDI_CC_COALESCING_ENABLED = true;
constexpr bool RTAL_MIDI_IGNORE_DUPLICATE_CC_VALUES = true;


// Freeze Delay – Build 0035
constexpr bool RTAL_DELAY_FREEZE_DEFAULT = false;
constexpr float RTAL_DELAY_FREEZE_SMOOTHING = 0.0025f;
constexpr uint8_t RTAL_MIDI_CC_DELAY_FREEZE = 78;
constexpr uint8_t RTAL_MIDI_SWITCH_OFF_MAX = 47;
constexpr uint8_t RTAL_MIDI_SWITCH_ON_MIN = 80;


// WELLENBAD bidirectional control link – Build 0036
constexpr bool RTAL_FX_LINK_ENABLED = true;
constexpr uint32_t RTAL_FX_LINK_BAUD = 115200;
constexpr int8_t RTAL_FX_LINK_RX_PIN = 40;
constexpr int8_t RTAL_FX_LINK_TX_PIN = 39;
constexpr uint8_t RTAL_FX_PROTOCOL_VERSION = 1;
constexpr uint8_t RTAL_FX_LINK_SERVICE_MAX_BYTES = 48;
constexpr uint32_t RTAL_FX_LINK_TIMEOUT_MS = 3000;


// High-quality Stereo Chorus – Build 0041b Hybrid Hermite/Linear
constexpr bool RTAL_STEREO_CHORUS_ENABLED = true;
constexpr bool RTAL_CHORUS_ENABLE_DEFAULT = false;
constexpr float RTAL_CHORUS_MIX_DEFAULT = 0.35f;
constexpr float RTAL_CHORUS_BASE_DELAY_MS_DEFAULT = 16.0f;
constexpr float RTAL_CHORUS_BASE_DELAY_MS_MIN = 8.0f;
constexpr float RTAL_CHORUS_BASE_DELAY_MS_MAX = 24.0f;
constexpr float RTAL_CHORUS_MAX_MOD_MS = 4.0f;
constexpr float RTAL_CHORUS_TONE_HZ_DEFAULT = 12000.0f;
constexpr float RTAL_CHORUS_TONE_HZ_MIN = 2000.0f;
constexpr float RTAL_CHORUS_TONE_HZ_MAX = 18000.0f;
constexpr float RTAL_CHORUS_PARAMETER_SMOOTHING = 0.0020f;
constexpr float RTAL_CHORUS_TAP_A_GAIN = 0.58f;
constexpr float RTAL_CHORUS_TAP_B_GAIN = 0.42f;

// High-quality Stereo Flanger – Build 0042
constexpr bool RTAL_STEREO_FLANGER_ENABLED = true;
constexpr bool RTAL_FLANGER_ENABLE_DEFAULT = false;
constexpr float RTAL_FLANGER_MIX_DEFAULT = 0.35f;
constexpr float RTAL_FLANGER_DELAY_MS_DEFAULT = 2.5f;
constexpr float RTAL_FLANGER_DELAY_MS_MIN = 0.20f;
constexpr float RTAL_FLANGER_DELAY_MS_MAX = 12.0f;
constexpr float RTAL_FLANGER_MAX_MOD_MS = 2.5f;
constexpr float RTAL_FLANGER_FEEDBACK_DEFAULT = 0.35f;
constexpr float RTAL_FLANGER_FEEDBACK_HPF_HZ_DEFAULT = 80.0f;
constexpr float RTAL_FLANGER_FEEDBACK_HPF_HZ_MIN = 20.0f;
constexpr float RTAL_FLANGER_FEEDBACK_HPF_HZ_MAX = 2000.0f;
constexpr float RTAL_FLANGER_SATURATION_DEFAULT = 0.12f;


// Build0043 - high-quality stereo phaser
constexpr bool RTAL_STEREO_PHASER_ENABLED = true;
constexpr bool RTAL_PHASER_ENABLE_DEFAULT = false;
constexpr float RTAL_PHASER_MIX_DEFAULT = 0.35f;
constexpr uint8_t RTAL_PHASER_STAGES_DEFAULT = 4;
constexpr float RTAL_PHASER_FEEDBACK_DEFAULT = 0.30f;
constexpr float RTAL_PHASER_FEEDBACK_HPF_HZ_DEFAULT = 80.0f;
constexpr float RTAL_PHASER_FEEDBACK_HPF_HZ_MIN = 20.0f;
constexpr float RTAL_PHASER_FEEDBACK_HPF_HZ_MAX = 2000.0f;
constexpr float RTAL_PHASER_SATURATION_DEFAULT = 0.08f;
constexpr float RTAL_PHASER_CENTER_HZ_DEFAULT = 900.0f;
constexpr float RTAL_PHASER_CENTER_HZ_MIN = 200.0f;
constexpr float RTAL_PHASER_CENTER_HZ_MAX = 4000.0f;

// Build0045 - final Mid/Side stereo width stage
constexpr bool RTAL_STEREO_WIDTH_ENABLED = true;
constexpr bool RTAL_WIDTH_ENABLE_DEFAULT = true;
constexpr float RTAL_WIDTH_DEFAULT = 1.0f;
constexpr float RTAL_WIDTH_MIN = 0.0f;
constexpr float RTAL_WIDTH_MAX = 2.0f;


// Build0046 - CPU-conscious high-quality stereo 4-line FDN reverb
constexpr bool RTAL_STEREO_REVERB_ENABLED = true;
constexpr bool RTAL_REVERB_ENABLE_DEFAULT = false;
constexpr float RTAL_REVERB_MIX_DEFAULT = 0.28f;
constexpr float RTAL_REVERB_SIZE_DEFAULT = 0.62f;
constexpr float RTAL_REVERB_DECAY_DEFAULT = 0.58f;
constexpr float RTAL_REVERB_DAMPING_HZ_DEFAULT = 7200.0f;
constexpr float RTAL_REVERB_DAMPING_HZ_MIN = 1200.0f;
constexpr float RTAL_REVERB_DAMPING_HZ_MAX = 16000.0f;
constexpr float RTAL_REVERB_PREDELAY_MS_DEFAULT = 18.0f;
constexpr float RTAL_REVERB_PREDELAY_MS_MAX = 200.0f;
constexpr uint32_t RTAL_REVERB_LINE_BUFFER_SAMPLES = 4096;
