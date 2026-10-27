#ifndef CORE_SCSTRING_H
#define CORE_SCSTRING_H
#include <stddef.h>
#include <stdint.h>
#define SCSTRING_DEFAULT_MAX_LENGTH 256
// Reads a game ScString at `value` into `out` (NUL-terminated).
// Returns 0 when the string is null, empty, longer than `maxLength`,
// or does not fit into `outSize`.
int read_scstring(uintptr_t value, size_t maxLength, char* out, size_t outSize);
#endif
