#pragma once
#include <Arduino.h>

// RTAL UI Engine 1.0 - shared 128x64 OLED vertical grid.
// Baseline coordinates for u8g2_font_5x8_tf.
namespace RTALUILayout {
  constexpr uint8_t STATUS_BASELINE = 7;
  constexpr uint8_t STATUS_DIVIDER_Y = 9;
  constexpr uint8_t TITLE_BASELINE = 17;

  // Four evenly spaced text rows for Performance and System screens.
  constexpr uint8_t ROW1_BASELINE = 25;
  constexpr uint8_t ROW2_BASELINE = 33;
  constexpr uint8_t ROW3_BASELINE = 41;
  constexpr uint8_t ROW4_BASELINE = 49;

  // Fixed text columns for the three UI levels. Values are left-aligned so
  // every row forms a stable, quickly readable two-column grid.
  constexpr uint8_t PERFORMANCE_LABEL_X = 5;
  constexpr uint8_t PERFORMANCE_VALUE_X = 43;

  constexpr uint8_t SYSTEM_CURSOR_X = 0;
  constexpr uint8_t SYSTEM_LABEL_X = 7;
  constexpr uint8_t SYSTEM_VALUE_X = 65;
  constexpr uint8_t SYSTEM_VALUE_MAX_CHARS = 12;

  constexpr uint8_t FOOTER_DIVIDER_Y = 50;
  constexpr uint8_t FOOTER_TEXT_Y = 53;
  constexpr uint8_t FOOTER_UNDERLINE_Y = 62;
}
