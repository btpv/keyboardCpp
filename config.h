#pragma once
#include <cstdint>
#include <string>
#include <vector>
typedef struct device_config_t {
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t brightnessCorrection[256]={127};
  uint8_t configId = 255;
} device_config_t;
typedef struct config_t {
  std::vector<device_config_t> devices = {};
} config_t;

extern config_t config;

bool loadConfig(const std::string &filename);
inline const device_config_t getDeviceConfig(uint8_t deviceConfigId) {
  if (deviceConfigId < config.devices.size()) {
    return config.devices[deviceConfigId];
  }
  else {
    static const device_config_t defaultConfig = {};
    return defaultConfig;
  }
} 