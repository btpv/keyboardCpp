#pragma once
#ifdef DEBUG_MODE
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <cstdio>
#include "device.h" 
#define RESET_COLOR "\033[0m"
#define RECEIVE_COLOR "\033[95m"
#define SEND_COLOR "\033[96m"
#define KEYBOARD_COLOR "\033[93m"
#define MACROPAD_COLOR "\033[91m"

struct LogDevice {
  uint16_t vendor_id;
  uint16_t product_id;
  const char *color;
};
const LogDevice logDevices[] = {
    {0x32AC, 0x0012, KEYBOARD_COLOR}, // Keyboard
    {0x32AC, 0x0013, MACROPAD_COLOR}, // Macropad
};
inline const char *getDeviceColor(uint16_t vendor_id, uint16_t product_id) {
  for (const LogDevice &device : logDevices) {
    if (device.vendor_id == vendor_id && device.product_id == product_id) {
      return device.color;
    }
  }
  return RESET_COLOR; // Default color if not found
}
inline std::string getTime() {
  std::time_t now = std::time(nullptr);
  std::tm localTime = *std::localtime(&now);

  std::stringstream ss;
  ss << std::put_time(&localTime, "%H:%M:%S");

  return ss.str();
}
inline std::string convertToHexString(const unsigned char *buffer) {
  char data[MESSAGE_SIZE * 3] = {0};
  const char hex[] = "0123456789ABCDEF";
  for (int i = 0; i < MESSAGE_SIZE; ++i) {

    data[i * 3 + 0] = hex[buffer[i] >> 4 & 0xF];
    data[i * 3 + 1] = hex[buffer[i] >> 0 & 0xF];
    data[i * 3 + 2] = ' ';
  }
  data[MESSAGE_SIZE * 3 - 1] = '\0'; // Null-terminate the string
  return std::string(data);
}

inline void log(const char *format, ...) {
  static bool isprinting = false;
  while (isprinting)
    ;
  isprinting = true;
  va_list args;
  va_start(args, format);

  vprintf(format, args);

  va_end(args);
  isprinting = false;
}
#endif
