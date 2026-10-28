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
    }
    line("[m] hz=%lu objs=%d ch=%d pr=%d en=%d hp=%u p=(%d,%d) v=%d dir=%s "
         "map=%dx%d dodge=%d hit=%d beam=%d ind=%d\n",
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
         ad_ind_n);
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
            snprintf(buf + len, sizeof(buf) - (size_t)len, " | none");
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
        line("[e%d] pid=%d p=(%.0f,%.0f) hp=%d d=%.0f a=%d bu=%d\n",
             i,
             ad_enemy_pid[i],
             ad_enemy_x[i],
             ad_enemy_y[i],
             ad_enemy_hp[i],
             ad_enemy_d[i],
             ad_enemy_aim[i],
             ad_enemy_burst[i]);
    }
    rotate_if_needed();
}

void autododge_scanner_log(uint64_t now, const char* format, ...)
{
    if (!format) return;
    if (!open_log()) return;
    line("\n[scan] t=%llu ", static_cast<unsigned long long>(now));
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);
}
