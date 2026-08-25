#include "KeyboardUtils.h"

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedKEYBOARD.h>
#include <M5Utility.h>

#include <M5HAL.hpp>

m5::unit::UnitUnified unit;
m5::unit::UnitCardKB2 keyboard;

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

bool setup_i2c() {
  auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
  auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
  Wire.end();
  Wire.begin(pin_num_sda, pin_num_scl, 100 * 1000U);

  return unit.add(keyboard, Wire) && unit.begin();
}

bool setup_keyboard_i2c() {
  if (!setup_i2c()) {
    return false;
  }

  return true;
}

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
