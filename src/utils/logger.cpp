#include "../core/scan.h"
#include "../core/scstring.h"
#include "../core/commands.h"
#include "../features/autododge.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace
{
constexpr long long LOG_MAX_BYTES = 4000000;
constexpr uint64_t  REPORT_US     = 1000000ULL;
constexpr uint64_t  FAST_TRACE_US = 50000ULL;
constexpr uint64_t  IDLE_TRACE_US = 500000ULL;

static const char* target_text(char* out, size_t cap)
{
    if (ad_target_x == INT32_MAX || ad_target_y == INT32_MAX)
    {
        snprintf(out, cap, "-");
        return out;
    }
    snprintf(out, cap, "%d,%d", (int) ad_target_x, (int) ad_target_y);
    return out;
}

char     log_path[1100] = {0};
FILE*    log_file       = nullptr;
uint64_t last_report_us = 0;
int32_t  last_hp        = -1;
float    last_tx        = 1.0f;
float    last_ty        = 0.0f;
int      dodge_count    = 0;
int      hit_count      = 0;
int      match_open     = 0;
int      idle_logged    = 0;

void document_path(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    out[0]           = 0;
    if (home != nullptr) snprintf(out, cap, "%s/Documents", home);
}

void line(const char* format, ...)
{
    if (log_file == nullptr) return;
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);
}

void rotate_if_needed()
{
    FILE* probe = fopen(log_path, "rb");
    if (probe == nullptr) return;
    fseek(probe, 0, SEEK_END);
    long long size = ftell(probe);
    fclose(probe);
    if (size <= LOG_MAX_BYTES) return;
    char backup[1120];
    snprintf(backup, sizeof(backup), "%s.1", log_path);
    rename(log_path, backup);
}

bool open_log()
{
    char dir[1024];
    if (log_file != nullptr) return true;
    document_path(dir, sizeof(dir));
    if (dir[0] == 0) return false;
    snprintf(log_path, sizeof(log_path), "%s/Recoil.log", dir);
    log_file = fopen(log_path, "a");
    if (log_file == nullptr) return false;
    fseek(log_file, 0, SEEK_END);
    if (ftell(log_file) == 0)
    {
        fprintf(log_file, "=== autododge log ===\n");
        fprintf(log_file, "documents: %s\n", dir);
        fflush(log_file);
    }
    return true;
}

int       val_ok          = 0;
int       val_fail        = 0;
int       val_skip        = 0;
int       val_stage       = 0;
int       val_proj_done   = 0;
int       val_proj_tries  = 0;
int       val_input_state = 0;
int       val_input_tries = 0;
uintptr_t val_ci_manager  = 0;
uintptr_t val_ci_queue    = 0;
int32_t   val_ci_count    = 0;
uint64_t  val_probe_next  = 0;

void vf(const char* name, bool pass, const char* format, ...)
{
    char    buf[192];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (pass)
    {
        val_ok++;
        line("[VAL] %-32s ok    %s\n", name, buf);
    }
    else
    {
        val_fail++;
        line("[VAL] %-32s FAIL  %s\n", name, buf);
    }
}

void vs(const char* name, const char* why)
{
    val_skip++;
    line("[VAL] %-32s skip  %s\n", name, why);
}

int val_client_input(uintptr_t clientPtr);

BOOL span(long long value)
{
    return (value >= -3000 && value <= 40000) ? YES : NO;
}

void val_rva_table()
{
    static const struct
    {
        const char* name;
        uint64_t    rva;
    } table[] = {
        {"BattleModeGetInstanceRva", BattleModeGetInstanceRva},
        {"LogicBattleModeClientGetTileMapRva",
         LogicBattleModeClientGetTileMapRva},
        {"LogicBattleModeClientGetOwnCharacterRva",
         LogicBattleModeClientGetOwnCharacterRva},
        {"LogicBattleModeClientGetOwnPlayerIndexRva",
         LogicBattleModeClientGetOwnPlayerIndexRva},
        {"LogicBattleModeClientGetOwnPlayerTeamRva",
         LogicBattleModeClientGetOwnPlayerTeamRva},
        {"LogicDataGetNameRva", LogicDataGetNameRva},
        {"LogicBattleModeClientSetClientPredictionMoveToRva",
         LogicBattleModeClientSetClientPredictionMoveToRva},
        {"ClientInputMessageConstructorRva", ClientInputMessageConstructorRva},
        {"ClientInputManagerAddInputRva", ClientInputManagerAddInputRva},
        {"LogicTileMapIsPassablePathFinderRva",
         LogicTileMapIsPassablePathFinderRva},
        {"LogicTileMapIsPlayerLineOfSightClearRva",
         LogicTileMapIsPlayerLineOfSightClearRva},
        {"LogicTileMapTileToLogicRva", LogicTileMapTileToLogicRva},
        {"LogicTileMapLogicToTileRva", LogicTileMapLogicToTileRva},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
    {
        uintptr_t address = callable(table[i].rva);
        uint32_t  head    = 0;
        BOOL      exec    = NO;
        BOOL      read    = NO;
        if (address)
        {
            exec = addr_executable(address);
            read = read_bytes(address, &head, sizeof(head));
        }
        vf(table[i].name,
           address && exec && read && head != 0,
           "rva=0x%llx addr=0x%llx exec=%d head=%08x",
           (unsigned long long) table[i].rva,
           (unsigned long long) address,
           exec ? 1 : 0,
           head);
    }
    {
        int center = game_tile_to_logic(3);
        vf("LogicTileMapTileToLogicRva",
           center == 3 * 300 + 150 && game_logic_to_tile(center) == 3,
           "3 -> %d (want 1050) back=%d",
           center,
           game_logic_to_tile(center));
    }
    vf("LogicTileMapLogicToTileRva",
       game_logic_to_tile(900) == 3,
       "900 -> %d (want 3)",
       game_logic_to_tile(900));
}

void val_globals(uintptr_t slot)
{
    uintptr_t scene                    = 0;
    uintptr_t clientPtr                = 0;
    uintptr_t get_client_input_manager = 0;
    uintptr_t array                    = 0;
    uintptr_t vtable                   = 0;
    int32_t   state                    = -1;
    int32_t   count                    = 0;
    uint8_t   hashOn                   = 0;
    uint8_t   key[16]                  = {0};
    vf("GameStateManagerRva",
       slot && pointer_plausible(slot),
       "0x%llx",
       (unsigned long long) slot);
    if (!slot)
    {
        vs("GameStateManagerStateOffset", "no game state");
        vs("GameStateManagerCurrentStateOffset", "no game state");
        return;
    }
    if (read_int(slot + GameStateManagerStateOffset, &state))
    {
        vf("GameStateManagerStateOffset",
           state >= 0 && state <= 8,
           "state=%d",
           state);
    }
    else
    {
        vf("GameStateManagerStateOffset", false, "read failed");
    }
    if (read_ptr(slot + GameStateManagerCurrentStateOffset, &scene))
    {
        vf("GameStateManagerCurrentStateOffset",
           scene && pointer_plausible(scene),
           "0x%llx",
           (unsigned long long) scene);
    }
    else
    {
        vf("GameStateManagerCurrentStateOffset", false, "read failed");
    }
    if (!scene)
    {
        vs("GameStateClientOffset", "no scene");
        vs("LogicBattleModeClientGameObjectManagerOffset", "no scene");
        vs("LogicBattleModeClientInputManagerOffset", "no scene");
    }
    else
    {
        if (read_ptr(scene + GameStateClientOffset, &clientPtr))
        {
            vf("GameStateClientOffset",
               clientPtr && pointer_plausible(clientPtr),
               "0x%llx",
               (unsigned long long) clientPtr);
        }
        else
        {
            vf("GameStateClientOffset", false, "read failed");
        }
        if (read_bytes(
                slot + LogicBattleModeClientHashEnabledOffset, &hashOn, 1))
        {
            BOOL keyOk =
                read_bytes(slot + LogicBattleModeClientHashKeyOffset, key, 16);
            BOOL used = NO;
            for (int i = 0; i < 16 && keyOk; i++)
                used = used || key[i];
            vf("LogicBattleModeClientHashEnabledOffset",
               hashOn <= 1 && (!hashOn || used),
               "on=%u key16=%d",
               (unsigned) hashOn,
               keyOk ? 1 : 0);
        }
        else
        {
            vf("LogicBattleModeClientHashEnabledOffset", false, "read failed");
        }
        if (!clientPtr)
        {
            vs("LogicBattleModeClientGameObjectManagerOffset", "no client");
            vs("LogicBattleModeClientInputManagerOffset", "no client");
        }
        else
        {
            if (read_ptr(clientPtr +
                             LogicBattleModeClientGameObjectManagerOffset,
                         &get_client_input_manager))
            {
                vf("LogicBattleModeClientGameObjectManagerOffset",
                   get_client_input_manager &&
                       pointer_plausible(get_client_input_manager),
                   "0x%llx",
                   (unsigned long long) get_client_input_manager);
            }
            else
            {
                vf("LogicBattleModeClientGameObjectManagerOffset",
                   false,
                   "read failed");
            }
            if (get_client_input_manager &&
                read_int(get_client_input_manager +
                             LogicGameObjectManagerServerGameObjectCountOffset,
                         &count))
            {
                vf("LogicGameObjectManagerServerGameObjectCountOffset",
                   count > 0 && count <= OBJECT_COUNT_MAX,
                   "count=%d",
                   count);
            }
            else
            {
                vf("LogicGameObjectManagerServerGameObjectCountOffset",
                   false,
                   "read failed");
            }
            if (get_client_input_manager &&
                read_ptr(get_client_input_manager +
                             LogicGameObjectManagerServerGameObjectsOffset,
                         &array))
            {
                vf("LogicGameObjectManagerServerGameObjectsOffset",
                   array && pointer_plausible(array),
                   "0x%llx",
                   (unsigned long long) array);
            }
            else
            {
                vf("LogicGameObjectManagerServerGameObjectsOffset",
                   false,
                   "read failed");
            }
            val_input_state = val_client_input(slot);
        }
    }
    if (read_ptr(base + LogicProjectileDataVtableRva, &vtable))
    {
        vf("LogicProjectileDataVtableRva",
           vtable && addr_executable(vtable),
           "asset anchor slot0=0x%llx",
           (unsigned long long) vtable);
    }
    else
    {
        vf("LogicProjectileDataVtableRva", false, "read failed");
    }
    vf("DATACONST_RVA",
       addr_readable(base + DataConstRvaLow, 16) &&
           addr_readable(base + DataConstRvaLow + DataConstRvaSize - 16, 16),
       "lo=0x%llx size=0x%llx",
       (unsigned long long) DataConstRvaLow,
       (unsigned long long) DataConstRvaSize);
    vf("ClientInputTypeTableRva",
       addr_readable(base + ClientInputTypeTableRva, 8),
       "0x%llx",
       (unsigned long long) ClientInputTypeTableRva);
    vf("ClientInputTypeFallbackRva",
       addr_readable(base + ClientInputTypeFallbackRva, 8),
       "0x%llx",
       (unsigned long long) ClientInputTypeFallbackRva);
    vf("CLIENTINPUT_HASH_MASK_RVA",
       addr_readable(base + ClientInputHashInnerMaskRva, 8) &&
           addr_readable(base + ClientInputHashOuterMaskRva, 8),
       "inner=0x%llx outer=0x%llx",
       (unsigned long long) ClientInputHashInnerMaskRva,
       (unsigned long long) ClientInputHashOuterMaskRva);
}

int val_client_input(uintptr_t clientPtr)
{
    uintptr_t get_client_input_manager = 0;
    uintptr_t queue                    = 0;
    uintptr_t array                    = 0;
    uintptr_t message                  = 0;
    int32_t   count                    = 0;
    int32_t   type                     = -1;
    int32_t   mx                       = 0;
    int32_t   my                       = 0;
    uint8_t   token[8]                 = {0};
    if (!read_ptr(clientPtr + LogicBattleModeClientInputManagerOffset,
                  &get_client_input_manager) ||
        !get_client_input_manager ||
        !pointer_plausible(get_client_input_manager))
    {
        return 0;
    }
    val_ci_manager = get_client_input_manager;
    if (!read_ptr(get_client_input_manager + ClientInputManagerInputQueueOffset,
                  &queue) ||
        !queue || !pointer_plausible(queue))
    {
        return 0;
    }
    val_ci_queue = queue;
    if (!read_int(queue + LogicGameObjectManagerServerGameObjectCountOffset,
                  &count) ||
        count <= 0 || count > 4096)
    {
        return 0;
    }
    val_ci_count = count;
    vf("LogicBattleModeClientInputManagerOffset",
       true,
       "0x%llx",
       (unsigned long long) get_client_input_manager);
    vf("ClientInputManagerInputQueueOffset",
       true,
       "0x%llx",
       (unsigned long long) queue);
    vf("CLIENTINPUT_QUEUE_COUNT", true, "count=%d", count);
    if (!read_ptr(queue + LogicGameObjectManagerServerGameObjectsOffset,
                  &array) ||
        !array || !read_ptr(array, &message) || !message ||
        !pointer_plausible(message))
    {
        vf("CLIENTINPUT_QUEUE_ARRAY", false, "array/message unreadable");
        return 1;
    }
    if (read_int(message + ClientInputMessageTypeOffset, &type))
    {
        vf("ClientInputMessageTypeOffset",
           type >= 0 && type <= (int32_t) ClientInputTypeCount,
           "type=%d max=%llu",
           type,
           (unsigned long long) ClientInputTypeCount);
    }
    else
    {
        vf("ClientInputMessageTypeOffset", false, "read failed");
    }
    if (read_int(message + ClientInputMessageXOffset, &mx) &&
        read_int(message + ClientInputMessageYOffset, &my))
    {
        vf("CLIENTINPUTMESSAGE_X/Y_OFF",
           span(mx) && span(my),
           "x=%d y=%d",
           mx,
           my);
    }
    else
    {
        vf("CLIENTINPUTMESSAGE_X/Y_OFF", false, "read failed");
    }
    vf("ClientInputMessageChecksumOffset",
       read_bytes(
           message + ClientInputMessageChecksumOffset, token, sizeof(token)),
       "hash=%02x%02x%02x%02x",
       token[0],
       token[1],
       token[2],
       token[3]);
    vf("ClientInputMessageSkillDataOffset",
       addr_readable(message + ClientInputMessageSkillDataOffset, 8),
       "readable");
    vf("ClientInputMessageSignedDataOffset",
       addr_readable(message + ClientInputMessageChecksumOffset +
                         ClientInputMessageSignedDataOffset,
                     4),
       "readable");
    return 1;
}

void val_character(uintptr_t own)
{
    int32_t   pid       = 0;
    int32_t   x         = 0;
    int32_t   y         = 0;
    int32_t   team      = 0;
    int32_t   angle     = 0;
    int32_t   hp        = 0;
    int32_t   collision = 0;
    uintptr_t data      = 0;
    char      name[48]  = {0};
    vf("LogicCharacterGlobalIdOffset",
       read_int(own + LogicCharacterGlobalIdOffset, &pid) && pid > 0,
       "pid=%d",
       pid);
    if (read_int(own + LogicCharacterXOffset, &x) &&
        read_int(own + LogicCharacterYOffset, &y))
    {
        vf("LOGICCHARACTER_X/Y_OFF", span(x) && span(y), "x=%d y=%d", x, y);
    }
    else
    {
        vf("LOGICCHARACTER_X/Y_OFF", false, "read failed");
    }
    if (read_int(own + LogicCharacterTeamOffset, &team))
    {
        vf("LogicCharacterTeamOffset", team >= 0 && team <= 3, "team=%d", team);
    }
    else
    {
        vf("LogicCharacterTeamOffset", false, "read failed");
    }
    if (read_int(own + LogicCharacterAttackAngleOffset, &angle))
    {
        vf("LogicCharacterAttackAngleOffset",
           angle >= -720 && angle <= 720,
           "angle=%d",
           angle);
    }
    else
    {
        vf("LogicCharacterAttackAngleOffset", false, "read failed");
    }
    if (read_int(own + LogicCharacterHitpointsOffset, &hp))
    {
        vf("LogicCharacterHitpointsOffset",
           hp >= 0 && hp <= 20000 && (uint32_t) hp == own_hp,
           "hp=%d scan=%u",
           hp,
           own_hp);
    }
    else
    {
        vf("LogicCharacterHitpointsOffset", false, "read failed");
    }
    if (read_ptr(own + LogicGameObjectDataOffset, &data))
    {
        vf("LogicGameObjectDataOffset",
           data && pointer_plausible(data),
           "0x%llx",
           (unsigned long long) data);
    }
    else
    {
        vf("LogicGameObjectDataOffset", false, "read failed");
    }
    if (data && pointer_plausible(data) &&
        read_int(data + LogicCharacterDataCollisionRadiusOffset, &collision))
    {
        vf("LogicCharacterDataCollisionRadiusOffset",
           collision >= 0 && collision <= 100000,
           "v=%d",
           collision);
    }
    else
    {
        vf("LogicCharacterDataCollisionRadiusOffset", false, "read failed");
    }
    if (data && pointer_plausible(data))
    {
        typedef uintptr_t (*name_t)(uintptr_t);
        name_t    fn = reinterpret_cast<name_t>(callable(LogicDataGetNameRva));
        uintptr_t text = fn ? fn(data) : 0;
        BOOL      ok   = NO;
        if (text && pointer_plausible(text))
        {
            ok = read_scstring(text, sizeof(name) - 1, name, sizeof(name)) ? YES
                                                                           : NO;
        }
        for (size_t i = 0; ok && i < sizeof(name) - 1 && name[i]; i++)
        {
            if (name[i] < 0x20 || name[i] > 0x7e) ok = NO;
        }
        vf("LogicDataGetNameRva(x)", ok, "name=%s", ok ? name : "-");
        if (text && pointer_plausible(text))
        {
            int32_t   strLen  = -1;
            uintptr_t strData = 0;
            BOOL      lenOk = read_int(text + StringByteLengthOffset, &strLen);
            BOOL dataOk     = read_ptr(text + StringCharactersOffset, &strData);
            vf("SCSTRING_LENGTH/DATA_OFF",
               lenOk && dataOk && strLen >= 0 && strLen <= 64 &&
                   strLen == (int32_t) strlen(ok ? name : ""),
               "len=%d data=0x%llx",
               strLen,
               (unsigned long long) strData);
        }
        else
        {
            vs("SCSTRING_LENGTH/DATA_OFF", "no name object");
        }
    }
    else
    {
        vs("LogicDataGetNameRva(x)", "no data pointer");
    }
}

void val_tilemap(uintptr_t clientPtr)
{
    typedef uintptr_t (*map_t)(uintptr_t);
    map_t fn =
        reinterpret_cast<map_t>(callable(LogicBattleModeClientGetTileMapRva));
    uintptr_t map    = fn ? fn(clientPtr) : 0;
    uintptr_t tiles  = 0;
    int32_t   width  = 0;
    int32_t   height = 0;
    vf("LogicBattleModeClientGetTileMapRva",
       map && pointer_plausible(map),
       "0x%llx",
       (unsigned long long) map);
    if (!map || !pointer_plausible(map))
    {
        vs("LogicTileMapWidthOffset", "no tilemap");
        vs("LogicTileMapHeightOffset", "no tilemap");
        vs("LogicTileMapTileArrayOffset", "no tilemap");
        vs("LogicTilePathFinderMaskOffset", "no tilemap");
        vs("LogicTileWaterPathFinderMaskOffset", "no tilemap");
        return;
    }
    if (read_int(map + LogicTileMapWidthOffset, &width))
    {
        vf("LogicTileMapWidthOffset", width > 0 && width <= 120, "w=%d", width);
    }
    else
    {
        vf("LogicTileMapWidthOffset", false, "read failed");
    }
    if (read_int(map + LogicTileMapHeightOffset, &height))
    {
        vf("LogicTileMapHeightOffset",
           height > 0 && height <= 120,
           "h=%d",
           height);
    }
    else
    {
        vf("LogicTileMapHeightOffset", false, "read failed");
    }
    if (!read_ptr(map + LogicTileMapTileArrayOffset, &tiles) || !tiles)
    {
        vf("LogicTileMapTileArrayOffset", false, "read failed");
        vs("LogicTilePathFinderMaskOffset", "no tile array");
        vs("LogicTileWaterPathFinderMaskOffset", "no tile array");
        return;
    }
    vf("LogicTileMapTileArrayOffset",
       pointer_plausible(tiles),
       "0x%llx",
       (unsigned long long) tiles);
    {
        int      total   = (width > 0 && height > 0 && width * height < 4096)
                               ? width * height
                               : 256;
        int      stride  = total / 96 + 1;
        int      seen    = 0;
        int      blocked = 0;
        uint64_t sample  = 0;
        for (int i = 0; i < total; i += stride)
        {
            uintptr_t item  = 0;
            uint64_t  mask  = 0;
            uint64_t  water = 0;
            uint64_t  hard  = 0;
            if (!read_ptr(tiles + (uintptr_t) i * 8, &item) || !item) continue;
            if (!read_bytes(item + LogicTilePathFinderMaskOffset,
                            &mask,
                            sizeof(mask)) ||
                !read_bytes(item + LogicTileWaterPathFinderMaskOffset,
                            &water,
                            sizeof(water)) ||
                !read_bytes(
                    item + LogicTileHardBlockMaskOffset, &hard, sizeof(hard)))
                continue;
            seen++;
            if (mask && !sample) sample = mask;
            if ((mask | water | hard) & 0x1ffULL) blocked++;
        }
        vf("LogicTilePathFinderMaskOffset",
           seen > 0,
           "blocked=%d/%d sample=0x%llx",
           blocked,
           seen,
           (unsigned long long) sample);
    }

    vf("TILEMAP_GEOMETRY",
       width > 0 && height > 0 && width == wall_cache_width() &&
           height == wall_cache_height(),
       "%dx%d cache=%dx%d",
       width,
       height,
       wall_cache_width(),
       wall_cache_height());
}

void val_projectile()
{
    uintptr_t found     = 0;
    uintptr_t objPtr    = 0;
    int32_t   startTick = 0;
    int32_t   endTick   = 0;
    int32_t   sx        = 0;
    int32_t   sy        = 0;
    int32_t   ex        = 0;
    int32_t   ey        = 0;
    {
        float   px = 0.0f, py = 0.0f;
        int32_t t0 = 0, t1 = 0;
        if (!ad::find_live_projectile(&objPtr, &found, &t0, &t1, &px, &py))
        {
            objPtr = 0;
        }
        else if (val_proj_tries == 5)
        {
            line("[VAL] proj probe objs=%d shot=%d t=%d..%d p=(%.0f,%.0f)\n",
                 obj_count,
                 (int) ad_proj_n,
                 t0,
                 t1,
                 px,
                 py);
        }
    }
    if (!objPtr)
    {
        val_proj_tries++;
        if (val_proj_tries >= 120)
        {
            vs("PROJECTILEDATA_*", "no projectile seen in 120s");
            val_proj_done = 1;
        }
        return;
    }
    {
        bool ticks_ok =
            read_int(objPtr + LogicProjectileServerTickStartOffset,
                     &startTick) &&
            read_int(objPtr + LogicProjectileServerTickEndOffset, &endTick);
        if (!ticks_ok || startTick < 0 || endTick < startTick)
        {
            if (val_proj_tries < 119)
            {
                val_proj_tries++;
                return;
            }
            val_proj_done = 1;
            vf("PROJSERVER_START/ENDTICK",
               false,
               "%d..%d after 120s",
               startTick,
               endTick);
            char dump[420];
            int  at = 0;
            at += snprintf(
                dump, sizeof(dump), " obj=%llx", (unsigned long long) objPtr);
            for (int i = 0x00; i <= 0x60 && at < (int) sizeof(dump) - 12;
                 i += 4)
            {
                int32_t v = 0;
                read_int(objPtr + i, &v);
                at += snprintf(dump + at, sizeof(dump) - at, " %x=%d", i, v);
            }
            line("[VAL] proj obj dump%s\n", dump);
            return;
        }
        vf("PROJSERVER_START/ENDTICK", true, "%d..%d", startTick, endTick);
    }
    val_proj_done = 1;
    vf("PROJSERVER_TICK_SPAN",
       endTick - startTick >= 0 && endTick - startTick <= 3000,
       "%d..%d span=%d",
       startTick,
       endTick,
       endTick - startTick);
    {
        int32_t xi = 0, yi = 0;
        if (read_int(objPtr + LogicProjectileServerXOffset, &xi) &&
            read_int(objPtr + LogicProjectileServerYOffset, &yi))
        {
            int   mapW = wall_cache_width(), mapH = wall_cache_height();
            float unit   = wall_logic_units(1);
            bool  in_map = mapW > 0 && mapH > 0 && xi >= 0 && yi >= 0 &&
                          (float) xi <= (float) mapW * unit &&
                          (float) yi <= (float) mapH * unit;
            vf("PROJSERVER_X/Y_OFF",
               in_map,
               "p=(%d,%d) map=%dx%d",
               xi,
               yi,
               mapW,
               mapH);
        }
        else
        {
            vf("PROJSERVER_X/Y_OFF", false, "read failed");
        }
    }
    if (!found)
    {
        vs("PROJECTILEDATA_*", "shot exposes no data-asset pointer");
        return;
    }
    vf("LogicProjectileDataSize",
       addr_readable(found, LogicProjectileDataSize),
       "0x%llx readable 0x%llx",
       (unsigned long long) found,
       (unsigned long long) LogicProjectileDataSize);
    vf("PROJSERVER_IN_OBJECT",
       LogicProjectileServerTargetYOffset + 4 <= OBJ_BLOCK_SIZE,
       "endY=0x%llx block=0x%x",
       (unsigned long long) (LogicProjectileServerTargetYOffset + 4),
       (unsigned) OBJ_BLOCK_SIZE);
    if (read_int(objPtr + LogicProjectileServerStartXOffset, &sx) &&
        read_int(objPtr + LogicProjectileServerStartYOffset, &sy) &&
        read_int(objPtr + LogicProjectileServerTargetXOffset, &ex) &&
        read_int(objPtr + LogicProjectileServerTargetYOffset, &ey))
    {
        vf("PROJSERVER_COORDS",
           sx > 0 && sy > 0 && ex > 0 && ey > 0 && span(sx) && span(sy) &&
               span(ex) && span(ey),
           "(%d,%d)->(%d,%d)",
           sx,
           sy,
           ex,
           ey);
    }
    else
    {
        vf("PROJSERVER_COORDS", false, "read failed");
    }
}

void validate_offsets()
{
    uintptr_t slot = (uintptr_t) read_global_ptr(GameStateManagerRva);
    val_ok         = 0;
    val_fail       = 0;
    val_skip       = 0;
    line("\n=== validate ===\n");
    val_rva_table();
    val_globals(slot);
    if (own_elem && pointer_plausible(own_elem)) val_character(own_elem);
    {
        uintptr_t clientPtr = client();
        if (clientPtr && pointer_plausible(clientPtr))
        {
            val_tilemap(clientPtr);
        }
        else
        {
            vs("LogicBattleModeClientGetTileMapRva", "no client");
        }
    }
    val_projectile();
    line("[VAL] summary ok=%d fail=%d skip=%d\n", val_ok, val_fail, val_skip);
}

void maybe_validate()
{
    uint64_t now_us = us();
    if (val_stage == 0)
    {
        if (!base || !own_elem || dead) return;
        val_stage = 1;
        validate_offsets();
        val_probe_next = now_us + 500000ULL;
        return;
    }
    if (val_input_state && val_proj_done) return;
    if (val_probe_next && now_us < val_probe_next) return;
    val_probe_next = now_us + 1000000ULL;
    if (!base || !own_elem || dead) return;
    val_ok   = 0;
    val_fail = 0;
    val_skip = 0;
    if (val_input_state == 0)
    {
        uintptr_t battle = (uintptr_t) read_global_ptr(GameStateManagerRva);
        val_input_state  = (battle && pointer_plausible(battle))
                               ? val_client_input(battle)
                               : 0;
        if (val_input_state == 0 && ++val_input_tries >= 120)
        {
            char why[128];
            snprintf(why,
                     sizeof(why),
                     "queue not populated in 120s mgr=0x%llx q=0x%llx n=%d",
                     (unsigned long long) val_ci_manager,
                     (unsigned long long) val_ci_queue,
                     val_ci_count);
            vs("CLIENTINPUT_*", why);
            val_input_state = 2;
        }
    }
    if (!val_proj_done) val_projectile();
    if (val_ok + val_fail + val_skip > 0)
    {
        line("[VAL] late summary ok=%d fail=%d skip=%d\n",
             val_ok,
             val_fail,
             val_skip);
    }
}

const char* dir_name(int dir)
{
    switch (dir)
    {
    case 0:
        return "+x";
    case 1:
        return "+x+y";
    case 2:
        return "+y";
    case 3:
        return "-x+y";
    case 4:
        return "-x";
    case 5:
        return "-x-y";
    case 6:
        return "-y";
    case 7:
        return "+x-y";
    default:
        return "none";
    }
}
}

void diag_tick()
{
    char          target_buf[32];
    uint64_t      now_us = 0;
    unsigned long hz     = 0;
    uint64_t      wl_avg = 0;
    uint32_t      wl_max = 0;
    if (!open_log()) return;
    maybe_validate();
    now_us = us();
    if (base && own_elem && !dead)
    {
        static uint64_t last_fast = 0;
        uint64_t        prev = __atomic_load_n(&last_fast, __ATOMIC_RELAXED);
        bool            busy =
            ad::threat_n || active || ad_hold_ms || ad_urgent || ad_human;
        uint64_t period = busy ? FAST_TRACE_US : IDLE_TRACE_US;
        bool     slot   = !prev || now_us - prev >= period;
        if (slot && !__atomic_compare_exchange_n(&last_fast,
                                                 &prev,
                                                 now_us,
                                                 false,
                                                 __ATOMIC_RELAXED,
                                                 __ATOMIC_RELAXED))
            slot = false;
        if (slot)
        {
            char buf[2048];
            int  n = snprintf(buf,
                             sizeof(buf),
                             "[f] t=%llu p=%d,%d v=%d d=%.3f,%.3f tgt=%s n=%d "
                              "h=%d u=%d hm=%d",
                             (unsigned long long) (now_us / 1000),
                             own_x,
                             own_y,
                             (int) (own_speed + 0.5f),
                             ad_dir_x,
                             ad_dir_y,
                             target_text(target_buf, sizeof(target_buf)),
                             ad::threat_n,
                             ad_hold_ms,
                             ad_urgent,
                             ad_human);
            for (int i = 0;
                 i < ad::threat_n && n > 0 && n < (int) sizeof(buf) - 48;
                 i++)
            {
                n += snprintf(buf + n,
                              sizeof(buf) - (size_t) n,
                              ";%d,%d,%d,%d,%d",
                              (int) ad::threats[i].x,
                              (int) ad::threats[i].y,
                              (int) ad::threats[i].vx,
                              (int) ad::threats[i].vy,
                              (int) ad::threats[i].rad);
            }
            if (n > 0 && n < (int) sizeof(buf) - 2)
            {
                buf[n++] = '\n';
                buf[n]   = '\0';
                line("%s", buf);
            }
        }
    }
    if (last_report_us && now_us - last_report_us < REPORT_US) return;
    last_report_us = now_us;
    if (!base || !own_elem || dead)
    {
        if (!base || !own_elem) match_open = 0;
        if (!idle_logged)
        {
            line("idle\n");
            idle_logged = 1;
        }
        rotate_if_needed();
        return;
    }
    idle_logged = 0;
    if (!match_open)
    {
        line("\n=== match ===\n");
        match_open  = 1;
        dodge_count = 0;
        hit_count   = 0;
        last_hp     = -1;
        last_tx     = 1.0f;
        last_ty     = 0.0f;
    }
    if (last_hp > 0 && static_cast<int32_t>(own_hp) < last_hp) hit_count++;
    last_hp = static_cast<int32_t>(own_hp);
    if (active && ad_dir_x * last_tx + ad_dir_y * last_ty < 0.94f)
    {
        dodge_count++;
    }
    last_tx = ad_dir_x;
    last_ty = ad_dir_y;
    if (!active)
    {
        last_tx = 1.0f;
        last_ty = 0.0f;
    }
    {
        static uint32_t prev_ticks = 0;
        static uint64_t prev_ms    = 0;
        uint64_t        now_ms     = now_us / 1000;
        uint32_t        ticks      = workload_ticks - prev_ticks;
        uint64_t dt_ms  = (prev_ms && now_ms > prev_ms) ? now_ms - prev_ms : 0;
        hz              = dt_ms ? static_cast<unsigned long>(
                         (static_cast<uint64_t>(ticks) * 1000ULL) / dt_ms)
                                : 0UL;
        prev_ticks      = workload_ticks;
        prev_ms         = now_ms;
        wl_avg          = workload_us_n ? workload_us_sum / workload_us_n : 0;
        wl_max          = workload_us_max;
        workload_us_sum = 0;
        workload_us_max = 0;
        workload_us_n   = 0;
    }
    int32_t own_aim = 0;
    if (own_elem)
    {
        read_int(own_elem + LogicCharacterAttackAngleOffset, &own_aim);
    }
    line("[m] hz=%lu wl=%lu/%lu objs=%d ch=%d pr=%d en=%d hp=%u p=(%d,%d) v=%d "
         "dir=%s map=%dx%d dodge=%d hit=%d hb=%d ind=%d fd=%d tgt=%s "
         "hu=%d pm=%u/%u wb=%d/%d pt=%d wp=%d wu=%d\n",
         static_cast<unsigned long>(hz),
         static_cast<unsigned long>(wl_avg),
         static_cast<unsigned long>(wl_max),
         obj_count,
         ad_char_n,
         ad_proj_n,
         ad_enemy_n,
         own_hp,
         own_x,
         own_y,
         (int) (own_speed + 0.5f),
         active ? dir_name(ad_best_dir) : "idle",
         wall_cache_width(),
         wall_cache_height(),
         dodge_count,
         hit_count,
         (int) (ad::own_hitbox + 0.5f),
         ad_ind_n,
         own_aim,
         target_text(target_buf, sizeof(target_buf)),
         ad_human,
         ad_proj_move,
         ad_proj_sample,
         wall_cached_blocked_count(),
         wall_cache_width() * wall_cache_height(),
         ad_pass_own,
         wall_engine_blocked_total(),
         ad_windup_n);
    ad_proj_move   = 0;
    ad_proj_sample = 0;
    {
        char qbuf[192];
        int  qn = snprintf(qbuf, sizeof(qbuf), "qc=");
        for (int i = 0; i < 16 && qn > 0 && qn < (int) sizeof(qbuf) - 8; i++)
        {
            qn += snprintf(qbuf + qn,
                           sizeof(qbuf) - (size_t) qn,
                           i ? ",%u" : "%u",
                           (unsigned) ad_qc[i]);
            ad_qc[i] = 0;
        }
        if (qn > 0 && qn < (int) sizeof(qbuf) - 2)
        {
            qbuf[qn++] = '\n';
            qbuf[qn]   = '\0';
            line("%s", qbuf);
        }
    }
    for (int i = 0; i < ad_threat_n && i < 3; i++)
    {
        line("[t%d] %s pid=%d p=(%.0f,%.0f) v=(%.0f,%.0f) r=%.0f d=%.0f "
             "ca=%.0f ttc=%.3f\n",
             i,
             ad_threat_name[i][0] ? ad_threat_name[i] : "-",
             ad_threat_pid[i],
             ad_threat_x[i],
             ad_threat_y[i],
             ad_threat_vx[i],
             ad_threat_vy[i],
             ad_threat_r[i],
             ad_threat_d[i],
             ad_threat_ca[i],
             ad_threat_ttc[i]);
    }
    if (ad_hit_dmg > 0 && !ad_hit_act && ad_hit_n == 0)
    {
        static uint64_t last_env_us = 0;
        if (!last_env_us || now_us - last_env_us >= 2000000ULL)
        {
            last_env_us = now_us;
            line("[env] dmg=%d hp=%u p=(%d,%d)\n",
                 ad_hit_dmg,
                 own_hp,
                 ad_hit_x,
                 ad_hit_y);
        }
        ad_hit_dmg    = 0;
        ad_hit_picked = 0;
    }
    else if (ad_hit_dmg > 0)
    {
        char buf[256];
        int  len = snprintf(buf,
                           sizeof(buf),
                           "dmg=%d act=%d n=%d p=(%d,%d)",
                           ad_hit_dmg,
                           ad_hit_act,
                           ad_hit_n,
                           ad_hit_x,
                           ad_hit_y);
        if (ad_hit_picked < 1 && len > 0)
        {
            const char* why = " | unresolved";
            snprintf(buf + len, sizeof(buf) - (size_t) len, "%s", why);
        }
        for (int i = 0; i < ad_hit_picked && i < AD_REPORT_MAX; i++)
        {
            if (len <= 0 || len >= (int) sizeof(buf) - 40) break;
            len += snprintf(buf + len,
                            sizeof(buf) - (size_t) len,
                            " | ca=%.0f ttc=%.2f r=%.0f",
                            ad_hit_ca[i],
                            ad_hit_ttc[i],
                            ad_hit_r[i]);
        }
        line("[hit] %s\n", buf);
        ad_hit_dmg    = 0;
        ad_hit_picked = 0;
    }
    for (int i = 0; i < ad_proj_n && i < AD_REPORT_MAX; i++)
    {
        line("[p%d] pid=%d t=%d..%d p=(%.0f,%.0f) v=(%.0f,%.0f) sp=%.0f "
             "r=%.0f vel=%d\n",
             i,
             ad_pj_pid[i],
             ad_pj_t0[i],
             ad_pj_t1[i],
             ad_pj_x[i],
             ad_pj_y[i],
             ad_pj_vx[i],
             ad_pj_vy[i],
             ad_pj_sp[i],
             ad_pj_r[i],
             ad_pj_vel[i]);
    }
    for (int i = 0; i < ad_enemy_n && i < 4; i++)
    {
        line("[e%d] pid=%d p=(%.0f,%.0f) hp=%d d=%.0f a=%d\n",
             i,
             ad_enemy_pid[i],
             ad_enemy_x[i],
             ad_enemy_y[i],
             ad_enemy_hp[i],
             ad_enemy_d[i],
             ad_enemy_aim[i]);
    }
    rotate_if_needed();
}
