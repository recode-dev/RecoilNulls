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
float last_tx = 1.0f; // last reported heading, not a target point
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
    now_us = us();
    if (base && own_elem && !dead)
    {
        // `last_fast` used to be read here and never written, so `!last_fast`
        // stayed true and every frame got a row - the log was 120 Hz, not the
        // intended 20 Hz, which is why rows sit 8 ms apart and the 50 ms grid
        // looked like 8 ms frames. The compare-exchange writes the slot back.
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
    // The target point is recomputed from the current position on every send,
    // so with a per-frame send its coordinates change every frame. Count
    // heading changes instead, otherwise dodge=N turns into a 120 Hz frame
    // counter and stops meaning "how many dodges".
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
        // hz used to be the raw tick delta since the previous report. When the
        // report is held back (idle, between matches) that delta covers many
        // seconds and printed as hz=1697. Divide by the real elapsed time so
        // the column is a rate and stays readable.
        static uint32_t prev_ticks = 0;
        static uint64_t prev_ms = 0;
        uint64_t now_ms = now_us / 1000;
        uint32_t ticks = workload_ticks - prev_ticks;
        uint64_t dt_ms = (prev_ms && now_ms > prev_ms) ? now_ms - prev_ms : 0;
        hz = dt_ms ? static_cast<unsigned long>(
                         (static_cast<uint64_t>(ticks) * 1000ULL) / dt_ms)
                   : static_cast<unsigned long>(ticks);
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
        read_int(own_elem + OBJ_ATTACK_ANGLE_OFF, &own_aim);
    }
    line("[m] hz=%lu wl=%lu/%lu objs=%d ch=%d pr=%d en=%d hp=%u p=(%d,%d) v=%d "
         "dir=%s map=%dx%d dodge=%d hit=%d beam=%d ind=%d fd=%d tgt=(%d,%d) "
         "hu=%d\n",
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
