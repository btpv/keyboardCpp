#include "device.h"
#include <cstring>
#include <hidapi/hidapi.h>
#include <iostream>
device::device(uint16_t vendor_id, uint16_t product_id)
    : vid(vendor_id), pid(product_id) {
  hid_init();
  // Constructor implementation
}
hid_device *device::getInterface() {
  hid_device_info *devices = hid_enumerate(vid, pid);
  hid_device *device = nullptr;
  for (auto *d = devices; d; d = d->next) {
#ifdef DEBUG_MODE
    log("Device Found: %ls\n", d->product_string);
#endif
    if (d->usage_page == 0xFF60 && d->usage == 0x61) {
      device = hid_open_path(d->path);
#ifdef DEBUG_MODE
      std::wcscpy(name, d->product_string ? d->product_string : L"Unknown");
#endif
      break;
    }
  }
  hid_free_enumeration(devices);

  if (!device) {
    std::cerr << "Device not found\n";
  }

  return device;
}
bool device::open() {
  hidDevice = getInterface();
  if (!hidDevice) {
    std::cerr << "Failed to open device\n";
    return false;
  }
  if (!write(MessageType::CM_OPEN)) {
    std::cerr << "Failed to send open command\n";
    return false;
  }
  return true;
}

bool device::write(MessageType type,
                   const unsigned char data[MESSAGE_SIZE - 1]) {
  if (!hidDevice) {
    std::cerr << "Device not opened\n";
    return false;
  }
  unsigned char buffer[MESSAGE_SIZE];
  buffer[0] = static_cast<unsigned char>(type);
  memcpy(buffer + 1, data, MESSAGE_SIZE - 1);
#ifdef DEBUG_MODE
  log("%s %s%-35lss: %s%s%s\n", getTime().c_str(), getDeviceColor(vid, pid),
      name, SEND_COLOR, convertToHexString(buffer).c_str(), RESET_COLOR);
#endif
  while (isWriteing)
    ;
  isWriteing = true;
  int res = hid_write(hidDevice, buffer, sizeof(buffer));
  isWriteing = false;
  if (res < 0) {
    std::cerr << "Failed to write to device\n";
    return false;
  }
  return true;
}
MessageType device::read(unsigned char buffer[MESSAGE_SIZE]) {
  if (!hidDevice) {
    std::cerr << "Device not opened\n";
    return MessageType::CORRUPT;
  }
  int res = hid_read(hidDevice, buffer, MESSAGE_SIZE);
#ifdef DEBUG_MODE
  log("%s %s%-35lsr: %s%s%s\n", getTime().c_str(), getDeviceColor(vid, pid),
      name, RECEIVE_COLOR, convertToHexString(buffer).c_str(), RESET_COLOR);
#endif
  if (res < 0) {
    std::cerr << "Failed to read from device\n";
    return MessageType::CORRUPT;
  }
  if (res != MESSAGE_SIZE) {
    return MessageType::CORRUPT;
  }
  MessageType type = static_cast<MessageType>(buffer[0]);
  if (type != MessageType::CM_ACK) {
    unsigned char data[MESSAGE_SIZE - 1] = {0};
    data[0] = buffer[0];
    write(MessageType::CM_ACK, data);
  }
  return type;
}
device::~device() {
  if (hidDevice) {
    write(MessageType::CM_CLOSE);
    hid_close(hidDevice);
  }
  hid_exit();
}
