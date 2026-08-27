#include "KeyboardUtils.h"

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedKEYBOARD.h>
#include <M5Utility.h>

#include <M5HAL.hpp>

m5::unit::UnitUnified unit;
m5::unit::UnitCardKB2UART keyboard;

const char* special_key_name(char ch) {
  switch (ch) {
    case '\b':
      return "BS";
    case '\t':
      return "TAB";
    case '\n':
      return "LF";
    case '\r':
      return "CR";
    case 0x1B:
      return "ESC";
    case 0x7F:
      return "DEL";
    case m5::unit::cardkb2::SCHAR_LEFT:
      return "LEFT";
    case m5::unit::cardkb2::SCHAR_UP:
      return "UP";
    case m5::unit::cardkb2::SCHAR_DOWN:
      return "DOWN";
    case m5::unit::cardkb2::SCHAR_RIGHT:
      return "RIGHT";
    default:
      break;
  }

  return nullptr;
}

bool setup_uart() {
  const auto pin_num_rx = M5.getPin(m5::pin_name_t::port_a_pin1);
  const auto pin_num_tx = M5.getPin(m5::pin_name_t::port_a_pin2);
  if (pin_num_rx < 0 || pin_num_tx < 0) {
    return false;
  }

  Wire.end();
  Serial2.begin(115200, SERIAL_8N1, pin_num_rx, pin_num_tx);

  auto config = keyboard.config();
  config.interval = 1;
  keyboard.config(config);

  return unit.add(keyboard, Serial2) && unit.begin();
}

bool setup_keyboard_uart() { return setup_uart(); }

void update_unit() { unit.update(); }

bool is_keyboard_updated() { return keyboard.updated(); }

bool is_keyboard_available() { return keyboard.available(); }

bool read_key(char& key) {
  if (!keyboard.available()) {
    return false;
  }

  key = keyboard.getchar();
  keyboard.discard();
  return true;
}

bool was_escape_shortcut_pressed() {
  return keyboard.wasPressed(m5::unit::cardkb2::KEY_1) &&
         keyboard.isFunction();
}

bool is_cursor_left_key(char key) {
  return key == m5::unit::cardkb2::SCHAR_LEFT;
}

bool is_cursor_right_key(char key) {
  return key == m5::unit::cardkb2::SCHAR_RIGHT;
}

bool is_cursor_up_key(char key) {
  return key == m5::unit::cardkb2::SCHAR_UP;
}

bool is_cursor_down_key(char key) {
  return key == m5::unit::cardkb2::SCHAR_DOWN;
}

bool is_escape_key(char key) { return key == 0x1B; }
