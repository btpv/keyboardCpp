#include "brightness.h"
#include "config.h"
#include "device.h"
#include "log.h"
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
extern config_t config;
typedef struct device_data_t {
  device dev;
  std::thread thread;
  std::string commandBuffer = "";
  uint8_t configId = 255;
} device_data_t;
std::vector<device_data_t> deviceData;
bool running = true;
int exitcode = 0;
struct Message {
  DeviceInfo deviceInfo;
  MessageType type;
  unsigned char data[MESSAGE_SIZE - 1];
};
Message relayMessages[32];
int relayMessageIndex = 0;
void updateBrightness(int brightness) {
  unsigned char buffer[MESSAGE_SIZE - 1] = {0};
#ifdef DEBUG_MODE
  log("%s Updating brightness to %d\n", getTime().c_str(), brightness);
#endif
  for (device_data_t &devData : deviceData) {
    buffer[0] = static_cast<unsigned char>(
        getDeviceConfig(devData.configId).brightnessCorrection[brightness]);
#ifdef DEBUG_MODE
    log("%s %s%-35ls%sSetting brightness to %d corrected to %d\n",
        getTime().c_str(),
        getDeviceColor(devData.dev.getDeviceInfo().vendor_id,
                       devData.dev.getDeviceInfo().product_id),
        devData.dev.name, RESET_COLOR, brightness, buffer[0]);
#endif
    devData.dev.write(MessageType::MSG_TYPE_SET_BRIGHTNESS, buffer);
  }
}
void writeToDevices() {
  static bool isRunning = false;
  if (isRunning)
    return;
  isRunning = true;
  while (relayMessageIndex > 0) {
    Message message = relayMessages[--relayMessageIndex];
    for (device_data_t &devData : deviceData) {
      if (devData.dev.getDeviceInfo().vendor_id ==
              message.deviceInfo.vendor_id &&
          devData.dev.getDeviceInfo().product_id ==
              message.deviceInfo.product_id) {
        continue; // Skip the device that sent the message
      }
      unsigned char buffer[MESSAGE_SIZE - 1];
      memcpy(buffer, message.data, MESSAGE_SIZE - 1);
      devData.dev.write(message.type, buffer);
    }
  }
  isRunning = false;
}
void handleCommand(Message message) {
  if (message.type != MessageType::MSG_TYPE_CMD)
    return;

  device_data_t *devicePtr = nullptr;
  for (device_data_t &devData : deviceData) {
    if (devData.dev.getDeviceInfo().vendor_id == message.deviceInfo.vendor_id &&
        devData.dev.getDeviceInfo().product_id ==
            message.deviceInfo.product_id) {
      devicePtr = &devData;
      break;
    }
  }
  if (devicePtr == nullptr) {
    std::cerr << "Unknown device sent a command\n";
    return;
  }
  uint8_t length = 0;
  bool endOfCommand = false;
  for (int i = 0; i < MESSAGE_SIZE - 1; ++i) {
    if (message.data[i] == 0) {
      endOfCommand = true;
      break;
    }
    length++;
  }
  devicePtr->commandBuffer +=
      std::string(reinterpret_cast<char *>(message.data), length);
  if (endOfCommand) {
    endOfCommand = false;
    int result = system(devicePtr->commandBuffer.c_str());
#ifdef DEBUG_MODE
    log("%s %s%-35lssend command: %s%s command exited with code %d\n",
        getTime().c_str(),
        getDeviceColor(message.deviceInfo.vendor_id,
                       message.deviceInfo.product_id),
        devicePtr->dev.name, RESET_COLOR, devicePtr->commandBuffer.c_str(),
        result);
#endif
    devicePtr->commandBuffer.clear();
  }
  // TODO
}
void handleMessage(Message message) {
  if (message.type != MessageType::CORRUPT &&
      message.type >= MessageType::MSG_TYPE_FN) {
    switch (message.type) {
    case MessageType::CORRUPT:
    case MessageType::CM_PING:
    case MessageType::CM_OPEN:
    case MessageType::CM_CLOSE:
    case MessageType::CM_ACK:
    case MessageType::MSG_TYPE_SET_BRIGHTNESS:
      break; // nothing to do for these types
    case MessageType::MSG_TYPE_CMD:
      handleCommand(message);
      break;
    case MessageType::MSG_TYPE_EFFECT:
    case MessageType::MSG_TYPE_FN:
    default:
      if (relayMessageIndex <
          sizeof(relayMessages) / sizeof(relayMessages[0]) - 1) {
        relayMessages[relayMessageIndex++] = message;
        writeToDevices();
      } else {
        std::cerr << "Relay message buffer full, dropping message\n";
      }
      break;
    }
  }
}
void readDevice(device &dev) {
  while (running) {
    unsigned char buffer[MESSAGE_SIZE];
    MessageType type = dev.read(buffer);
    if (type == MessageType::CORRUPT) {
      std::cerr << "Received corrupt message from device\n";
      while (!dev.open()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
      updateBrightness(readBrightness());
      continue;
    }
    Message message;
    message.deviceInfo = dev.getDeviceInfo();
    message.type = type;
    memcpy(message.data, buffer + 1, MESSAGE_SIZE - 1);
    handleMessage(message);
  }
}
int main(int argc, char **argv) {
  const std::string configFilename =
      argc > 1 ? argv[1] : "/etc/keyboardCpp/config.json";
  if (!loadConfig(configFilename)) {
    std::cerr << "Unable to load config file: " << configFilename << '\n';
    return 1;
  }

  deviceData.reserve(config.devices.size());
  for (const device_config_t &deviceConfig : config.devices) {
    deviceData.push_back(
        {.dev = device(deviceConfig.vendor_id, deviceConfig.product_id),
         .configId = deviceConfig.configId});
    device_data_t &deviceDataEntry = deviceData.back();

    if (deviceDataEntry.dev.open()) {
      deviceDataEntry.thread =
          std::thread(readDevice, std::ref(deviceDataEntry.dev));
    }
  }
  std::this_thread::sleep_for(
      std::chrono::milliseconds(100)); // Give devices time to initialize
  std::thread brightnessThread([] { watchBrightness(updateBrightness); });

  for (device_data_t &deviceDataEntry : deviceData) {
    if (deviceDataEntry.thread.joinable()) {
      deviceDataEntry.thread.join();
    }
  }
  if (brightnessThread.joinable()) {
    brightnessThread.join();
  }
  return exitcode;
}