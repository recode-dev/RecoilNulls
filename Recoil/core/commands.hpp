#pragma once

#include <cstdint>
#include <functional>

bool sendCommand(int32_t cmdType, const std::function<void(uintptr_t, uintptr_t)> &fill = {});
