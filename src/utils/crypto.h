#ifndef UTILS_CRYPTO_H
#define UTILS_CRYPTO_H
#include <cstdint>
uint32_t ClientInputMessageComputeChecksum(const uint8_t*  key16,
                                           const uint8_t*  signedData,
                                           const uint32_t* typeTable,
                                           uint32_t        typeFallback,
                                           const uint8_t*  innerMask,
                                           const uint8_t*  outerMask);
#endif
