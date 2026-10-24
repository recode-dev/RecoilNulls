#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>

size_t readScStringInto(uintptr_t value, char *out, size_t capacity);
std::optional<std::string> readScString(uintptr_t value, size_t maxLength = 256);
