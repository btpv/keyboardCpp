#pragma once

#include <functional>

int readBrightness();
void watchBrightness(std::function<void(int)> callback);
