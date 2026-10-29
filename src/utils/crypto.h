#ifndef UTILS_CRYPTO_H
#define UTILS_CRYPTO_H
#include <stddef.h>
#include <stdint.h>
void     sha(const uint8_t* data, size_t length, uint8_t out[32]);
void     ci_make_block(const uint8_t key16[16],
                       const uint8_t mask16[16],
                       uint8_t       pad,
                       uint8_t       out[64]);
uint32_t ci_compute_token(const uint8_t   key16[16],
                          const uint8_t   cmd16[16],
                          const uint32_t* table,
                          uint32_t        fallback,
                          const uint8_t   innerMask[16],
                          const uint8_t   outerMask[16]);
#endif
