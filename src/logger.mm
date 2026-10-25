#include "data.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uintptr_t rcl_addr_getinstance;
extern uintptr_t rcl_addr_getownchar;
extern uintptr_t rcl_addr_getteam;
extern uintptr_t rcl_addr_getownidx;
extern uintptr_t rcl_addr_getx;
extern uintptr_t rcl_addr_gety;
extern uintptr_t rcl_addr_getdata;
extern uintptr_t rcl_addr_getgid;
extern uintptr_t rcl_addr_movedto;
extern uintptr_t rcl_addr_charspeed;
extern uintptr_t rcl_addr_charradius;
extern uintptr_t rcl_addr_projspeed;
extern uintptr_t rcl_addr_projradius;
extern uintptr_t rcl_addr_projbeam;

namespace
{
    constexpr long long LOG_MAX_BYTES = 8000000;
    constexpr int LOG_INTERVAL_MS = 200;
    constexpr int PROJ_SLOTS = 16;
    constexpr int RAW_EVERY = 50;
    constexpr uintptr_t RAW_BYTES = 0x1F0;

    FILE *g_file = nullptr;
    dispatch_source_t g_timer = nullptr;
    unsigned long long g_ticks = 0;
    uint64_t g_lastMs = 0;
    uint64_t g_lastScan = 0;
    int g_rate = 0;
    char g_path[1024];

    const char *document_path(void)
    {
        static char resolved[1024];
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);

        resolved[0] = 0;
        if (paths != nil && [paths count] > 0)
        {
            const char *dir = [[paths objectAtIndex:0] UTF8String];

            if (dir != nullptr) snprintf(resolved, sizeof(resolved), "%s", dir);
        }
        if (resolved[0] == 0)
        {
            const char *home = getenv("HOME");

            if (home != nullptr) snprintf(resolved, sizeof(resolved), "%s/Documents", home);
        }

        return resolved;
    }

    void rotate_if_needed(void)
    {
        long long size = 0;
        FILE *probe = nullptr;

        probe = fopen(g_path, "rb");
        if (probe == nullptr) return;

        fseek(probe, 0, SEEK_END);
        size = ftell(probe);
        fclose(probe);

        if (size <= LOG_MAX_BYTES) return;

        char backup[1100];

        snprintf(backup, sizeof(backup), "%s.1", g_path);
        rename(g_path, backup);
    }

    bool open_log(void)
    {
        const char *dir = document_path();

        if (dir[0] == 0) return false;

        snprintf(g_path, sizeof(g_path), "%s/Recoil.log", dir);

        g_file = fopen(g_path, "a");
        if (g_file == nullptr) return false;

        fseek(g_file, 0, SEEK_END);
        if (ftell(g_file) == 0)
        {
            fprintf(g_file, "=== recoil log ===\n");
            fprintf(g_file, "documents: %s\n", dir);
        }

        return true;
    }

    void line(const char *format, ...)
    {
        va_list args;

        if (g_file == nullptr) return;

        va_start(args, format);
        vfprintf(g_file, format, args);
        va_end(args);
    }

    void dump_header(void)
    {
        uint32_t count = _dyld_image_count();
        uint32_t i = 0;
        uintptr_t found = 0;

        line("\n########## recoil header ##########\n");
        line("base=%p setup=%d ticks=%llu rate=%d/s\n", (void *)rcl_base, (int)rcl_setup_done, g_ticks,
             g_rate);
        line("rva getinstance=%p getownchar=%p getteam=%p getownidx=%p\n", (void *)rcl_addr_getinstance,
             (void *)rcl_addr_getownchar, (void *)rcl_addr_getteam, (void *)rcl_addr_getownidx);
        line("rva getx=%p gety=%p getdata=%p getgid=%p movedto=%p\n", (void *)rcl_addr_getx,
             (void *)rcl_addr_gety, (void *)rcl_addr_getdata, (void *)rcl_addr_getgid,
             (void *)rcl_addr_movedto);
        line("rva charspeed=%p charradius=%p projspeed=%p projradius=%p projbeam=%p\n",
             (void *)rcl_addr_charspeed, (void *)rcl_addr_charradius, (void *)rcl_addr_projspeed,
             (void *)rcl_addr_projradius, (void *)rcl_addr_projbeam);
        line("off state=%llx stateenum=%llx scene=%llx client=%llx mgr=%llx array=%llx count=%llx\n",
             (unsigned long long)RCL_STATE_RVA, (unsigned long long)RCL_STATE_ENUM_OFF,
             (unsigned long long)RCL_SCENE_OFF, (unsigned long long)RCL_CLIENT_OFF,
             (unsigned long long)RCL_MODE_MANAGER_OFF, (unsigned long long)RCL_MGR_ARRAY_OFF,
             (unsigned long long)RCL_MGR_COUNT_OFF);
        line("off ownidx=%llx ownteam=%llx x=%llx y=%llx team=%llx owner=%llx hp=%llx dead=%llx\n",
             (unsigned long long)RCL_OWNIDX_OFF, (unsigned long long)RCL_OWNTEAM_OFF,
             (unsigned long long)RCL_OBJ_X_OFF, (unsigned long long)RCL_OBJ_Y_OFF,
             (unsigned long long)RCL_OBJ_TEAM_OFF, (unsigned long long)RCL_OBJ_OWNERINDEX_OFF,
             (unsigned long long)RCL_OBJ_HP_OFF, (unsigned long long)RCL_OBJ_DEADFLAG_OFF);
        line("off projdata vtable=%llx speed=%llx radius=%llx\n",
             (unsigned long long)RCL_VTABLE_PROJ_DATA_RVA,
             (unsigned long long)RCL_PROJDATA_SPEED_OFF, (unsigned long long)RCL_PROJDATA_RADIUS_OFF);

        for (i = 0; i < count; i++)
        {
            uintptr_t base = (uintptr_t)_dyld_get_image_header(i);

            if (base != (uintptr_t)rcl_base) continue;
            found = base;
            line("[game] idx=%u base=%p name=%s\n", i, (void *)base, _dyld_get_image_name(i));
        }
        if (!found) line("[game] base %p not found in dyld list (count=%u)\n", (void *)rcl_base, count);
    }

    void dump_chain(void)
    {
        uintptr_t slot = (uintptr_t)rcl_read_global_ptr(RCL_STATE_RVA);
        uintptr_t scene = 0;
        uintptr_t client = 0;
        uintptr_t m28 = 0;
        uintptr_t m58 = 0;
        uintptr_t a28 = 0;
        uintptr_t a58 = 0;
        void *value = nullptr;
        int32_t state = -1;
        int32_t c28 = 0;
        int32_t c58 = 0;

        if (!slot)
        {
            line("chain slot=0 (not readable)\n");
            return;
        }

        rcl_read_int(slot + RCL_STATE_ENUM_OFF, &state);
        if (rcl_read_ptr(slot + RCL_SCENE_OFF, &value) && value) scene = (uintptr_t)value;
        if (scene && rcl_read_ptr(scene + RCL_CLIENT_OFF, &value) && value) client = (uintptr_t)value;

        if (!client && rcl_addr_getinstance)
        {
            client = ((uintptr_t (*)(void))rcl_addr_getinstance)();
        }

        if (client)
        {
            if (rcl_read_ptr(client + RCL_MODE_MANAGER_OFF, &value) && value) m28 = (uintptr_t)value;
            if (rcl_read_ptr(client + RCL_MGR_OFF, &value) && value) m58 = (uintptr_t)value;
            if (m28)
            {
                if (rcl_read_ptr(m28 + RCL_MGR_ARRAY_OFF, &value) && value) a28 = (uintptr_t)value;
                rcl_read_int(m28 + RCL_MGR_COUNT_OFF, &c28);
            }
            if (m58)
            {
                if (rcl_read_ptr(m58 + RCL_MGR_ARRAY_OFF, &value) && value) a58 = (uintptr_t)value;
                rcl_read_int(m58 + RCL_MGR_COUNT_OFF, &c58);
            }
        }

        line("chain slot=%p state=%d scene=%p client=%p\n", (void *)slot, state, (void *)scene,
             (void *)client);
        line("chain mgr[28]=%p arr=%p cnt=%d | mgr[58]=%p arr=%p cnt=%d\n", (void *)m28, (void *)a28, c28,
             (void *)m58, (void *)a58, c58);
    }

    void dump_own(void)
    {
        int32_t x = 0;
        int32_t y = 0;
        int32_t hp = 0;
        int32_t team = -1;
        int32_t owner = -1;
        uint8_t dead = 0;
        int okHp = 0;

        line("own elem=%p x=%d y=%d team=%d idx=%d dead=%d hp=%u ok=%d r=%.1f spd=%.1f\n",
             (void *)rcl_own_elem, rcl_own_x, rcl_own_y, rcl_own_team_b, rcl_own_index_v, rcl_dead,
             rcl_own_hp, rcl_own_ok_hp, rcl_own_r, rcl_own_speed);

        if (!rcl_own_elem) return;

        rcl_read_int(rcl_own_elem + RCL_OBJ_X_OFF, &x);
        rcl_read_int(rcl_own_elem + RCL_OBJ_Y_OFF, &y);
        rcl_read_int(rcl_own_elem + RCL_OBJ_HP_OFF, &hp);
        rcl_read_int(rcl_own_elem + RCL_OBJ_TEAM_OFF, &team);
        rcl_read_int(rcl_own_elem + RCL_OBJ_OWNERINDEX_OFF, &owner);
        rcl_read_byte(rcl_own_elem + RCL_OBJ_DEADFLAG_OFF, &dead);
        okHp = rcl_read_int(rcl_own_elem + RCL_OBJ_HP_OFF, &hp);

        line("own raw x=%d y=%d hp=%d(ok=%d) team=%d owner=%d deadbyte=%d\n", x, y, hp, okHp, team, owner,
             dead);
    }

    void dump_projectiles(void)
    {
        int i = 0;
        int n = 0;

        for (i = 0; i < PROJ_SLOTS; i++)
        {
            if (rcl_projs[i].elem) n++;
        }

        line("proj n=%d\n", n);

        for (i = 0; i < PROJ_SLOTS; i++)
        {
            rcl_proj_t *p = &rcl_projs[i];

            if (!p->elem) continue;

            line("  p%d elem=%p gid=%d team=%d x=%d y=%d px=%d py=%d vx=%.0f vy=%.0f t=%llu->%llu prev=%d "
                 "r=%.1f spd=%.1f beam=%d thr=%d cast=%d ang=%.0f\n",
                 i, (void *)p->elem, p->gid, p->team, p->x, p->y, p->px, p->py, p->vx, p->vy,
                 (unsigned long long)p->pms, (unsigned long long)p->qms, p->hasPrev, p->radius, p->speed,
                 p->isBeam, p->isThrower, p->castRange, p->angle);
        }
    }

    void dump_ad(void)
    {
        line("adg stage=%d haz=%d thr=%d danger=%d dir=(%.2f,%.2f) dmg=%.0f first=%.1f "
             "tgt=(%.0f,%.0f) sent=%d active=%d moving=%d\n",
             rcl_dbg.stage, rcl_dbg.hazards, rcl_dbg.threats, rcl_dbg.danger, rcl_dbg.dx,
             rcl_dbg.dy, rcl_dbg.ix, rcl_dbg.iy, rcl_dbg.tx, rcl_dbg.ty, rcl_dbg.sent, rcl_active,
             rcl_moving);

        for (int i = 0; i < rcl_dbg.thn; i++)
        {
            line("  th%d x=%.0f y=%.0f vx=%.0f vy=%.0f rad=%.0f\n", i, rcl_dbg.thx[i], rcl_dbg.thy[i],
                 rcl_dbg.thvx[i], rcl_dbg.thvy[i], rcl_dbg.thrad[i]);
        }
        if (rcl_dbg.note[0]) line("adg note=%s\n", rcl_dbg.note);
    }

    void dump_roster(void)
    {
        int i = 0;

        line("roster enemy=%d mate=%d team=%d\n", rcl_enemy_n, rcl_mate_n, rcl_own_team_b);

        for (i = 0; i < rcl_enemy_n && i < 12; i++)
        {
            line("  e%d x=%d y=%d team=%d\n", i, rcl_enemy_x[i], rcl_enemy_y[i], rcl_pl_team[i]);
        }
        for (i = 0; i < rcl_mate_n && i < 8; i++)
        {
            line("  m%d x=%d y=%d\n", i, rcl_mate_x[i], rcl_mate_y[i]);
        }
    }

    void dump_raw(void)
    {
        uintptr_t addr = 0;
        uintptr_t off = 0;

        if (!rcl_own_elem) return;

        line("own raw dump (%llu bytes)\n", (unsigned long long)RAW_BYTES);

        for (off = 0; off < RAW_BYTES; off += 0x10)
        {
            uint32_t w0 = 0;
            uint32_t w1 = 0;
            uint32_t w2 = 0;
            uint32_t w3 = 0;

            addr = rcl_own_elem + off;

            if (!rcl_read_int(addr + 0x0, (int32_t *)&w0)) break;
            if (!rcl_read_int(addr + 0x4, (int32_t *)&w1)) break;
            if (!rcl_read_int(addr + 0x8, (int32_t *)&w2)) break;
            if (!rcl_read_int(addr + 0xC, (int32_t *)&w3)) break;

            line("  %03llx %08x %08x %08x %08x | %d %d %d %d\n", (unsigned long long)off, w0, w1, w2, w3,
                 (int32_t)w0, (int32_t)w1, (int32_t)w2, (int32_t)w3);
        }
    }

    void dump_objects(void)
    {
        uintptr_t array = rcl_manager_array_ptr;
        int32_t count = rcl_manager_count_v;
        int32_t i = 0;

        if (!array) return;

        line("objects count=%d\n", count);

        for (i = 0; i < count && i < 24; i++)
        {
            void *raw = nullptr;
            uintptr_t object = 0;
            uintptr_t vt = 0;
            int32_t gid = 0;
            int32_t team = -1;
            int32_t owner = -1;
            int32_t x = 0;
            int32_t y = 0;
            int32_t hp = 0;
            uint8_t dead = 0;

            if (!rcl_read_ptr(array + (uintptr_t)i * 8ULL, &raw) || !raw) continue;

            object = (uintptr_t)raw;
            rcl_read_ptr(object, &raw);
            vt = (uintptr_t)raw;

            rcl_read_int(object + RCL_OBJ_GLOBALID_OFF, &gid);
            rcl_read_int(object + RCL_OBJ_TEAM_OFF, &team);
            rcl_read_int(object + RCL_OBJ_OWNERINDEX_OFF, &owner);
            rcl_read_int(object + RCL_OBJ_X_OFF, &x);
            rcl_read_int(object + RCL_OBJ_Y_OFF, &y);
            rcl_read_int(object + RCL_OBJ_HP_OFF, &hp);
            rcl_read_byte(object + RCL_OBJ_DEADFLAG_OFF, &dead);

            line("  o%d obj=%p vt=%llx gid=%d team=%d owner=%d x=%d y=%d hp=%d dead=%d\n", i, (void *)object,
                 (unsigned long long)(vt >= rcl_base ? vt - rcl_base : vt), gid, team, owner, x, y, hp,
                 dead);
        }
    }

    void tick(void)
    {
        uint64_t now = 0;
        uint64_t scan = 0;
        int32_t probeTeam = -1;
        int32_t probeIdx = -1;

        if (g_file == nullptr) return;

        g_ticks++;
        now = (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
        scan = (uint64_t)rcl_ticks_a;

        if (g_lastMs && now > g_lastMs)
        {
            g_rate = (int)(((scan - g_lastScan) * 1000ULL) / (now - g_lastMs));
        }

        line("\n--- t=%llu ms=%.3f dt=%llu scan=%llu rate=%d/s ---\n", g_ticks,
             CFAbsoluteTimeGetCurrent(), (unsigned long long)(g_lastMs ? now - g_lastMs : 0),
             (unsigned long long)scan, g_rate);

        dump_chain();
        dump_own();

        if (rcl_addr_getteam && rcl_controller())
        {
            uintptr_t client = rcl_controller();

            probeTeam = ((int32_t (*)(uintptr_t))rcl_addr_getteam)(client);
            if (rcl_addr_getownidx) probeIdx = ((int32_t (*)(uintptr_t))rcl_addr_getownidx)(client);
            line("client probe team=%d idx=%d ctrl=%p\n", probeTeam, probeIdx, (void *)client);
        }

        dump_projectiles();
        dump_ad();

        if ((g_ticks % 2) == 0) dump_roster();
        if ((g_ticks % 4) == 0) dump_objects();
        if ((g_ticks % RAW_EVERY) == 0) dump_raw();
        if ((g_ticks % 50) == 0) dump_header();

        g_lastMs = now;
        g_lastScan = scan;

        fflush(g_file);
        rotate_if_needed();
    }

    void start(void)
    {
        dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);

        if (!open_log()) return;

        dump_header();
        fflush(g_file);

        g_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        if (!g_timer) return;

        dispatch_source_set_timer(g_timer, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(150 * NSEC_PER_MSEC)),
                                  (uint64_t)(LOG_INTERVAL_MS * NSEC_PER_MSEC), (uint64_t)(10 * NSEC_PER_MSEC));
        dispatch_source_set_event_handler(g_timer, ^{
          tick();
        });
        dispatch_resume(g_timer);
    }
}

__attribute__((constructor)) static void recoil_logger_entry(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
      start();
    });
}
