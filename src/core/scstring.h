#ifndef CORE_SCSTRING_H
#define CORE_SCSTRING_H
#include <cstddef>
#include <cstdint>
inline constexpr int SCSTRING_DEFAULT_MAX_LENGTH = 256;
int read_scstring(uintptr_t value, size_t maxLength, char* out, size_t outSize);
#endif
