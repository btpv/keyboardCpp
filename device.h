#pragma once
#define MESSAGE_SIZE 32
#include "log.h"
#include <cstdint>
#ifdef DEBUG_MODE
#include <cwchar>
#endif
#include <hidapi/hidapi.h>
#include <string>
enum class MessageType {
  CORRUPT = 0xFF,
  CM_PING = 0x00,
  CM_OPEN,
  CM_CLOSE,
  CM_ACK,
  MSG_TYPE_FN = 0x10,
  MSG_TYPE_EFFECT,
  MSG_TYPE_CMD = 0x20,
  MSG_TYPE_SET_BRIGHTNESS = 0x30,
};
struct DeviceInfo {
  uint16_t vendor_id;
  uint16_t product_id;
};

class device {
public:
  device(uint16_t vendor_id, uint16_t product_id);
  ~device();
  bool open();
  MessageType read(unsigned char buffer[MESSAGE_SIZE]);
  DeviceInfo getDeviceInfo() { return {vid, pid}; }
  bool write(MessageType type, const unsigned char data[MESSAGE_SIZE - 1]);
  bool write(MessageType type) {
    const static unsigned char buffer[MESSAGE_SIZE - 1] = {0};
    return write(type, buffer);
  }
#ifdef DEBUG_MODE
  wchar_t name[256];
#endif

private:
  uint16_t vid;
  uint16_t pid;
  hid_device *hidDevice = nullptr;
  hid_device *getInterface();
  bool isWriteing = false;
};