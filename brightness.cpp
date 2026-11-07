#include "brightness.h"
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <poll.h>
#include <string>
#include <unistd.h>
extern bool running;
namespace {
const std::filesystem::path backlightDirectory = "/sys/class/backlight";

std::optional<std::filesystem::path> findBacklightDevice() {
  std::error_code error;
  if (!std::filesystem::is_directory(backlightDirectory, error)) {
    return std::nullopt;
  }

  for (const auto &entry :
       std::filesystem::directory_iterator(backlightDirectory, error)) {
    const auto &path = entry.path();
    if (std::filesystem::exists(path / "brightness") &&
        std::filesystem::exists(path / "max_brightness") &&
        std::filesystem::exists(path / "actual_brightness")) {
      return path;
    }
  }

  return std::nullopt;
}
}

int readBrightness() {
  const auto devicePath = findBacklightDevice();
  if (!devicePath) {
    return -1;
  }

  int brightness = 0;
  int maxBrightness = 0;

  if (!(std::ifstream(*devicePath / "brightness") >> brightness) ||
      !(std::ifstream(*devicePath / "max_brightness") >> maxBrightness) ||
      maxBrightness <= 0) {
    return -1;
  }

  return brightness * 255 / maxBrightness;
}

void watchBrightness(std::function<void(int)> callback) {
  const auto devicePath = findBacklightDevice();
  if (!devicePath) {
    std::cerr << "No backlight device found\n";
    return;
  }

  const std::string actualBrightnessPath =
      (*devicePath / "actual_brightness").string();
  int fd = open(actualBrightnessPath.c_str(), O_RDONLY);

  if (fd < 0) {
    perror("open actual_brightness");
    return;
  }

  char buffer[32];

  read(fd, buffer, sizeof(buffer));

  int lastBrightness = readBrightness();
  if (lastBrightness >= 0) {
    callback(lastBrightness);
  }

  pollfd pfd{.fd = fd, .events = POLLPRI | POLLERR};

  while (running) {
    int result = poll(&pfd, 1, -1);

    if (result < 0) {
      perror("poll");
      break;
    }

    if (pfd.revents & (POLLPRI | POLLERR)) {
      lseek(fd, 0, SEEK_SET);
      read(fd, buffer, sizeof(buffer));

      const int brightness = readBrightness();
      if (brightness >= 0 && brightness != lastBrightness) {
        lastBrightness = brightness;
        callback(brightness);
      }
    }
  }

  close(fd);
}