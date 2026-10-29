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
constexpr uint64_t  REPORT_US     = 1000000;
constexpr uint64_t  FAST_TRACE_US = 50000;
constexpr uint64_t  IDLE_TRACE_US = 500000;

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
        {"BATTLEMODE__GETINSTANCE", BATTLEMODE__GETINSTANCE},
        {"LOGICBATTLEMODECLIENT__GETTILEMAP",
         LOGICBATTLEMODECLIENT__GETTILEMAP},
        {"LOGICBATTLEMODECLIENT__GETOWNCHARACTER",
         LOGICBATTLEMODECLIENT__GETOWNCHARACTER},
        {"LOGICBATTLEMODECLIENT__GETOWNPLAYERINDEX",
         LOGICBATTLEMODECLIENT__GETOWNPLAYERINDEX},
        {"LOGICBATTLEMODECLIENT__GETOWNPLAYERTEAM",
         LOGICBATTLEMODECLIENT__GETOWNPLAYERTEAM},
        {"LOGICDATA__GETNAME", LOGICDATA__GETNAME},
        {"LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO",
         LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO},
        {"CLIENTINPUTMESSAGE__CONSTRUCTOR", CLIENTINPUTMESSAGE__CONSTRUCTOR},
        {"CLIENTINPUTMANAGER__ADDINPUT", CLIENTINPUTMANAGER__ADDINPUT},
        {"LOGICTILEMAP__ISPASSABLEPATHFINDER",
         LOGICTILEMAP__ISPASSABLEPATHFINDER},
        {"LOGICTILEMAP__ISPLAYERLINEOFSIGHTCLEAR",
         LOGICTILEMAP__ISPLAYERLINEOFSIGHTCLEAR},
        {"LOGICTILEMAP__TILETOLOGIC", LOGICTILEMAP__TILETOLOGIC},
        {"LOGICTILEMAP__LOGICTOTILE", LOGICTILEMAP__LOGICTOTILE},
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
        vf("LOGICTILEMAP__TILETOLOGIC",
           center == 3 * 300 + 150 && game_logic_to_tile(center) == 3,
           "3 -> %d (want 1050) back=%d",
           center,
           game_logic_to_tile(center));
    }
    vf("LOGICTILEMAP__LOGICTOTILE",
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
    vf("GAMESTATEMANAGER__INSTANCE",
       slot && pointer_plausible(slot),
       "0x%llx",
       (unsigned long long) slot);
    if (!slot)
    {
        vs("GAMESTATEMANAGER__STATE", "no game state");
        vs("GAMESTATEMANAGER__CURRENTSTATE", "no game state");
        return;
    }
    if (read_int(slot + GAMESTATEMANAGER__STATE, &state))
    {
        vf("GAMESTATEMANAGER__STATE",
           state >= 0 && state <= 8,
           "state=%d",
           state);
    }
    else
    {
        vf("GAMESTATEMANAGER__STATE", false, "read failed");
    }
    if (read_ptr(slot + GAMESTATEMANAGER__CURRENTSTATE, &scene))
    {
        vf("GAMESTATEMANAGER__CURRENTSTATE",
           scene && pointer_plausible(scene),
           "0x%llx",
           (unsigned long long) scene);
    }
    else
    {
        vf("GAMESTATEMANAGER__CURRENTSTATE", false, "read failed");
    }
    if (!scene)
    {
        vs("GAMESTATEMANAGER__CLIENT", "no scene");
        vs("LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER", "no scene");
        vs("LOGICBATTLEMODECLIENT__INPUTMANAGER", "no scene");
    }
    else
    {
        if (read_ptr(scene + GAMESTATEMANAGER__CLIENT, &clientPtr))
        {
            vf("GAMESTATEMANAGER__CLIENT",
               clientPtr && pointer_plausible(clientPtr),
               "0x%llx",
               (unsigned long long) clientPtr);
        }
        else
        {
            vf("GAMESTATEMANAGER__CLIENT", false, "read failed");
        }
        if (!clientPtr)
        {
            vs("LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER", "no client");
            vs("LOGICBATTLEMODECLIENT__INPUTMANAGER", "no client");
        }
        else
        {
            if (read_ptr(clientPtr + LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER,
                         &get_client_input_manager))
            {
                vf("LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER",
                   get_client_input_manager &&
                       pointer_plausible(get_client_input_manager),
                   "0x%llx",
                   (unsigned long long) get_client_input_manager);
            }
            else
            {
                vf("LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER",
                   false,
                   "read failed");
            }
            if (get_client_input_manager &&
                read_int(get_client_input_manager +
                             LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT,
                         &count))
            {
                vf("LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT",
                   count > 0 && count <= OBJECT_COUNT_MAX,
                   "count=%d",
                   count);
            }
            else
            {
                vf("LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT",
                   false,
                   "read failed");
            }
            if (get_client_input_manager &&
                read_ptr(get_client_input_manager +
                             LOGICGAMEOBJECTMANAGER__GAMEOBJECTS,
                         &array))
            {
                vf("LOGICGAMEOBJECTMANAGER__GAMEOBJECTS",
                   array && pointer_plausible(array),
                   "0x%llx",
                   (unsigned long long) array);
            }
            else
            {
                vf("LOGICGAMEOBJECTMANAGER__GAMEOBJECTS", false, "read failed");
            }
            val_input_state = val_client_input(slot);
        }
    }
    if (read_ptr(base + LOGICPROJECTILEDATA__VTABLE, &vtable))
    {
        vf("LOGICPROJECTILEDATA__VTABLE",
           vtable && addr_executable(vtable),
           "asset anchor slot0=0x%llx",
           (unsigned long long) vtable);
    }
    else
    {
        vf("LOGICPROJECTILEDATA__VTABLE", false, "read failed");
    }
    vf("CLIENTINPUT__TYPETABLE",
       addr_readable(base + CLIENTINPUT__TYPETABLE, 8),
       "0x%llx",
       (unsigned long long) CLIENTINPUT__TYPETABLE);
    vf("CLIENTINPUT__TYPEFALLBACK",
       addr_readable(base + CLIENTINPUT__TYPEFALLBACK, 8),
       "0x%llx",
       (unsigned long long) CLIENTINPUT__TYPEFALLBACK);
    vf("CLIENTINPUT_HASH_MASK_RVA",
       addr_readable(base + CLIENTINPUTMESSAGE__HASHINNERMASK, 8) &&
           addr_readable(base + CLIENTINPUTMESSAGE__HASHOUTERMASK, 8),
       "inner=0x%llx outer=0x%llx",
       (unsigned long long) CLIENTINPUTMESSAGE__HASHINNERMASK,
       (unsigned long long) CLIENTINPUTMESSAGE__HASHOUTERMASK);
}

int val_client_input(uintptr_t clientPtr)
{
    uintptr_t get_client_input_manager = 0;
    uintptr_t queue                    = 0;
    if (!read_ptr(clientPtr + LOGICBATTLEMODECLIENT__INPUTMANAGER,
                  &get_client_input_manager) ||
        !get_client_input_manager ||
        !pointer_plausible(get_client_input_manager))
    {
        return 0;
    }
    val_ci_manager = get_client_input_manager;
    if (!read_ptr(get_client_input_manager + CLIENTINPUTMANAGER__INPUTQUEUE,
                  &queue) ||
        !queue || !pointer_plausible(queue))
    {
        return 0;
    }
    val_ci_queue = queue;
    vf("LOGICBATTLEMODECLIENT__INPUTMANAGER",
       true,
       "0x%llx",
       (unsigned long long) get_client_input_manager);
    vf("CLIENTINPUTMANAGER__INPUTQUEUE",
       true,
       "0x%llx",
       (unsigned long long) queue);
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
    vf("LOGICCHARACTER__GLOBALID",
       read_int(own + LOGICCHARACTER__GLOBALID, &pid) && pid > 0,
       "pid=%d",
       pid);
    if (read_int(own + LOGICCHARACTER__X, &x) &&
        read_int(own + LOGICCHARACTER__Y, &y))
    {
        vf("LOGICCHARACTER_X/Y_OFF", span(x) && span(y), "x=%d y=%d", x, y);
    }
    else
    {
        vf("LOGICCHARACTER_X/Y_OFF", false, "read failed");
    }
    if (read_int(own + LOGICCHARACTER__TEAM, &team))
    {
        vf("LOGICCHARACTER__TEAM", team >= 0 && team <= 3, "team=%d", team);
    }
    else
    {
        vf("LOGICCHARACTER__TEAM", false, "read failed");
    }
    if (read_int(own + LOGICCHARACTER__ATTACKANGLE, &angle))
    {
        vf("LOGICCHARACTER__ATTACKANGLE",
           angle >= -720 && angle <= 720,
           "angle=%d",
           angle);
    }
    else
    {
        vf("LOGICCHARACTER__ATTACKANGLE", false, "read failed");
    }
    if (read_int(own + LOGICCHARACTER__HITPOINTS, &hp))
    {
        vf("LOGICCHARACTER__HITPOINTS",
           hp >= 0 && hp <= 20000 && (uint32_t) hp == own_hp,
           "hp=%d scan=%u",
           hp,
           own_hp);
    }
    else
    {
        vf("LOGICCHARACTER__HITPOINTS", false, "read failed");
    }
    if (read_ptr(own + LOGICGAMEOBJECT__DATA, &data))
    {
        vf("LOGICGAMEOBJECT__DATA",
           data && pointer_plausible(data),
           "0x%llx",
           (unsigned long long) data);
    }
    else
    {
        vf("LOGICGAMEOBJECT__DATA", false, "read failed");
    }
    if (data && pointer_plausible(data) &&
        read_int(data + LOGICCHARACTER__DATACOLLISIONRADIUS, &collision))
    {
        vf("LOGICCHARACTER__DATACOLLISIONRADIUS",
           collision >= 0 && collision <= 100000,
           "v=%d",
           collision);
    }
    else
    {
        vf("LOGICCHARACTER__DATACOLLISIONRADIUS", false, "read failed");
    }
    if (data && pointer_plausible(data))
    {
        typedef uintptr_t (*name_t)(uintptr_t);
        name_t    fn   = reinterpret_cast<name_t>(callable(LOGICDATA__GETNAME));
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
        vf("LOGICDATA__GETNAME(x)", ok, "name=%s", ok ? name : "-");
        if (text && pointer_plausible(text))
        {
            int32_t   strLen  = -1;
            uintptr_t strData = 0;
            BOOL      lenOk   = read_int(text + SCSTRING__BYTELENGTH, &strLen);
            BOOL      dataOk  = read_ptr(text + SCSTRING__CHARACTERS, &strData);
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
        vs("LOGICDATA__GETNAME(x)", "no data pointer");
    }
}

void val_tilemap(uintptr_t clientPtr)
{
    typedef uintptr_t (*map_t)(uintptr_t);
    map_t fn =
        reinterpret_cast<map_t>(callable(LOGICBATTLEMODECLIENT__GETTILEMAP));
    uintptr_t map    = fn ? fn(clientPtr) : 0;
    uintptr_t tiles  = 0;
    int32_t   width  = 0;
    int32_t   height = 0;
    vf("LOGICBATTLEMODECLIENT__GETTILEMAP",
       map && pointer_plausible(map),
       "0x%llx",
       (unsigned long long) map);
    if (!map || !pointer_plausible(map))
    {
        vs("LOGICTILEMAP__WIDTH", "no tilemap");
        vs("LOGICTILEMAP__HEIGHT", "no tilemap");
        vs("LOGICTILEMAP__TILEARRAY", "no tilemap");
        vs("LOGICTILE__PATHFINDERMASK", "no tilemap");
        vs("LOGICTILE__WATERPATHFINDERMASK", "no tilemap");
        return;
    }
    if (read_int(map + LOGICTILEMAP__WIDTH, &width))
    {
        vf("LOGICTILEMAP__WIDTH", width > 0 && width <= 120, "w=%d", width);
    }
    else
    {
        vf("LOGICTILEMAP__WIDTH", false, "read failed");
    }
    if (read_int(map + LOGICTILEMAP__HEIGHT, &height))
    {
        vf("LOGICTILEMAP__HEIGHT", height > 0 && height <= 120, "h=%d", height);
    }
    else
    {
        vf("LOGICTILEMAP__HEIGHT", false, "read failed");
    }
    if (!read_ptr(map + LOGICTILEMAP__TILEARRAY, &tiles) || !tiles)
    {
        vf("LOGICTILEMAP__TILEARRAY", false, "read failed");
        vs("LOGICTILE__PATHFINDERMASK", "no tile array");
        vs("LOGICTILE__WATERPATHFINDERMASK", "no tile array");
        return;
    }
    vf("LOGICTILEMAP__TILEARRAY",
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
            if (!read_bytes(
                    item + LOGICTILE__PATHFINDERMASK, &mask, sizeof(mask)) ||
                !read_bytes(item + LOGICTILE__WATERPATHFINDERMASK,
                            &water,
                            sizeof(water)) ||
                !read_bytes(
                    item + LOGICTILE__HARDBLOCKMASK, &hard, sizeof(hard)))
                continue;
            seen++;
            if (mask && !sample) sample = mask;
            if ((mask | water | hard) & 0x1ff) blocked++;
        }
        vf("LOGICTILE__PATHFINDERMASK",
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
    uintptr_t found  = 0;
    uintptr_t objPtr = 0;
    {
        float px = 0.0f, py = 0.0f;
        if (!ad::find_live_projectile(&objPtr, &found, &px, &py))
        {
            objPtr = 0;
        }
        else if (val_proj_tries == 5)
        {
            line("[VAL] proj probe objs=%d shot=%d p=(%.0f,%.0f)\n",
                 obj_count,
                 (int) ad_proj_n,
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
    val_proj_done = 1;
    if (!found)
    {
        vs("PROJECTILEDATA_*", "shot exposes no data-asset pointer");
        return;
    }
    int32_t speed = 0;
    if (read_int(found + LOGICPROJECTILEDATA__SPEED, &speed))
    {
        vf("LOGICPROJECTILEDATA__SPEED",
           speed > 50 && speed < 20000,
           "speed=%d",
           speed);
    }
    else
    {
        vf("LOGICPROJECTILEDATA__SPEED", false, "read failed");
    }
}

void validate_offsets()
{
    uintptr_t slot = (uintptr_t) read_global_ptr(GAMESTATEMANAGER__INSTANCE);
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
            vs("LOGICBATTLEMODECLIENT__GETTILEMAP", "no client");
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
        val_probe_next = now_us + 500000;
        return;
    }
    if (val_input_state && val_proj_done) return;
    if (val_probe_next && now_us < val_probe_next) return;
    val_probe_next = now_us + 1000000;
    if (!base || !own_elem || dead) return;
    val_ok   = 0;
    val_fail = 0;
    val_skip = 0;
    if (val_input_state == 0)
    {
        uintptr_t battle =
            (uintptr_t) read_global_ptr(GAMESTATEMANAGER__INSTANCE);
        val_input_state = (battle && pointer_plausible(battle))
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
                static char last_state[2048];
                const char* state = strstr(buf, " p=");
                buf[n++]          = '\n';
                buf[n]            = '\0';
                if (!state || strcmp(last_state, state) != 0)
                {
                    if (state)
                    {
                        snprintf(last_state, sizeof(last_state), "%s", state);
                    }
                    line("%s", buf);
                }
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
                         (static_cast<uint64_t>(ticks) * 1000) / dt_ms)
                                : 0;
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
        read_int(own_elem + LOGICCHARACTER__ATTACKANGLE, &own_aim);
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
        int32_t tick_start = 0;
        int32_t tick_end   = 0;
        float   engine_x   = 0.0f;
        float   engine_y   = 0.0f;
        int     path_len   = engine_own_path_length();
        int     window     = engine_own_tick_window(&tick_start, &tick_end);
        int     forecast   = 0;
        if (window && tick_end > tick_start)
        {
            forecast = engine_own_pos_at_tick(tick_end, &engine_x, &engine_y);
        }
        line("[eng] plen=%d win=%d..%d fc=%d e=(%.0f,%.0f) pf=%d/%d sk=%d "
             "tgt=%d,%d\n",
             path_len,
             window ? (int) tick_start : 0,
             window ? (int) tick_end : 0,
             forecast,
             engine_x,
             engine_y,
             engine_pathfind_ok,
             engine_pathfind_calls,
             engine_pathfind_skip,
             (int) engine_pathfind_x,
             (int) engine_pathfind_y);
        engine_pathfind_ok    = 0;
        engine_pathfind_calls = 0;
        engine_pathfind_skip  = 0;
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
        if (!last_env_us || now_us - last_env_us >= 2000000)
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
        line("[p%d] pid=%d p=(%.0f,%.0f) v=(%.0f,%.0f) sp=%.0f "
             "r=%.0f vel=%d\n",
             i,
             ad_pj_pid[i],
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
