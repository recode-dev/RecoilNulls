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
#include <fcntl.h>
#include <unistd.h>

namespace
{
constexpr long long LOG_MAX_BYTES = 4000000;
constexpr uint64_t  REPORT_US     = 1000000;
constexpr uint64_t  FAST_TRACE_US = 50000;
constexpr uint64_t  IDLE_TRACE_US = 500000;

const char* target_text(char* out, size_t cap)
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
int      last_ang       = 0;
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
        fprintf(log_file, "=== recoil log ===\n");
        fprintf(log_file, "documents: %s\n", dir);
        fflush(log_file);
    }
    return true;
}

struct code_entry
{
    const char* name;
    uintptr_t   rva;
};

constexpr code_entry kCodeOffsetTable[] = {
    {"BATTLEMODE__GETINSTANCE", BATTLEMODE__GETINSTANCE},
    {"LOGICDATA__GETNAME", LOGICDATA__GETNAME},
    {"LOGICBATTLEMODECLIENT__GETTILEMAP", LOGICBATTLEMODECLIENT__GETTILEMAP},
    {"LOGICBATTLEMODECLIENT__GETOWNCHARACTER",
     LOGICBATTLEMODECLIENT__GETOWNCHARACTER},
    {"LOGICBATTLEMODECLIENT__GETOWNPLAYERTEAM",
     LOGICBATTLEMODECLIENT__GETOWNPLAYERTEAM},
    {"LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO",
     LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO},
    {"CLIENTINPUTMESSAGE__CONSTRUCTOR", CLIENTINPUTMESSAGE__CONSTRUCTOR},
    {"CLIENTINPUTMANAGER__ADDINPUT", CLIENTINPUTMANAGER__ADDINPUT},
    {"LOGICTILEMAP__ISPASSABLEPATHFINDER", LOGICTILEMAP__ISPASSABLEPATHFINDER},
    {"LOGICTILEMAP__TILETOLOGIC", LOGICTILEMAP__TILETOLOGIC},
    {"LOGICTILEMAP__LOGICTOTILE", LOGICTILEMAP__LOGICTOTILE},
    {"LOGICTILEMAP__ISPLAYERLINEOFSIGHT", LOGICTILEMAP__ISPLAYERLINEOFSIGHT},
    {"LOGICMATH__SQRT", LOGICMATH__SQRT},
    {"LOGICMATH__GETDISTANCESQUARETOLINE", LOGICMATH__GETDISTANCESQUARETOLINE},
    {"LOGICMATH__NORMALIZEANGLE180", LOGICMATH__NORMALIZEANGLE180},
    {"LOGICPROJECTILEDATA__GETRADIUS", LOGICPROJECTILEDATA__GETRADIUS},
    {"LOGICPROJECTILEDATA__GETSPEED", LOGICPROJECTILEDATA__GETSPEED},
    {"LOGICCHARACTER__GETCOLLISIONRADIUS", LOGICCHARACTER__GETCOLLISIONRADIUS},
    {"LOGICGAMEOBJECTCLIENT__GETX", LOGICGAMEOBJECTCLIENT__GETX},
    {"LOGICGAMEOBJECTCLIENT__GETY", LOGICGAMEOBJECTCLIENT__GETY},
    {"LOGICVECTOR2__GETANGLE", LOGICVECTOR2__GETANGLE},
    {"LOGICVECTOR2__NORMALIZE", LOGICVECTOR2__NORMALIZE},
    {"LOGICVECTOR2__ROTATE", LOGICVECTOR2__ROTATE},
    {"LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN",
     LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN},
    {"LOGICGAMEPLAYUTIL__GETDISTANCESQUAREDBETWEEN",
     LOGICGAMEPLAYUTIL__GETDISTANCESQUAREDBETWEEN},
    {"LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE",
     LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE},
    {"LOGICPROJECTILECLIENT__GETTRAVELTICKS",
     LOGICPROJECTILECLIENT__GETTRAVELTICKS},
};

struct field_entry
{
    const char* name;
    uintptr_t   offset;
    size_t      width;
};

constexpr field_entry kCharacterFieldTable[] = {
    {"LOGICGAMEOBJECT__DATA", LOGICGAMEOBJECT__DATA, sizeof(uintptr_t)},
    {"LOGICCHARACTER__GLOBALID", LOGICCHARACTER__GLOBALID, sizeof(int32_t)},
    {"LOGICCHARACTER__X", LOGICCHARACTER__X, sizeof(int32_t)},
    {"LOGICCHARACTER__Y", LOGICCHARACTER__Y, sizeof(int32_t)},
    {"LOGICCHARACTER__ATTACKANGLE",
     LOGICCHARACTER__ATTACKANGLE,
     sizeof(int32_t)},
    {"LOGICCHARACTER__HITPOINTS", LOGICCHARACTER__HITPOINTS, sizeof(int32_t)},
};

int val_ok   = 0;
int val_fail = 0;
int val_done = 0;

void check(bool pass, const char* name, const char* detail)
{
    if (pass)
    {
        val_ok++;
        line("[VAL] %-48s ok    %s\n", name, detail);
    }
    else
    {
        val_fail++;
        line("[VAL] %-48s FAIL  %s\n", name, detail);
    }
}

void validate_code_offsets()
{
    char detail[96];
    for (const code_entry& entry : kCodeOffsetTable)
    {
        uintptr_t address = callable_at_rva(entry.rva);
        bool      ok      = address != 0 && addr_executable(address) &&
                  addr_readable(address, sizeof(uint32_t));
        snprintf(detail,
                 sizeof(detail),
                 "rva=0x%llx addr=0x%llx",
                 (unsigned long long) entry.rva,
                 (unsigned long long) address);
        check(ok, entry.name, detail);
    }
}

void validate_character_fields(uintptr_t own)
{
    char detail[128];
    for (const field_entry& entry : kCharacterFieldTable)
    {
        uintptr_t value = 0;
        bool      ok    = read_bytes(own + entry.offset, &value, entry.width);
        snprintf(detail,
                 sizeof(detail),
                 "off=0x%llx value=0x%llx",
                 (unsigned long long) entry.offset,
                 (unsigned long long) value);
        check(ok, entry.name, detail);
    }
    int32_t x       = 0;
    int32_t y       = 0;
    int32_t hp      = 0;
    int32_t team    = 0;
    bool    span_ok = read_int(own + LOGICCHARACTER__X, &x) &&
                   read_int(own + LOGICCHARACTER__Y, &y) && x >= -3000 &&
                   x <= 40000 && y >= -3000 && y <= 40000;
    snprintf(detail, sizeof(detail), "x=%d y=%d", x, y);
    check(span_ok, "LOGICCHARACTER__X/Y span", detail);
    bool hp_ok = read_int(own + LOGICCHARACTER__HITPOINTS, &hp) && hp >= 0 &&
                 hp <= 1000000;
    snprintf(detail, sizeof(detail), "hp=%d", hp);
    check(hp_ok, "LOGICCHARACTER__HITPOINTS range", detail);
    uintptr_t data    = 0;
    bool      team_ok = read_ptr(own + LOGICGAMEOBJECT__DATA, &data) &&
                   read_int(data + LOGICGAMEOBJECTDATA__TEAM, &team) &&
                   team >= 0 && team <= 64;
    snprintf(detail,
             sizeof(detail),
             "data=0x%llx team=%d",
             (unsigned long long) data,
             team);
    check(team_ok, "LOGICGAMEOBJECTDATA__TEAM range", detail);
}

void validate_tilemap_fields(uintptr_t clientPtr)
{
    char      detail[128];
    uintptr_t tileMap = 0;
    uintptr_t tileFn  = callable(LOGICBATTLEMODECLIENT__GETTILEMAP);
    if (!tileFn) return;
    tileMap     = reinterpret_cast<uintptr_t (*)(uintptr_t)>(tileFn)(clientPtr);
    bool map_ok = pointer_plausible(tileMap);
    snprintf(
        detail, sizeof(detail), "map=0x%llx", (unsigned long long) tileMap);
    check(map_ok, "LOGICBATTLEMODECLIENT__GETTILEMAP", detail);
    if (!map_ok) return;
    int32_t width  = 0;
    int32_t height = 0;
    read_int(tileMap + LOGICTILEMAP__WIDTH, &width);
    read_int(tileMap + LOGICTILEMAP__HEIGHT, &height);
    snprintf(detail, sizeof(detail), "w=%d h=%d", width, height);
    check(width > 0 && width <= 200 && height > 0 && height <= 200,
          "LOGICTILEMAP__WIDTH/HEIGHT",
          detail);
    uintptr_t tiles = 0;
    read_ptr(tileMap + LOGICTILEMAP__TILEARRAY, &tiles);
    snprintf(
        detail, sizeof(detail), "tiles=0x%llx", (unsigned long long) tiles);
    check(pointer_plausible(tiles), "LOGICTILEMAP__TILEARRAY", detail);
}

void validate_globals()
{
    char      detail[128];
    uintptr_t slot = (uintptr_t) read_global_ptr(GAMESTATEMANAGER__INSTANCE);
    snprintf(detail, sizeof(detail), "slot=0x%llx", (unsigned long long) slot);
    check(pointer_plausible(slot), "GAMESTATEMANAGER__INSTANCE", detail);
    if (!pointer_plausible(slot)) return;
    int32_t   state   = -1;
    uintptr_t current = 0;
    read_int(slot + GAMESTATEMANAGER__STATE, &state);
    read_ptr(slot + GAMESTATEMANAGER__CURRENTSTATE, &current);
    snprintf(detail,
             sizeof(detail),
             "state=%d current=0x%llx",
             state,
             (unsigned long long) current);
    check(state >= 0 && state < 32 && pointer_plausible(current),
          "GAMESTATEMANAGER__STATE/CURRENTSTATE",
          detail);
}

void validate_projectile_speed()
{
    uintptr_t objOut  = 0;
    uintptr_t dataOut = 0;
    float     px      = 0.0f;
    float     py      = 0.0f;
    int32_t   speed   = 0;
    char      detail[128];
    if (!ad::find_live_projectile(&objOut, &dataOut, &px, &py)) return;
    if (!dataOut) return;
    speed = projectile_speed(dataOut);
    snprintf(detail, sizeof(detail), "speed=%d", speed);
    check(speed > 0 && speed < 20000, "LOGICPROJECTILEDATA__GETSPEED", detail);
}
}  // namespace

void validate_offsets()
{
    val_ok   = 0;
    val_fail = 0;
    line("\n=== validate ===\n");
    validate_code_offsets();
    validate_globals();
    if (own_elem && pointer_plausible(own_elem))
    {
        validate_character_fields(own_elem);
    }
    {
        uintptr_t clientPtr = client();
        if (clientPtr && pointer_plausible(clientPtr))
        {
            validate_tilemap_fields(clientPtr);
        }
    }
    validate_projectile_speed();
    line("[VAL] summary ok=%d fail=%d\n", val_ok, val_fail);
}

void maybe_validate()
{
    if (val_done) return;
    if (!base || !own_elem || dead) return;
    if (!ad_proj_n) return;
    validate_offsets();
    val_done = 1;
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
        last_ang    = 0;
    }
    if (last_hp > 0 && static_cast<int32_t>(own_hp) < last_hp) hit_count++;
    last_hp = static_cast<int32_t>(own_hp);
    {
        int ang   = ad_dir_angle_deg(ad_dir_x, ad_dir_y);
        int delta = ad_angle_delta(ang, last_ang);
        if (active && (delta > 20 || delta < -20))
        {
            dodge_count++;
        }
        last_ang = active ? ang : 0;
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
         "hu=%d pm=%u/%u pt=%d wu=%d\n",
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
         ad_pass_own,
         ad_windup_n);
    ad_proj_move   = 0;
    ad_proj_sample = 0;
    for (int i = 0; i < ad_threat_n && i < AD_REPORT_MAX; i++)
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
            snprintf(buf + len, sizeof(buf) - (size_t) len, " | unresolved");
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
        line("[p%d] pid=%d p=(%.0f,%.0f) v=(%.0f,%.0f) sp=%.0f r=%.0f vel=%d\n",
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
    for (int i = 0; i < ad_enemy_n && i < AD_ENEMY_MAX; i++)
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
