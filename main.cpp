#include "brightness.h"
#include "device.h"
#include "log.h"
#include <cstring>
#include <iostream>
#include <thread>
#define DEVICE_COUNT sizeof(devices) / sizeof(devices[0])
device devices[] = {device(0x32AC, 0x0012), device(0x32AC, 0x0013)};
std::thread threads[DEVICE_COUNT];
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
  buffer[0] = static_cast<unsigned char>(brightness);
  for (device &dev : devices) {
    dev.write(MessageType::MSG_TYPE_SET_BRIGHTNESS, buffer);
  }
}
void writeToDevices() {
  static bool isRunning = false;
  if (isRunning)
    return;
  isRunning = true;
  while (relayMessageIndex > 0) {
    Message message = relayMessages[--relayMessageIndex];
    for (device &dev : devices) {
      if (dev.getDeviceInfo().vendor_id == message.deviceInfo.vendor_id &&
          dev.getDeviceInfo().product_id == message.deviceInfo.product_id) {
        continue; // Skip the device that sent the message
      }
      unsigned char buffer[MESSAGE_SIZE - 1];
      memcpy(buffer, message.data, MESSAGE_SIZE - 1);
      dev.write(message.type, buffer);
    }
  }
  isRunning = false;
}
void handleCommand(Message message) {
  if (message.type != MessageType::MSG_TYPE_CMD)
    return;
  static std::string commandBuffer[DEVICE_COUNT] = {""};
  int deviceIndex = -1;
  for (int i = 0; i < DEVICE_COUNT; ++i) {
    if (devices[i].getDeviceInfo().vendor_id == message.deviceInfo.vendor_id &&
        devices[i].getDeviceInfo().product_id ==
            message.deviceInfo.product_id) {
      deviceIndex = i;
      break;
    }
  }
  if (deviceIndex == -1) {
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
  commandBuffer[deviceIndex] +=
      std::string(reinterpret_cast<char *>(message.data), length);
  if (endOfCommand) {
    endOfCommand = false;
    int result = system(commandBuffer[deviceIndex].c_str());
#ifdef DEBUG_MODE
    log("%s %s%-35lssend command: %s%s command exited with code %d\n",
        getTime().c_str(),
        getDeviceColor(message.deviceInfo.vendor_id,
                       message.deviceInfo.product_id),
        devices[deviceIndex].name, RESET_COLOR,
        commandBuffer[deviceIndex].c_str(), result);
#endif
    commandBuffer[deviceIndex].clear();
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
int main() {
  for (device &dev : devices) {
    if (dev.open()) {
      threads[&dev - devices] = std::thread(readDevice, std::ref(dev));
    }
  }
  std::this_thread::sleep_for(
      std::chrono::milliseconds(100)); // Give devices time to initialize
  std::thread brightnessThread([] {
    watchBrightness(updateBrightness);
  });

  for (std::thread &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  // brightnessThread.join();
  return exitcode;
}