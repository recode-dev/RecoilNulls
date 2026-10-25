#include "data.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uintptr_t rcl_addr_getdata;
extern uintptr_t rcl_addr_getgid;
extern uintptr_t rcl_addr_getownidx;
extern uintptr_t rcl_addr_movedto;
extern uintptr_t rcl_addr_charspeed;
extern uintptr_t rcl_addr_charradius;
extern uintptr_t rcl_addr_projspeed;
extern uintptr_t rcl_addr_projradius;
extern uintptr_t rcl_addr_projbeam;

extern float rcl_own_r;
extern float rcl_own_speed;

namespace
{
    constexpr double LOG_INTERVAL_MS = 250.0;
    constexpr long long LOG_MAX_BYTES = 1500000;
    constexpr int OBJECT_DUMP_EVERY = 4;
    constexpr int OBJECT_DUMP_LIMIT = 32;

    FILE *g_file = nullptr;
    uintptr_t g_chainClient = 0;
    uintptr_t g_chainArray = 0;
    int32_t g_chainCount = 0;
    dispatch_source_t g_timer = nullptr;
    unsigned long long g_ticks = 0;
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

    void dump_images(void)
    {
        uint32_t count = _dyld_image_count();
        uint32_t i = 0;

        line("[images] count=%u\n", count);

        for (i = 0; i < count && i < 12; i++)
        {
            const char *name = _dyld_get_image_name(i);
            uintptr_t base = (uintptr_t)_dyld_get_image_header(i);

            line("[image] base=%p name=%s\n", (void *)base, name ? name : "?");
        }
    }

    void dump_header(void)
    {
        line("\n########## recoil diagnostic ##########\n");
        line("base=%p setup=%d ticks=%llu\n", (void *)rcl_base, (int)rcl_setup_done, g_ticks);
        line("rva getinstance=%p getownchar=%p getteam=%p getownidx=%p\n",
             (void *)rcl_addr_getinstance, (void *)rcl_addr_getownchar,
             (void *)rcl_addr_getteam, (void *)rcl_addr_getownidx);
        line("rva getx=%p gety=%p getdata=%p getgid=%p movedto=%p\n",
             (void *)rcl_addr_getx, (void *)rcl_addr_gety,
             (void *)rcl_addr_getdata, (void *)rcl_addr_getgid, (void *)rcl_addr_movedto);
        line("rva charspeed=%p charradius=%p projspeed=%p projradius=%p projbeam=%p\n",
             (void *)rcl_addr_charspeed, (void *)rcl_addr_charradius,
             (void *)rcl_addr_projspeed, (void *)rcl_addr_projradius, (void *)rcl_addr_projbeam);
        line("off state=%llx stateenum=%llx scene=%llx client=%llx mgr=%llx mgrarray=%llx mgrcount=%llx\n",
             (unsigned long long)RCL_STATE_RVA, (unsigned long long)RCL_STATE_ENUM_OFF,
             (unsigned long long)RCL_SCENE_OFF, (unsigned long long)RCL_CLIENT_OFF,
             (unsigned long long)RCL_MGR_OFF, (unsigned long long)RCL_MGR_ARRAY_OFF,
             (unsigned long long)RCL_MGR_COUNT_OFF);
        line("off ownidx=%llx ownteam=%llx objx=%llx objy=%llx objteam=%llx objowner=%llx\n",
             (unsigned long long)RCL_OWNIDX_OFF, (unsigned long long)RCL_OWNTEAM_OFF,
             (unsigned long long)RCL_OBJ_X_OFF, (unsigned long long)RCL_OBJ_Y_OFF,
             (unsigned long long)RCL_OBJ_TEAM_OFF, (unsigned long long)RCL_OBJ_OWNERINDEX_OFF);
        line("off dead=%llx vtable projdata=%llx\n",
             (unsigned long long)RCL_OBJ_DEADFLAG_OFF, (unsigned long long)RCL_VTABLE_PROJ_DATA_RVA);
        dump_images();
    }

    void dump_chain(void)
    {
        uintptr_t slot = (uintptr_t)rcl_read_global_ptr(RCL_STATE_RVA);
        uintptr_t scene = 0;
        uintptr_t client = 0;
        uintptr_t manager = 0;
        uintptr_t array = 0;
        void *value = nullptr;
        int32_t state = -1;
        int32_t count = 0;

        line("\n[t=%llu] chain\n", g_ticks);

        if (!slot)
        {
            line("  slot=0 (state global not readable)\n");
            return;
        }

        rcl_read_int(slot + RCL_STATE_ENUM_OFF, &state);
        line("  slot=%p state=%d\n", (void *)slot, state);

        if (rcl_read_ptr(slot + RCL_SCENE_OFF, &value) && value)
        {
            scene = (uintptr_t)value;
            line("  scene=%p\n", (void *)scene);
        }
        if (scene && rcl_read_ptr(scene + RCL_CLIENT_OFF, &value) && value)
        {
            client = (uintptr_t)value;
            line("  client=%p\n", (void *)client);
        }
        if (!client && rcl_addr_getinstance)
        {
            client = ((uintptr_t (*)(void))rcl_addr_getinstance)();
            line("  client(getinstance)=%p\n", (void *)client);
        }
        if (!client) return;

        if (rcl_read_ptr(client + RCL_MGR_OFF, &value) && value)
        {
            manager = (uintptr_t)value;
            line("  mgr(client+%llx)=%p\n", (unsigned long long)RCL_MGR_OFF, (void *)manager);
        }
        if (!manager && rcl_read_ptr(client + RCL_MODE_MANAGER_OFF, &value) && value)
        {
            manager = (uintptr_t)value;
            line("  mgr(client+%llx)=%p\n", (unsigned long long)RCL_MODE_MANAGER_OFF, (void *)manager);
        }
        if (!manager) return;

        if (rcl_read_ptr(manager + RCL_MGR_ARRAY_OFF, &value) && value)
        {
            array = (uintptr_t)value;
        }
        rcl_read_int(manager + RCL_MGR_COUNT_OFF, &count);

        line("  array=%p count=%d\n", (void *)array, count);

        g_chainClient = client;
        g_chainArray = array;
        g_chainCount = count;
    }

    void dump_own(void)
    {
        int32_t x = 0;
        int32_t y = 0;
        uint8_t dead = 0;
        int32_t idx = -1;

        line("  own elem=%p dead=%d team=%d idx=%d radius=%.1f speed=%.1f\n",
             (void *)rcl_own_elem, rcl_dead, rcl_own_team_b,
             rcl_addr_getownidx && g_chainClient ? (int)((int32_t (*)(uintptr_t))rcl_addr_getownidx)(g_chainClient) : -1,
             rcl_own_r, rcl_own_speed);

        if (!rcl_own_elem) return;

        rcl_read_int(rcl_own_elem + RCL_OBJ_X_OFF, &x);
        rcl_read_int(rcl_own_elem + RCL_OBJ_Y_OFF, &y);
        rcl_read_byte(rcl_own_elem + RCL_OBJ_DEADFLAG_OFF, &dead);
        rcl_read_int(g_chainClient + RCL_OWNIDX_OFF, &idx);

        line("  own x=%d y=%d deadbyte=%d clientidx=%d\n", x, y, dead, idx);
    }

    void dump_projectiles(void)
    {
        int i = 0;

        line("  projectiles:\n");

        for (i = 0; i < 16; i++)
        {
            if (rcl_projs[i].gid == 0 && rcl_projs[i].elem == 0) continue;

            line("    [%d] elem=%p gid=%d team=%d x=%d y=%d vx=%.0f vy=%.0f r=%.0f beam=%d prev=%d\n",
                 i, (void *)rcl_projs[i].elem, rcl_projs[i].gid, rcl_projs[i].team,
                 rcl_projs[i].x, rcl_projs[i].y, rcl_projs[i].vx, rcl_projs[i].vy,
                 rcl_projs[i].radius, rcl_projs[i].isBeam, rcl_projs[i].hasPrev);
        }
    }

    void dump_objects(int limit)
    {
        int i = 0;

        line("  objects (%d of %d):\n", limit, g_chainCount);

        for (i = 0; i < g_chainCount && i < limit; i++)
        {
            void *raw = nullptr;
            void *dataRaw = nullptr;
            uintptr_t object = 0;
            uintptr_t vtable = 0;
            uintptr_t data = 0;
            uintptr_t dataVtable = 0;
            int32_t gid = 0;
            int32_t team = -1;
            int32_t owner = -1;
            int32_t x = 0;
            int32_t y = 0;
            int32_t hp = 0;
            uint8_t dead = 0;
            uint8_t active = 0;

            if (!rcl_read_ptr(g_chainArray + (uintptr_t)i * 8ULL, &raw) || !raw) continue;
            object = (uintptr_t)raw;
            if (!rcl_read_ptr(object, &dataRaw)) continue;
            vtable = (uintptr_t)dataRaw;

            data = rcl_addr_getdata ? ((uintptr_t (*)(uintptr_t))rcl_addr_getdata)(object) : 0;
            if (data && rcl_read_ptr(data, &dataRaw)) dataVtable = (uintptr_t)dataRaw;

            gid = rcl_addr_getgid ? ((int32_t (*)(uintptr_t))rcl_addr_getgid)(object) : 0;
            rcl_read_int(object + RCL_OBJ_TEAM_OFF, &team);
            rcl_read_int(object + RCL_OBJ_OWNERINDEX_OFF, &owner);
            rcl_read_int(object + RCL_OBJ_X_OFF, &x);
            rcl_read_int(object + RCL_OBJ_Y_OFF, &y);
            rcl_read_int(object + RCL_OBJ_HP_OFF, &hp);
            rcl_read_byte(object + RCL_OBJ_DEADFLAG_OFF, &dead);
            rcl_read_byte(object + RCL_OBJ_ACTIVEFLAG_OFF, &active);

            line("    [%d] obj=%p vt=%p(%llx) data=%p dvt=%p(%llx)\n",
                 i, (void *)object, (void *)vtable,
                 (unsigned long long)(vtable >= rcl_base ? vtable - rcl_base : 0),
                 (void *)data, (void *)dataVtable,
                 (unsigned long long)(dataVtable >= rcl_base ? dataVtable - rcl_base : 0));
            line("        gid=%d team=%d owner=%d x=%d y=%d hp=%d dead=%d active=%d\n",
                 gid, team, owner, x, y, hp, dead, active);
        }
    }


    void tick(void)
    {
        if (g_file == nullptr) return;

        g_ticks++;

        dump_chain();
        dump_own();
        dump_projectiles();

        if (g_chainArray && g_chainCount > 0 && (g_ticks % OBJECT_DUMP_EVERY) == 0)
        {
            dump_objects(OBJECT_DUMP_LIMIT);
        }

        if ((g_ticks % 16) == 0) dump_header();

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

        dispatch_source_set_timer(g_timer,
                                  dispatch_time(DISPATCH_TIME_NOW, (int64_t)(250 * NSEC_PER_MSEC)),
                                  (uint64_t)(250 * NSEC_PER_MSEC),
                                  (uint64_t)(50 * NSEC_PER_MSEC));
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
