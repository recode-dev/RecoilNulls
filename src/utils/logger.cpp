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
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace
{
constexpr long long LOG_MAX_BYTES = 4000000;
constexpr uint64_t REPORT_US = 1000000ULL;

char log_path[1100] = {0};
FILE* log_file = nullptr;
uint64_t last_report_us = 0;
int32_t last_hp = -1;
float last_tx = 1.0f;
float last_ty = 0.0f;
int dodge_count = 0;
int hit_count = 0;
int match_open = 0;
int idle_logged = 0;

void document_path(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    out[0] = 0;
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

int val_ok = 0;
int val_fail = 0;
int val_skip = 0;
int val_stage = 0;
int val_proj_done = 0;
int val_proj_tries = 0;
int val_input_state = 0;
int val_input_tries = 0;
uintptr_t val_ci_manager = 0;
uintptr_t val_ci_queue = 0;
int32_t val_ci_count = 0;
uint64_t val_probe_next = 0;

void vf(const char* name, bool pass, const char* format, ...)
{
    char buf[192];
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
        uint64_t rva;
    } table[] = {
        {"RVA_BATTLEMODE_GETINSTANCE", RVA_BATTLEMODE_GETINSTANCE},
        {"RVA_GETTILEMAP", RVA_LOGICBATTLEMODECLIENT_GETTILEMAP},
        {"RVA_GETOWNCHARACTER", RVA_LOGICBATTLEMODECLIENT_GETOWNCHARACTER},
        {"RVA_GETOWNPLAYERINDEX", RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX},
        {"RVA_GETOWNPLAYERTEAM", RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERTEAM},
        {"RVA_LOGICDATA_GETNAME", RVA_LOGICDATA_GETNAME},
        {"RVA_SETCLIENTPREDICTIONMOVETO",
         RVA_LOGICBATTLEMODECLIENT_SETCLIENTPREDICTIONMOVETO},
        {"RVA_CLIENTINPUTMESSAGE_CTOR", RVA_CLIENTINPUTMESSAGE_CONSTRUCT},
        {"RVA_CLIENTINPUTMANAGER_ADDINPUT", RVA_CLIENTINPUTMANAGER_ADDINPUT},
        {"RVA_ISPASSABLEPATHFINDER", RVA_LOGIC_TILEMAP_ISPASSABLEPATHFINDER},
        {"RVA_ISPLAYERLINEOFSIGHTCLEAR",
         RVA_LOGIC_TILEMAP_ISPLAYERLINEOFSIGHTCLEAR},
        {"RVA_LOGIC_TILE_TO_LOGIC", RVA_LOGIC_TILE_TO_LOGIC},
        {"RVA_LOGIC_LOGIC_TO_TILE", RVA_LOGIC_LOGIC_TO_TILE},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
    {
        uintptr_t address = callable(table[i].rva);
        uint32_t head = 0;
        BOOL exec = NO;
        BOOL read = NO;
        if (address)
        {
            exec = addr_executable(address);
            read = read_bytes(address, &head, sizeof(head));
        }
        vf(table[i].name,
           address && exec && read && head != 0,
           "rva=0x%llx addr=0x%llx exec=%d head=%08x",
           (unsigned long long)table[i].rva,
           (unsigned long long)address,
           exec ? 1 : 0,
           head);
    }
    {
        int center = game_tile_to_logic(3);
        vf("RVA_TILE_TO_LOGIC(x)",
           center == 3 * 300 + 150 && game_logic_to_tile(center) == 3,
           "3 -> %d (want 1050) back=%d",
           center,
           game_logic_to_tile(center));
    }
    vf("RVA_LOGIC_TO_TILE(x)",
       game_logic_to_tile(900) == 3,
       "900 -> %d (want 3)",
       game_logic_to_tile(900));
}

void val_globals(uintptr_t slot)
{
    uintptr_t scene = 0;
    uintptr_t clientPtr = 0;
    uintptr_t manager = 0;
    uintptr_t array = 0;
    uintptr_t vtable = 0;
    int32_t state = -1;
    int32_t count = 0;
    uint8_t hashOn = 0;
    uint8_t key[16] = {0};
    vf("GAMESTATE_RVA",
       slot && pointer_plausible(slot),
       "0x%llx",
       (unsigned long long)slot);
    if (!slot)
    {
        vs("GAMESTATE_STATE_OFF", "no game state");
        vs("GAMESTATE_SCENE_OFF", "no game state");
        return;
    }
    if (read_int(slot + GAMESTATE_STATE_OFF, &state))
    {
        vf("GAMESTATE_STATE_OFF", state >= 0 && state <= 8, "state=%d", state);
    }
    else
    {
        vf("GAMESTATE_STATE_OFF", false, "read failed");
    }
    if (read_ptr(slot + GAMESTATE_SCENE_OFF, &scene))
    {
        vf("GAMESTATE_SCENE_OFF",
           scene && pointer_plausible(scene),
           "0x%llx",
           (unsigned long long)scene);
    }
    else
    {
        vf("GAMESTATE_SCENE_OFF", false, "read failed");
    }
    if (!scene)
    {
        vs("SCENE_CLIENT_OFF", "no scene");
        vs("LOGICBATTLEMODECLIENT_OBJECTMANAGER_OFF", "no scene");
        vs("LOGICBATTLEMODECLIENT_INPUTMANAGER_OFF", "no scene");
    }
    else
    {
        if (read_ptr(scene + SCENE_CLIENT_OFF, &clientPtr))
        {
            vf("SCENE_CLIENT_OFF",
               clientPtr && pointer_plausible(clientPtr),
               "0x%llx",
               (unsigned long long)clientPtr);
        }
        else
        {
            vf("SCENE_CLIENT_OFF", false, "read failed");
        }
        if (read_bytes(scene + BATTLEMODE_HASH_ENABLED_OFF, &hashOn, 1))
        {
            BOOL keyOk = read_bytes(scene + BATTLEMODE_HASH_KEY_OFF, key, 16);
            BOOL used = NO;
            for (int i = 0; i < 16 && keyOk; i++)
                used = used || key[i];
            vf("BATTLEMODE_HASH_ENABLED_OFF",
               hashOn <= 1 && (!hashOn || used),
               "on=%u key16=%d",
               (unsigned)hashOn,
               keyOk ? 1 : 0);
        }
        else
        {
            vf("BATTLEMODE_HASH_ENABLED_OFF", false, "read failed");
        }
        if (!clientPtr)
        {
            vs("LOGICBATTLEMODECLIENT_OBJECTMANAGER_OFF", "no client");
            vs("LOGICBATTLEMODECLIENT_INPUTMANAGER_OFF", "no client");
        }
        else
        {
            if (read_ptr(clientPtr + LOGICBATTLEMODECLIENT_OBJECTMANAGER_OFF,
                         &manager))
            {
                vf("LOGICBATTLEMODECLIENT_OBJECTMANAGER_OFF",
                   manager && pointer_plausible(manager),
                   "0x%llx",
                   (unsigned long long)manager);
            }
            else
            {
                vf("LOGICBATTLEMODECLIENT_OBJECTMANAGER_OFF",
                   false,
                   "read failed");
            }
            if (manager && read_int(manager + OBJECTMANAGER_COUNT_OFF, &count))
            {
                vf("OBJECTMANAGER_COUNT_OFF",
                   count > 0 && count <= OBJECT_COUNT_MAX,
                   "count=%d",
                   count);
            }
            else
            {
                vf("OBJECTMANAGER_COUNT_OFF", false, "read failed");
            }
            if (manager && read_ptr(manager + OBJECTMANAGER_ARRAY_OFF, &array))
            {
                vf("OBJECTMANAGER_ARRAY_OFF",
                   array && pointer_plausible(array),
                   "0x%llx",
                   (unsigned long long)array);
            }
            else
            {
                vf("OBJECTMANAGER_ARRAY_OFF", false, "read failed");
            }
            val_input_state = val_client_input(clientPtr);
        }
    }
    if (read_ptr(base + VTABLE_PROJECTILEDATA_RVA, &vtable))
    {
        vf("VTABLE_PROJECTILEDATA_RVA",
           vtable && addr_executable(vtable),
           "slot0=0x%llx",
           (unsigned long long)vtable);
    }
    else
    {
        vf("VTABLE_PROJECTILEDATA_RVA", false, "read failed");
    }
    vf("DATACONST_RVA",
       addr_readable(base + DATACONST_RVA_LO, 16) &&
           addr_readable(base + DATACONST_RVA_LO + DATACONST_RVA_SIZE - 16, 16),
       "lo=0x%llx size=0x%llx",
       (unsigned long long)DATACONST_RVA_LO,
       (unsigned long long)DATACONST_RVA_SIZE);
    vf("CLIENTINPUT_TYPE_TABLE_RVA",
       addr_readable(base + CLIENTINPUT_TYPE_TABLE_RVA, 8),
       "0x%llx",
       (unsigned long long)CLIENTINPUT_TYPE_TABLE_RVA);
    vf("CLIENTINPUT_TYPE_FALLBACK_RVA",
       addr_readable(base + CLIENTINPUT_TYPE_FALLBACK_RVA, 8),
       "0x%llx",
       (unsigned long long)CLIENTINPUT_TYPE_FALLBACK_RVA);
    vf("CLIENTINPUT_HASH_MASK_RVA",
       addr_readable(base + CLIENTINPUT_HASH_INNER_MASK_RVA, 8) &&
           addr_readable(base + CLIENTINPUT_HASH_OUTER_MASK_RVA, 8),
       "inner=0x%llx outer=0x%llx",
       (unsigned long long)CLIENTINPUT_HASH_INNER_MASK_RVA,
       (unsigned long long)CLIENTINPUT_HASH_OUTER_MASK_RVA);
}

int val_client_input(uintptr_t clientPtr)
{
    uintptr_t manager = 0;
    uintptr_t queue = 0;
    uintptr_t array = 0;
    uintptr_t message = 0;
    int32_t count = 0;
    int32_t type = -1;
    int32_t mx = 0;
    int32_t my = 0;
    uint8_t token[8] = {0};
    if (!read_ptr(clientPtr + LOGICBATTLEMODECLIENT_INPUTMANAGER_OFF,
                  &manager) ||
        !manager || !pointer_plausible(manager))
    {
        return 0;
    }
    val_ci_manager = manager;
    if (!read_ptr(manager + CLIENTINPUTMANAGER_QUEUE_OFF, &queue) || !queue ||
        !pointer_plausible(queue))
    {
        return 0;
    }
    val_ci_queue = queue;
    if (!read_int(queue + OBJECTMANAGER_COUNT_OFF, &count) || count <= 0 ||
        count > 4096)
    {
        return 0;
    }
    val_ci_count = count;
    vf("LOGICBATTLEMODECLIENT_INPUTMANAGER_OFF",
       true,
       "0x%llx",
       (unsigned long long)manager);
    vf("CLIENTINPUTMANAGER_QUEUE_OFF",
       true,
       "0x%llx",
       (unsigned long long)queue);
    vf("CLIENTINPUT_QUEUE_COUNT", true, "count=%d", count);
    if (!read_ptr(queue + OBJECTMANAGER_ARRAY_OFF, &array) || !array ||
        !read_ptr(array, &message) || !message || !pointer_plausible(message))
    {
        vf("CLIENTINPUT_QUEUE_ARRAY", false, "array/message unreadable");
        return 1;
    }
    if (read_int(message + CLIENTINPUTMESSAGE_TYPE_OFF, &type))
    {
        vf("CLIENTINPUTMESSAGE_TYPE_OFF",
           type >= 0 && type <= (int32_t)CLIENTINPUT_TYPE_COUNT,
           "type=%d max=%llu",
           type,
           (unsigned long long)CLIENTINPUT_TYPE_COUNT);
    }
    else
    {
        vf("CLIENTINPUTMESSAGE_TYPE_OFF", false, "read failed");
    }
    if (read_int(message + CLIENTINPUTMESSAGE_X_OFF, &mx) &&
        read_int(message + CLIENTINPUTMESSAGE_Y_OFF, &my))
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
    vf("CLIENTINPUT_TOKEN_OFF",
       read_bytes(message + CLIENTINPUT_TOKEN_OFF, token, sizeof(token)),
       "hash=%02x%02x%02x%02x",
       token[0],
       token[1],
       token[2],
       token[3]);
    vf("CLIENTINPUT_SKILLDATA_OFF",
       addr_readable(message + CLIENTINPUT_SKILLDATA_OFF, 8),
       "readable");
    vf("CLIENTINPUT_TOKEN_INPUT_OFF",
       addr_readable(
           message + CLIENTINPUT_TOKEN_OFF + CLIENTINPUT_TOKEN_INPUT_OFF, 4),
       "readable");
    return 1;
}

void val_character(uintptr_t own)
{
    int32_t pid = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t team = 0;
    int32_t angle = 0;
    int32_t hp = 0;
    int32_t collision = 0;
    uintptr_t data = 0;
    char name[48] = {0};
    vf("LOGICCHARACTER_GLOBALID_OFF",
       read_int(own + LOGICCHARACTER_GLOBALID_OFF, &pid) && pid > 0,
       "pid=%d",
       pid);
    if (read_int(own + LOGICCHARACTER_X_OFF, &x) &&
        read_int(own + LOGICCHARACTER_Y_OFF, &y))
    {
        vf("LOGICCHARACTER_X/Y_OFF", span(x) && span(y), "x=%d y=%d", x, y);
    }
    else
    {
        vf("LOGICCHARACTER_X/Y_OFF", false, "read failed");
    }
    if (read_int(own + LOGICCHARACTER_TEAM_OFF, &team))
    {
        vf("LOGICCHARACTER_TEAM_OFF", team >= 0 && team <= 3, "team=%d", team);
    }
    else
    {
        vf("LOGICCHARACTER_TEAM_OFF", false, "read failed");
    }
    if (read_int(own + LOGICCHARACTER_ATTACK_ANGLE_OFF, &angle))
    {
        vf("LOGICCHARACTER_ATTACK_ANGLE_OFF",
           angle >= -720 && angle <= 720,
           "angle=%d",
           angle);
    }
    else
    {
        vf("LOGICCHARACTER_ATTACK_ANGLE_OFF", false, "read failed");
    }
    if (read_int(own + LOGICCHARACTER_HP_OFF, &hp))
    {
        vf("LOGICCHARACTER_HP_OFF",
           hp >= 0 && hp <= 20000 && (uint32_t)hp == own_hp,
           "hp=%d scan=%u",
           hp,
           own_hp);
    }
    else
    {
        vf("LOGICCHARACTER_HP_OFF", false, "read failed");
    }
    if (read_ptr(own + LOGICGAMEOBJECT_DATA_OFF, &data))
    {
        vf("LOGICGAMEOBJECT_DATA_OFF",
           data && pointer_plausible(data),
           "0x%llx",
           (unsigned long long)data);
    }
    else
    {
        vf("LOGICGAMEOBJECT_DATA_OFF", false, "read failed");
    }
    if (data && pointer_plausible(data) &&
        read_int(data + LOGICCHARACTERDATA_COLLISION_OFF, &collision))
    {
        vf("LOGICCHARACTERDATA_COLLISION_OFF",
           collision >= 0 && collision <= 100000,
           "v=%d",
           collision);
    }
    else
    {
        vf("LOGICCHARACTERDATA_COLLISION_OFF", false, "read failed");
    }
    if (data && pointer_plausible(data))
    {
        typedef uintptr_t (*name_t)(uintptr_t);
        name_t fn = reinterpret_cast<name_t>(callable(RVA_LOGICDATA_GETNAME));
        uintptr_t text = fn ? fn(data) : 0;
        BOOL ok = NO;
        if (text && pointer_plausible(text))
        {
            ok = read_scstring(text, sizeof(name) - 1, name, sizeof(name)) ? YES
                                                                           : NO;
        }
        for (size_t i = 0; ok && i < sizeof(name) - 1 && name[i]; i++)
        {
            if (name[i] < 0x20 || name[i] > 0x7e) ok = NO;
        }
        vf("RVA_LOGICDATA_GETNAME(x)", ok, "name=%s", ok ? name : "-");
        if (text && pointer_plausible(text))
        {
            int32_t strLen = -1;
            uintptr_t strData = 0;
            BOOL lenOk = read_int(text + SCSTRING_LENGTH_OFF, &strLen);
            BOOL dataOk = read_ptr(text + SCSTRING_DATA_OFF, &strData);
            vf("SCSTRING_LENGTH/DATA_OFF",
               lenOk && dataOk && strLen >= 0 && strLen <= 64 &&
                   strLen == (int32_t)strlen(ok ? name : ""),
               "len=%d data=0x%llx",
               strLen,
               (unsigned long long)strData);
        }
        else
        {
            vs("SCSTRING_LENGTH/DATA_OFF", "no name object");
        }
    }
    else
    {
        vs("RVA_LOGICDATA_GETNAME(x)", "no data pointer");
    }
}

void val_tilemap(uintptr_t clientPtr)
{
    typedef uintptr_t (*map_t)(uintptr_t);
    map_t fn =
        reinterpret_cast<map_t>(callable(RVA_LOGICBATTLEMODECLIENT_GETTILEMAP));
    uintptr_t map = fn ? fn(clientPtr) : 0;
    uintptr_t tiles = 0;
    int32_t width = 0;
    int32_t height = 0;
    vf("RVA_GETTILEMAP(x)",
       map && pointer_plausible(map),
       "0x%llx",
       (unsigned long long)map);
    if (!map || !pointer_plausible(map))
    {
        vs("TILEMAP_WIDTH_OFF", "no tilemap");
        vs("TILEMAP_HEIGHT_OFF", "no tilemap");
        vs("TILEMAP_TILES_OFF", "no tilemap");
        vs("LOGICTILEDATA_FLAGS_OFF", "no tilemap");
        vs("TILETYPEDATA_BLOCKFLAGS_OFF", "no tilemap");
        return;
    }
    if (read_int(map + TILEMAP_WIDTH_OFF, &width))
    {
        vf("TILEMAP_WIDTH_OFF", width > 0 && width <= 120, "w=%d", width);
    }
    else
    {
        vf("TILEMAP_WIDTH_OFF", false, "read failed");
    }
    if (read_int(map + TILEMAP_HEIGHT_OFF, &height))
    {
        vf("TILEMAP_HEIGHT_OFF", height > 0 && height <= 120, "h=%d", height);
    }
    else
    {
        vf("TILEMAP_HEIGHT_OFF", false, "read failed");
    }
    if (!read_ptr(map + TILEMAP_TILES_OFF, &tiles) || !tiles)
    {
        vf("TILEMAP_TILES_OFF", false, "read failed");
        vs("LOGICTILEDATA_FLAGS_OFF", "no tile array");
        vs("TILETYPEDATA_BLOCKFLAGS_OFF", "no tile array");
        return;
    }
    vf("TILEMAP_TILES_OFF",
       pointer_plausible(tiles),
       "0x%llx",
       (unsigned long long)tiles);
    {
        int total = (width > 0 && height > 0 && width * height < 4096)
                        ? width * height
                        : 256;
        int stride = total / 96 + 1;
        int seen = 0;
        int blocked = 0;
        uint32_t sample = 0;
        uint8_t sampleBlock = 0;
        for (int i = 0; i < total; i += stride)
        {
            uintptr_t item = 0;
            uintptr_t kind = 0;
            uint32_t f = 0;
            uint8_t b = 0;
            if (!read_ptr(tiles + (uintptr_t)i * 8, &item) || !item) continue;
            if (!read_ptr(item, &kind) || !kind) continue;
            if (!read_bytes(kind + LOGICTILEDATA_FLAGS_OFF, &f, sizeof(f)))
                continue;
            seen++;
            if (f && !sample) sample = f;
            if (read_bytes(kind + TILETYPEDATA_BLOCKFLAGS_OFF, &b, 1) && b)
            {
                blocked++;
                if (!sampleBlock) sampleBlock = b;
            }
        }
        vf("LOGICTILEDATA_FLAGS_OFF",
           seen > 0 && sample != 0,
           "probed=%d flags=0x%08x",
           seen,
           sample);
        vf("TILETYPEDATA_BLOCKFLAGS_OFF",
           blocked > 0,
           "blocked=%d/%d sample=0x%02x",
           blocked,
           seen,
           (unsigned)sampleBlock);
    }
    vf("TILEMAP_GEOMETRY",
       width > 0 && height > 0 && width == wall_cache_w() &&
           height == wall_cache_h(),
       "%dx%d cache=%dx%d",
       width,
       height,
       wall_cache_w(),
       wall_cache_h());
}

void val_projectile()
{
    uintptr_t found = 0;
    uintptr_t target = base + VTABLE_PROJECTILEDATA_RVA;
    int32_t radius = 0;
    int32_t ttype = 0;
    int32_t startTick = 0;
    int32_t endTick = 0;
    int32_t sx = 0;
    int32_t sy = 0;
    int32_t ex = 0;
    int32_t ey = 0;
    int32_t cfgRadius = 0;
    uint8_t homing = 0;
    uint8_t bouncing = 0;
    uint8_t pierce = 0;
    uint8_t butter = 0;
    uint8_t passes = 0;
    uintptr_t cfg = 0;
    {
        int scanned = 0;
        uintptr_t sampleA = 0;
        uintptr_t sampleB = 0;
        for (int i = 0; i < obj_count; i++)
        {
            uintptr_t dataPtr = 0;
            uintptr_t vtable = 0;
            if (!obj_snapshot[i]) continue;
            if (!read_ptr(obj_snapshot[i] + LOGICGAMEOBJECT_DATA_OFF, &dataPtr) ||
                !dataPtr || !pointer_plausible(dataPtr))
                continue;
            if (!read_ptr(dataPtr, &vtable)) continue;
            scanned++;
            if (vtable == target)
            {
                found = dataPtr;
                break;
            }
            if (!sampleA)
                sampleA = vtable;
            else if (!sampleB && vtable != sampleA)
                sampleB = vtable;
        }
        if (!found && val_proj_tries == 5)
        {
            line("[VAL] proj probe objs=%d vt=0x%llx,0x%llx want=0x%llx\n",
                 scanned,
                 (unsigned long long)sampleA,
                 (unsigned long long)sampleB,
                 (unsigned long long)target);
        }
    }
    if (!found)
    {
        val_proj_tries++;
        if (val_proj_tries >= 120)
        {
            vs("PROJECTILEDATA_*", "no projectile seen in 120s");
            val_proj_done = 1;
        }
        return;
    }
    val_proj_done = 1;
    vf("PROJDATA_BLOCK_SIZE",
       addr_readable(found, PROJDATA_BLOCK_SIZE),
       "0x%llx readable 0x%llx",
       (unsigned long long)found,
       (unsigned long long)PROJDATA_BLOCK_SIZE);
    if (read_int(found + PROJECTILEDATA_RADIUS_OFF, &radius))
    {
        vf("PROJECTILEDATA_RADIUS_OFF",
           radius > 0 && radius <= 300,
           "r=%d",
           radius);
    }
    else
    {
        vf("PROJECTILEDATA_RADIUS_OFF", false, "read failed");
    }
    if (read_int(found + PROJDATA_TTYPE_OFF, &ttype))
    {
        vf("PROJDATA_TTYPE_OFF", ttype >= -1 && ttype <= 64, "tt=%d", ttype);
    }
    else
    {
        vf("PROJDATA_TTYPE_OFF", false, "read failed");
    }
    vf("PROJDATA_HOMING_OFF",
       read_bytes(found + PROJDATA_HOMING_OFF, &homing, 1) && homing <= 1,
       "homing=%u",
       (unsigned)homing);
    vf("PROJECTILEDATA_ISBOUNCING_OFF",
       read_bytes(found + PROJECTILEDATA_ISBOUNCING_OFF, &bouncing, 1) &&
           bouncing <= 1,
       "bounce=%u",
       (unsigned)bouncing);
    vf("PROJECTILEDATA_PIERCES_ENV_OFF",
       read_bytes(found + PROJECTILEDATA_PIERCES_ENV_OFF, &pierce, 1) &&
           pierce <= 1 &&
           read_bytes(
               found + PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF, &butter, 1) &&
           butter <= 1,
       "pierce=%u butter=%u",
       (unsigned)pierce,
       (unsigned)butter);
    vf("PROJECTILEDATA_PASSES_ENV_OFF",
       read_bytes(found + PROJECTILEDATA_PASSES_ENV_OFF, &passes, 1) &&
           passes <= 1,
       "passes=%u",
       (unsigned)passes);
    if (read_int(found + PROJSERVER_STARTTICK_OFF, &startTick) &&
        read_int(found + PROJSERVER_ENDTICK_OFF, &endTick))
    {
        vf("PROJSERVER_START/ENDTICK",
           startTick >= 0 && endTick >= startTick,
           "%d..%d",
           startTick,
           endTick);
    }
    else
    {
        vf("PROJSERVER_START/ENDTICK", false, "read failed");
    }
    if (read_int(found + PROJSERVER_STARTX_OFF, &sx) &&
        read_int(found + PROJSERVER_STARTY_OFF, &sy) &&
        read_int(found + PROJSERVER_ENDX_OFF, &ex) &&
        read_int(found + PROJSERVER_ENDY_OFF, &ey))
    {
        vf("PROJSERVER_COORDS",
           span(sx) && span(sy) && span(ex) && span(ey),
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
    if (read_ptr(found + PROJDATA_CFG_OFF, &cfg) && cfg &&
        pointer_plausible(cfg))
    {
        vf("PROJDATA_CFG_OFF",
           addr_readable(cfg, PROJDATA_CFG_BLOCK_SIZE),
           "0x%llx",
           (unsigned long long)cfg);
        if (read_int(cfg + PROJDATA_RADIUS_OFF, &cfgRadius))
        {
            vf("PROJDATA_RADIUS_OFF",
               cfgRadius >= 0 && cfgRadius <= 4000,
               "r=%d",
               cfgRadius);
        }
        else
        {
            vf("PROJDATA_RADIUS_OFF", false, "read failed");
        }
    }
    else
    {
        vf("PROJDATA_CFG_OFF", false, "read failed");
    }
}

void validate_offsets()
{
    uintptr_t slot = (uintptr_t)read_global_ptr(GAMESTATE_RVA);
    val_ok = 0;
    val_fail = 0;
    val_skip = 0;
    line("\n=== validate ===\n");
    val_rva_table();
    val_globals(slot);
    vf("HEAP_MAX_REGION_SIZE",
       own_elem >= HEAP_MAX_REGION_SIZE,
       "own=0x%llx",
       (unsigned long long)own_elem);
    if (own_elem && pointer_plausible(own_elem)) val_character(own_elem);
    {
        uintptr_t clientPtr = client();
        if (clientPtr && pointer_plausible(clientPtr))
        {
            val_tilemap(clientPtr);
        }
        else
        {
            vs("RVA_GETTILEMAP(x)", "no client");
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
    val_ok = 0;
    val_fail = 0;
    val_skip = 0;
    if (val_input_state == 0)
    {
        uintptr_t clientPtr = client();
        val_input_state = (clientPtr && pointer_plausible(clientPtr))
                              ? val_client_input(clientPtr)
                              : 0;
        if (val_input_state == 0 && ++val_input_tries >= 120)
        {
            char why[128];
            snprintf(why,
                     sizeof(why),
                     "queue not populated in 120s mgr=0x%llx q=0x%llx n=%d",
                     (unsigned long long)val_ci_manager,
                     (unsigned long long)val_ci_queue,
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
    uint64_t now_us = 0;
    unsigned long hz = 0;
    uint64_t wl_avg = 0;
    uint32_t wl_max = 0;
    if (!open_log()) return;
    maybe_validate();
    now_us = us();
    if (base && own_elem && !dead)
    {
        static uint64_t last_fast = 0;
        uint64_t prev = __atomic_load_n(&last_fast, __ATOMIC_RELAXED);
        bool slot = !prev || now_us - prev >= 50000;
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
            int n =
                snprintf(buf,
                         sizeof(buf),
                         "[f] t=%llu p=%d,%d v=%d d=%.3f,%.3f tgt=%d,%d n=%d "
                         "h=%d u=%d hm=%d",
                         (unsigned long long)(now_us / 1000),
                         own_x,
                         own_y,
                         (int)(own_speed + 0.5f),
                         ad_dir_x,
                         ad_dir_y,
                         ad_target_x,
                         ad_target_y,
                         ad::threat_n,
                         ad_hold_ms,
                         ad_urgent,
                         ad_human);
            for (int i = 0;
                 i < ad::threat_n && n > 0 && n < (int)sizeof(buf) - 48;
                 i++)
            {
                n += snprintf(buf + n,
                              sizeof(buf) - (size_t)n,
                              ";%d,%d,%d,%d,%d",
                              (int)ad::threats[i].x,
                              (int)ad::threats[i].y,
                              (int)ad::threats[i].vx,
                              (int)ad::threats[i].vy,
                              (int)ad::threats[i].rad);
            }
            if (n > 0 && n < (int)sizeof(buf) - 2)
            {
                buf[n++] = '\n';
                buf[n] = '\0';
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
        match_open = 1;
        dodge_count = 0;
        hit_count = 0;
        last_hp = -1;
        last_tx = 1.0f;
        last_ty = 0.0f;
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
        static uint64_t prev_ms = 0;
        uint64_t now_ms = now_us / 1000;
        uint32_t ticks = workload_ticks - prev_ticks;
        uint64_t dt_ms = (prev_ms && now_ms > prev_ms) ? now_ms - prev_ms : 0;
        hz = dt_ms ? static_cast<unsigned long>(
                         (static_cast<uint64_t>(ticks) * 1000ULL) / dt_ms)
                   : 0UL;
        prev_ticks = workload_ticks;
        prev_ms = now_ms;
        wl_avg = workload_us_n ? workload_us_sum / workload_us_n : 0;
        wl_max = workload_us_max;
        workload_us_sum = 0;
        workload_us_max = 0;
        workload_us_n = 0;
    }
    int32_t own_aim = 0;
    if (own_elem)
    {
        read_int(own_elem + LOGICCHARACTER_ATTACK_ANGLE_OFF, &own_aim);
    }
    line("[m] hz=%lu wl=%lu/%lu objs=%d ch=%d pr=%d en=%d hp=%u p=(%d,%d) v=%d "
         "dir=%s map=%dx%d dodge=%d hit=%d hb=%d ind=%d fd=%d tgt=(%d,%d) "
         "hu=%d pm=%u/%u wb=%d/%d pt=%d wp=%d\n",
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
         (int)(own_speed + 0.5f),
         active ? dir_name(ad_best_dir) : "idle",
         wall_cache_w(),
         wall_cache_h(),
         dodge_count,
         hit_count,
         (int)(ad::own_hitbox + 0.5f),
         ad_ind_n,
         own_aim,
         ad_target_x,
         ad_target_y,
         ad_human,
         ad_proj_move,
         ad_proj_sample,
         wall_blocked_count(),
         wall_cache_w() * wall_cache_h(),
         ad_pass_own,
         wall_engine_blocked_count());
    ad_proj_move = 0;
    ad_proj_sample = 0;
    {
        char qbuf[192];
        int qn = snprintf(qbuf, sizeof(qbuf), "qc=");
        for (int i = 0; i < 16 && qn > 0 && qn < (int)sizeof(qbuf) - 8; i++)
        {
            qn += snprintf(qbuf + qn,
                           sizeof(qbuf) - (size_t)qn,
                           i ? ",%u" : "%u",
                           (unsigned)ad_qc[i]);
            ad_qc[i] = 0;
        }
        if (qn > 0 && qn < (int)sizeof(qbuf) - 2)
        {
            qbuf[qn++] = '\n';
            qbuf[qn] = '\0';
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
    if (ad_hit_n >= 0 && ad_hit_dmg > 0)
    {
        char buf[256];
        int len = snprintf(buf,
                           sizeof(buf),
                           "dmg=%d act=%d n=%d p=(%d,%d)",
                           ad_hit_dmg,
                           ad_hit_act,
                           ad_hit_n,
                           ad_hit_x,
                           ad_hit_y);
        if (ad_hit_picked < 1 && len > 0)
        {
            const char* why = ad_hit_n ? " | unresolved" : " | env";
            snprintf(buf + len, sizeof(buf) - (size_t)len, "%s", why);
        }
        for (int i = 0; i < ad_hit_picked && i < AD_REPORT_MAX; i++)
        {
            if (len <= 0 || len >= (int)sizeof(buf) - 40) break;
            len += snprintf(buf + len,
                            sizeof(buf) - (size_t)len,
                            " | ca=%.0f ttc=%.2f r=%.0f",
                            ad_hit_ca[i],
                            ad_hit_ttc[i],
                            ad_hit_r[i]);
        }
        line("[hit] %s\n", buf);
        ad_hit_dmg = 0;
        ad_hit_picked = 0;
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
