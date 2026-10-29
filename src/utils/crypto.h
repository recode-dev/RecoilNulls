#ifndef UTILS_CRYPTO_H
#define UTILS_CRYPTO_H
#include <cstdint>
uint32_t ci_compute_token(const uint8_t* key16,
                          const uint8_t* cmd16,
                          const uint32_t* table,
                          uint32_t fallback,
                          const uint8_t* innerMask,
                          const uint8_t* outerMask);
#endif
