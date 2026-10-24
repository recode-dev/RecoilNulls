#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>

std::optional<std::string> readScString(uintptr_t value, size_t maxLength = 256);
