#include <M5Unified.h>
#include <M5Utility.h>

#include "KeyboardUtils.h"

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  bool unit_ready{};
  unit_ready = setup_keyboard_i2c();
  if (!unit_ready) {
    while (true) {
      m5::utility::delay(10000);
    }
  }
}

void loop() { M5.update(); }
