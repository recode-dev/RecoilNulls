#include "../core/scan.h"
#include "../core/commands.h"
#include "../features/autododge.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
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
int32_t last_tx = INT32_MAX;
int32_t last_ty = INT32_MAX;
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
    if (!open_log()) return;
    now_us = us();
    if (base && own_elem && !dead)
    {
        // The hook runs on more than one thread, so a plain `if` let both of
        // them through and the dense rows landed 6-10 ms apart instead of
        // every 50 ms. Reserving the slot with a compare-exchange keeps the
        // cadence and halves the log size.
        static uint64_t last_fast = 0;
        uint64_t prev = __atomic_load_n(&last_fast, __ATOMIC_RELAXED);
        bool slot = !prev || now_us - prev >= 50000;
        if (slot &&
            !__atomic_compare_exchange_n(
                &last_fast, &prev, now_us, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
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
        last_tx = INT32_MAX;
        last_ty = INT32_MAX;
    }
    if (last_hp > 0 && static_cast<int32_t>(own_hp) < last_hp) hit_count++;
    last_hp = static_cast<int32_t>(own_hp);
    if (active && (ad_target_x != last_tx || ad_target_y != last_ty))
    {
        dodge_count++;
        last_tx = ad_target_x;
        last_ty = ad_target_y;
    }
    if (!active)
    {
        last_tx = INT32_MAX;
        last_ty = INT32_MAX;
    }
    {
        static uint32_t prev_ticks = 0;
        hz = workload_ticks - prev_ticks;
        prev_ticks = workload_ticks;
        // a counter reset between matches showed up as hz=1697 in the log
        if (hz > 300) hz = 300;
    }
    int32_t own_aim = 0;
    if (own_elem)
    {
        read_int(own_elem + OBJ_ATTACK_ANGLE_OFF, &own_aim);
    }
    line("[m] hz=%lu objs=%d ch=%d pr=%d en=%d hp=%u p=(%d,%d) v=%d dir=%s "
         "map=%dx%d dodge=%d hit=%d beam=%d ind=%d fd=%d tgt=(%d,%d) hu=%d\n",
         static_cast<unsigned long>(hz),
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
         ad_beam_n,
         ad_ind_n,
         own_aim,
         ad_target_x,
         ad_target_y,
         ad_human);
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
        // "none" with act=0 is not a dodge failure - it is damage with no
        // projectile and no live dodge (zone, fall, storm). Tag it so the hit
        // count is not read as missed dodges.
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
