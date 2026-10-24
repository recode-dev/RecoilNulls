#include <cstdint>
#include <cstddef>
#include <cstring>
#include <frida-gum.h>

#include "offsets.hpp"
#include "functions.hpp"
#include "commands.hpp"
#include "../utils/crypto.hpp"

namespace
{
    constexpr size_t CMD_BUF_SIZE = 72;
    constexpr uint32_t MAX_TABLE_TYPE = 0x16;
    constexpr size_t TABLE_ENTRIES = MAX_TABLE_TYPE + 1;
    constexpr size_t MASK_SIZE = 16;
    constexpr uintptr_t CLIENT_INPUT_TOKEN = 0x34;

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        return gum_memory_read((GumAddress)address, out, (gsize)size);
    }

    bool readU32(uintptr_t address, uint32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readPtr(uintptr_t address, uintptr_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    uint32_t g_typeTable[TABLE_ENTRIES] = {};
    uint32_t g_fallbackType = 0;
    uint8_t g_innerMask[MASK_SIZE] = {};
    uint8_t g_outerMask[MASK_SIZE] = {};
    bool g_loaded = false;

    bool loadSigningConstants()
    {
        if (g_loaded) return true;
        const uintptr_t base = functions::base();
        if (base == 0) return false;

        uint8_t raw[TABLE_ENTRIES * 4];
        if (!readBytes(base + offsets::ClientInput_typeConstantTable, raw, sizeof(raw))) return false;
        for (size_t i = 0; i < TABLE_ENTRIES; i++)
        {
            g_typeTable[i] = (uint32_t)raw[i * 4] |
                ((uint32_t)raw[i * 4 + 1] << 8) |
                ((uint32_t)raw[i * 4 + 2] << 16) |
                ((uint32_t)raw[i * 4 + 3] << 24);
        }

        if (!readBytes(base + offsets::ClientInput_hashInnerMask, g_innerMask, MASK_SIZE)) return false;
        if (!readBytes(base + offsets::ClientInput_hashOuterMask, g_outerMask, MASK_SIZE)) return false;
        if (!readU32(base + offsets::ClientInput_typeFallback, g_fallbackType)) return false;

        g_loaded = true;
        return true;
    }

    void makeHashBlock(uint8_t block[64], const uint8_t key16[16], const uint8_t mask[MASK_SIZE], uint8_t padByte)
    {
        for (size_t i = 0; i < 16; i++) block[i] = key16[i] ^ mask[i];
        memset(block + 16, padByte, 64 - 16);
    }

    uint32_t computeToken(const uint8_t key16[16], const uint8_t cmdData16[16])
    {
        const uint32_t typeRaw = (uint32_t)cmdData16[4] |
            ((uint32_t)cmdData16[5] << 8) |
            ((uint32_t)cmdData16[6] << 16) |
            ((uint32_t)cmdData16[7] << 24);
        const uint32_t typeC = typeRaw <= MAX_TABLE_TYPE ? g_typeTable[typeRaw] : g_fallbackType;

        uint8_t msg[20];
        memcpy(msg, cmdData16, 16);
        msg[16] = (uint8_t)(typeC & 0xff);
        msg[17] = (uint8_t)((typeC >> 8) & 0xff);
        msg[18] = (uint8_t)((typeC >> 16) & 0xff);
        msg[19] = (uint8_t)((typeC >> 24) & 0xff);

        uint8_t hashBlock[64];
        uint8_t seed[84];
        uint8_t inner[32];
        makeHashBlock(hashBlock, key16, g_innerMask, 0x36);
        memcpy(seed, hashBlock, 64);
        memcpy(seed + 64, msg, 20);
        crypto::sha256(seed, sizeof(seed), inner);

        uint8_t outer[96];
        uint8_t digest[32];
        makeHashBlock(hashBlock, key16, g_outerMask, 0x5c);
        memcpy(outer, hashBlock, 64);
        memcpy(outer + 64, inner, 32);
        crypto::sha256(outer, sizeof(outer), digest);

        uint32_t token = (uint32_t)digest[0] | (((uint32_t)digest[1] & 0x7f) << 8);
        if (token <= 1) token = 1;
        return token;
    }

    uint32_t signClientInput(uintptr_t ci, uintptr_t battle)
    {
        if (ci == 0 || battle == 0) return 0;

        uint8_t state[MASK_SIZE + 1];
        if (!readBytes(battle + offsets::BattleMode_hashKey, state, sizeof(state))) return 0;
        if (state[MASK_SIZE] == 0) return 0;
        if (!loadSigningConstants()) return 0;

        uint8_t cmdData16[16];
        memcpy(cmdData16, (const void *)(ci + 4), sizeof(cmdData16));

        const uint32_t token = computeToken(state, cmdData16);
        memcpy((void *)(ci + CLIENT_INPUT_TOKEN), &token, sizeof(token));
        return token;
    }
}

bool sendCommand(int32_t cmdType, void (*fill)(uintptr_t, uintptr_t, void *), void *ctx)
{
    const uintptr_t battle = functions::BattleMode_getInstance();
    if (battle == 0) return false;

    uintptr_t manager = 0;
    if (!readPtr(battle + offsets::BattleMode_clientInputManager, manager)) return false;
    if (manager == 0) return false;

    const uintptr_t ci = functions::operator_new(CMD_BUF_SIZE);
    if (ci == 0) return false;

    memset((void *)ci, 0, CMD_BUF_SIZE);
    functions::ClientInput_constructor_int(ci, cmdType);
    if (fill != nullptr) fill(ci, battle, ctx);
    signClientInput(ci, battle);
    functions::ClientInputManager_addInput(manager, ci);
    return true;
}
