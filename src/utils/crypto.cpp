#include "crypto.h"
#include "../core/offsets.h"
#include <CommonCrypto/CommonDigest.h>
#include <cstring>

static void
ClientInputMessageHashKeyedParts(const uint8_t* key16,
                                 const uint8_t* mask16,
                                 uint8_t        pad,
                                 const uint8_t* message,
                                 size_t         messageLength,
                                 uint8_t        out[CC_SHA256_DIGEST_LENGTH])
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

uint32_t ClientInputMessageComputeChecksum(const uint8_t*  key16,
                                           const uint8_t*  signedData,
                                           const uint32_t* typeTable,
                                           uint32_t        typeFallback,
                                           const uint8_t*  innerMask,
                                           const uint8_t*  outerMask)
{
    constexpr size_t signedSpan =
        static_cast<size_t>(CLIENTINPUTMESSAGE__SIGNEDDATASIZE);
    constexpr size_t typeSpan = sizeof(uint32_t);
    uint32_t         rawType  = 0;
    memcpy(&rawType,
           signedData +
               (CLIENTINPUTMESSAGE__TYPE - CLIENTINPUTMESSAGE__SIGNEDDATA),
           sizeof(rawType));
    uint32_t mappedType =
        rawType < CLIENTINPUT__TYPECOUNT ? typeTable[rawType] : typeFallback;
    uint8_t message[signedSpan + typeSpan];
    memcpy(message, signedData, signedSpan);
    memcpy(message + signedSpan, &mappedType, typeSpan);
    uint8_t inner[CC_SHA256_DIGEST_LENGTH];
    ClientInputMessageHashKeyedParts(
        key16, innerMask, 0x36, message, sizeof(message), inner);
    uint8_t outer[CC_SHA256_DIGEST_LENGTH];
    ClientInputMessageHashKeyedParts(
        key16, outerMask, 0x5c, inner, sizeof(inner), outer);
    uint32_t token = static_cast<uint32_t>(outer[0]) |
                     (static_cast<uint32_t>(outer[1] & 0x7f) << 8);
    return token > 1 ? token : 1;
}
