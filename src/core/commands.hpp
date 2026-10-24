#pragma once

#include <cstdint>

bool sendCommand(int32_t cmdType, void (*fill)(uintptr_t, uintptr_t, void *) = nullptr, void *ctx = nullptr);
