#pragma once

#include <Preferences.h>

#include <cstdint>

struct DeviceSettings {
  bool wifi_enabled = false;
  bool bluetooth_enabled = false;
  uint8_t brightness_percent = 50;
};

DeviceSettings load_device_settings(Preferences& preferences);

void apply_device_settings(const DeviceSettings& settings);

void set_wifi_enabled(Preferences& preferences, DeviceSettings& settings,
                      bool enabled);

void set_bluetooth_enabled(Preferences& preferences, DeviceSettings& settings,
                           bool enabled);

void set_brightness_percent(Preferences& preferences, DeviceSettings& settings,
                            uint8_t percent);
