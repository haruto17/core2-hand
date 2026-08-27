#include "DeviceSettings.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <esp32-hal-bt.h>

namespace {

constexpr char kWifiKey[] = "wifi";
constexpr char kBluetoothKey[] = "bluetooth";
constexpr char kBrightnessKey[] = "brightness";
constexpr uint8_t kDefaultBrightnessPercent = 50;
constexpr uint8_t kMinimumBrightnessPercent = 10;

uint8_t normalize_brightness(uint8_t percent) {
  if (percent < kMinimumBrightnessPercent || percent > 100) {
    return kDefaultBrightnessPercent;
  }
  return percent;
}

void apply_wifi(bool enabled) {
  WiFi.mode(enabled ? WIFI_STA : WIFI_OFF);
}

void apply_bluetooth(bool enabled) {
  if (enabled) {
    if (!btStarted()) {
      btStart();
    }
  } else if (btStarted()) {
    btStop();
  }
}

void apply_brightness(uint8_t percent) {
  const uint8_t brightness = static_cast<uint8_t>(
      (static_cast<uint16_t>(percent) * 255U) / 100U);
  M5.Display.setBrightness(brightness);
}

}  // namespace

DeviceSettings load_device_settings(Preferences& preferences) {
  DeviceSettings settings;
  settings.wifi_enabled = preferences.getBool(kWifiKey, false);
  settings.bluetooth_enabled = preferences.getBool(kBluetoothKey, false);
  settings.brightness_percent = normalize_brightness(
      preferences.getUChar(kBrightnessKey, kDefaultBrightnessPercent));
  return settings;
}

void apply_device_settings(const DeviceSettings& settings) {
  apply_wifi(settings.wifi_enabled);
  apply_bluetooth(settings.bluetooth_enabled);
  apply_brightness(settings.brightness_percent);
}

void set_wifi_enabled(Preferences& preferences, DeviceSettings& settings,
                      bool enabled) {
  settings.wifi_enabled = enabled;
  preferences.putBool(kWifiKey, enabled);
  apply_wifi(enabled);
}

void set_bluetooth_enabled(Preferences& preferences, DeviceSettings& settings,
                           bool enabled) {
  settings.bluetooth_enabled = enabled;
  preferences.putBool(kBluetoothKey, enabled);
  apply_bluetooth(enabled);
}

void set_brightness_percent(Preferences& preferences, DeviceSettings& settings,
                            uint8_t percent) {
  settings.brightness_percent = normalize_brightness(percent);
  preferences.putUChar(kBrightnessKey, settings.brightness_percent);
  apply_brightness(settings.brightness_percent);
}
