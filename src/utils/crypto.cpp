#include "crypto.h"
#include "../core/offsets.h"
#include <CommonCrypto/CommonDigest.h>
#include <cstring>

static void hash_keyed_parts(const uint8_t* key16,
                             const uint8_t* mask16,
                             uint8_t pad,
                             const uint8_t* message,
                             size_t messageLength,
                             uint8_t out[CC_SHA256_DIGEST_LENGTH])
{
    uint8_t keyedBlock[64];
    for (int i = 0; i < 16; ++i)
    {
        keyedBlock[i] = static_cast<uint8_t>(key16[i] ^ mask16[i]);
    }
    memset(keyedBlock + 16, pad, sizeof(keyedBlock) - 16);
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    CC_SHA256_Update(&context, keyedBlock, sizeof(keyedBlock));
    CC_SHA256_Update(&context, message, static_cast<CC_LONG>(messageLength));
    CC_SHA256_Final(out, &context);
}

uint32_t ci_compute_token(const uint8_t* key16,
                          const uint8_t* cmd16,
                          const uint32_t* table,
                          uint32_t fallback,
                          const uint8_t* innerMask,
                          const uint8_t* outerMask)
{
    uint32_t rawType = 0;
    memcpy(&rawType, cmd16 + 4, sizeof(rawType));
    uint32_t mappedType =
        rawType < CLIENT_INPUT_TYPE_COUNT ? table[rawType] : fallback;
    uint8_t message[20];
    memcpy(message, cmd16, 16);
    memcpy(message + 16, &mappedType, sizeof(mappedType));
    uint8_t inner[CC_SHA256_DIGEST_LENGTH];
    hash_keyed_parts(key16, innerMask, 0x36, message, sizeof(message), inner);
    uint8_t outer[CC_SHA256_DIGEST_LENGTH];
    hash_keyed_parts(key16, outerMask, 0x5c, inner, sizeof(inner), outer);
    uint32_t token = static_cast<uint32_t>(outer[0]) |
                     (static_cast<uint32_t>(outer[1] & 0x7f) << 8);
    return token > 1 ? token : 1;
}
