#include "../core/scan.h"
#include "../features/autododge.h"
#include <dispatch/dispatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uintptr_t addr_getownidx;

extern uintptr_t addr_getdata;
extern uintptr_t addr_getgid;
extern uintptr_t addr_movedto;
extern uintptr_t addr_charspeed;
extern uintptr_t addr_charradius;

namespace
{
constexpr long long LOG_MAX_BYTES = 8000000;
constexpr int LOG_INTERVAL_MS = 200;
constexpr int RAW_EVERY = 50;
constexpr uintptr_t RAW_BYTES = 0x1F0;
FILE* g_file = nullptr;
dispatch_source_t g_timer = nullptr;
unsigned long long g_ticks = 0;
uint64_t g_lastMs = 0;
uint64_t g_lastScan = 0;
int g_rate = 0;
char g_path[1024];
const char* document_path(void)
{
    static char resolved[1024];
    const char* home = getenv("HOME");
    resolved[0] = 0;
    if (home != nullptr)
    {
        snprintf(resolved, sizeof(resolved), "%s/Documents", home);
    }
    if (resolved[0] == 0)
    {
        snprintf(resolved, sizeof(resolved), "%s", "/tmp");
    }
    return resolved;
}
void rotate_if_needed(void)
{
    long long size = 0;
    FILE* probe = nullptr;
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
    const char* dir = document_path();
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
void line(const char* format, ...)
{
    va_list args;
    if (g_file == nullptr) return;
    va_start(args, format);
    vfprintf(g_file, format, args);
    va_end(args);
}
void dump_header(void)
{
    line("\n########## recoil header ##########\n");
    line("base=%p setup=%d ticks=%llu rate=%d/s\n",
         (void*)base,
         (int)setup_done,
         g_ticks,
         g_rate);
    line("rva getinstance=%p getownchar=%p getteam=%p getownidx=%p\n",
         (void*)addr_getinstance,
         (void*)addr_getownchar,
         (void*)addr_getteam,
         (void*)addr_getownidx);
    line("rva getx=%p gety=%p getdata=%p getgid=%p movedto=%p\n",
         (void*)addr_getx,
         (void*)addr_gety,
         (void*)addr_getdata,
         (void*)addr_getgid,
         (void*)addr_movedto);
    line("rva charspeed=%p charradius=%p projspeed=%p projradius=%p "
         "projbeam=%p\n",
         (void*)addr_charspeed,
         (void*)addr_charradius,
         (void*)addr_projspeed,
         (void*)addr_projradius,
         (void*)addr_projbeam);
    line("off state=%llx stateenum=%llx scene=%llx client=%llx mgr=%llx "
         "array=%llx count=%llx\n",
         (unsigned long long)STATE_RVA,
         (unsigned long long)STATE_ENUM_OFF,
         (unsigned long long)SCENE_OFF,
         (unsigned long long)CLIENT_OFF,
         (unsigned long long)MODE_MANAGER_OFF,
         (unsigned long long)MGR_ARRAY_OFF,
         (unsigned long long)MGR_COUNT_OFF);
    line("off x=%llx y=%llx team=%llx owner=%llx "
         "hp=%llx dead=%llx\n",
         (unsigned long long)OBJ_X_OFF,
         (unsigned long long)OBJ_Y_OFF,
         (unsigned long long)OBJ_TEAM_OFF,
         (unsigned long long)OBJ_OWNERINDEX_OFF,
         (unsigned long long)OBJ_HP_OFF,
         (unsigned long long)OBJ_DEADFLAG_OFF);
    line("off projdata vtable=%llx\n",
         (unsigned long long)VTABLE_PROJ_DATA_RVA);
}
void dump_chain(void)
{
    uintptr_t slot = (uintptr_t)read_global_ptr(STATE_RVA);
    uintptr_t scene = 0;
    uintptr_t clientPtr = 0;
    uintptr_t m28 = 0;
    uintptr_t m58 = 0;
    uintptr_t a28 = 0;
    uintptr_t a58 = 0;
    void* value = nullptr;
    int32_t state = -1;
    int32_t c28 = 0;
    int32_t c58 = 0;
    if (!slot)
    {
        line("chain slot=0 (not readable)\n");
        return;
    }
    read_int(slot + STATE_ENUM_OFF, &state);
    if (read_ptr(slot + SCENE_OFF, &value) && value) scene = (uintptr_t)value;
    if (scene && read_ptr(scene + CLIENT_OFF, &value) && value)
        clientPtr = (uintptr_t)value;
    if (!clientPtr && addr_getinstance)
    {
        clientPtr = ((uintptr_t(*)(void))addr_getinstance)();
    }
    if (clientPtr)
    {
        if (read_ptr(clientPtr + MODE_MANAGER_OFF, &value) && value)
            m28 = (uintptr_t)value;
        if (read_ptr(clientPtr + MGR_OFF, &value) && value)
            m58 = (uintptr_t)value;
        if (m28)
        {
            if (read_ptr(m28 + MGR_ARRAY_OFF, &value) && value)
                a28 = (uintptr_t)value;
            read_int(m28 + MGR_COUNT_OFF, &c28);
        }
        if (m58)
        {
            if (read_ptr(m58 + MGR_ARRAY_OFF, &value) && value)
                a58 = (uintptr_t)value;
            read_int(m58 + MGR_COUNT_OFF, &c58);
        }
    }
    line("chain slot=%p state=%d scene=%p client=%p\n",
         (void*)slot,
         state,
         (void*)scene,
         (void*)clientPtr);
    line("chain mgr[28]=%p arr=%p cnt=%d | mgr[58]=%p arr=%p cnt=%d\n",
         (void*)m28,
         (void*)a28,
         c28,
         (void*)m58,
         (void*)a58,
         c58);
}
void dump_own(void)
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t hp = 0;
    int32_t team = -1;
    int32_t ownerIdx = -1;
    int okHp = 0;
    line("own elem=%p x=%d y=%d team=%d idx=%d dead=%d hp=%u ok=%d r=%.1f "
         "spd=%.1f\n",
         (void*)own_elem,
         own_x,
         own_y,
         own_team_b,
         own_index_v,
         dead,
         own_hp,
         own_ok_hp,
         own_r,
         own_speed);
    if (!own_elem) return;
    read_int(own_elem + OBJ_X_OFF, &x);
    read_int(own_elem + OBJ_Y_OFF, &y);
    read_int(own_elem + OBJ_HP_OFF, &hp);
    read_int(own_elem + OBJ_TEAM_OFF, &team);
    read_int(own_elem + OBJ_OWNERINDEX_OFF, &ownerIdx);
    okHp = read_int(own_elem + OBJ_HP_OFF, &hp);
    line("own raw x=%d y=%d hp=%d(ok=%d) team=%d owner=%d\n",
         x,
         y,
         hp,
         okHp,
         team,
         ownerIdx);
}
void dump_ad(void)
{
    line("adg stage=%d haz=%d thr=%d danger=%d dir=(%.2f,%.2f) dmg=%.0f "
         "first=%.1f range=%.0f map=%d rk=%d "
         "tgt=(%.0f,%.0f) sent=%d active=%d moving=%d\n",
         dbg.stage,
         dbg.hazards,
         dbg.threats,
         dbg.danger,
         dbg.dx,
         dbg.dy,
         dbg.ix,
         dbg.iy,
         own_cast_range(),
         dbg.mapn,
         dbg.ranged,
         dbg.tx,
         dbg.ty,
         dbg.sent,
         active,
         moving);
    if (dbg.note[0]) line("adg note=%s\n", dbg.note);
}
void dump_raw(void)
{
    uintptr_t addr = 0;
    uintptr_t off = 0;
    if (!own_elem) return;
    line("own raw dump (%llu bytes)\n", (unsigned long long)RAW_BYTES);
    for (off = 0; off < RAW_BYTES; off += 0x10)
    {
        uint32_t w0 = 0;
        uint32_t w1 = 0;
        uint32_t w2 = 0;
        uint32_t w3 = 0;
        addr = own_elem + off;
        if (!read_int(addr + 0x0, (int32_t*)&w0)) break;
        if (!read_int(addr + 0x4, (int32_t*)&w1)) break;
        if (!read_int(addr + 0x8, (int32_t*)&w2)) break;
        if (!read_int(addr + 0xC, (int32_t*)&w3)) break;
        line("  %03llx %08x %08x %08x %08x | %d %d %d %d\n",
             (unsigned long long)off,
             w0,
             w1,
             w2,
             w3,
             (int32_t)w0,
             (int32_t)w1,
             (int32_t)w2,
             (int32_t)w3);
    }
}
void dump_objects(void)
{
    uintptr_t array = manager_array_ptr;
    int32_t count = manager_count_v;
    int32_t i = 0;
    if (!array) return;
    line("objects count=%d\n", count);
    for (i = 0; i < count && i < 24; i++)
    {
        void* raw = nullptr;
        uintptr_t object = 0;
        uintptr_t vt = 0;
        int32_t projGid = 0;
        int32_t team = -1;
        int32_t ownerIdx = -1;
        int32_t x = 0;
        int32_t y = 0;
        int32_t hp = 0;
        uint8_t deadFlag = 0;
        if (!read_ptr(array + (uintptr_t)i * 8ULL, &raw) || !raw) continue;
        object = (uintptr_t)raw;
        if (!pointer_plausible(object)) continue;
        read_ptr(object, &raw);
        vt = (uintptr_t)raw;
        read_int(object + OBJ_GLOBALID_OFF, &projGid);
        read_int(object + OBJ_TEAM_OFF, &team);
        read_int(object + OBJ_OWNERINDEX_OFF, &ownerIdx);
        read_int(object + OBJ_X_OFF, &x);
        read_int(object + OBJ_Y_OFF, &y);
        read_int(object + OBJ_HP_OFF, &hp);
        read_byte(object + OBJ_DEADFLAG_OFF, &deadFlag);
        line("  o%d obj=%p vt=%llx gid=%d team=%d owner=%d x=%d y=%d hp=%d "
             "dead=%d\n",
             i,
             (void*)object,
             (unsigned long long)(vt >= base ? vt - base : vt),
             projGid,
             team,
             ownerIdx,
             x,
             y,
             hp,
             deadFlag);
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
    scan = (uint64_t)ticks_a;
    if (g_lastMs && now > g_lastMs)
    {
        g_rate = (int)(((scan - g_lastScan) * 1000ULL) / (now - g_lastMs));
    }
    line("\n--- t=%llu ms=%.3f dt=%llu scan=%llu rate=%d/s ---\n",
         g_ticks,
         CFAbsoluteTimeGetCurrent(),
         (unsigned long long)(g_lastMs ? now - g_lastMs : 0),
         (unsigned long long)scan,
         g_rate);
    dump_chain();
    dump_own();
    if (addr_getteam && controller())
    {
        uintptr_t clientPtr = controller();
        probeTeam = ((int32_t(*)(uintptr_t))addr_getteam)(clientPtr);
        if (addr_getownidx)
            probeIdx = ((int32_t(*)(uintptr_t))addr_getownidx)(clientPtr);
        line("client probe team=%d idx=%d ctrl=%p\n",
             probeTeam,
             probeIdx,
             (void*)clientPtr);
    }
    dump_ad();
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
    dispatch_source_set_timer(
        g_timer,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(150 * NSEC_PER_MSEC)),
        (uint64_t)(LOG_INTERVAL_MS * NSEC_PER_MSEC),
        (uint64_t)(10 * NSEC_PER_MSEC));
    dispatch_source_set_event_handler(g_timer, ^{ tick(); });
    dispatch_resume(g_timer);
}
}
void stage_note(const char* what)
{
    if (what == nullptr) return;
    line("%s", what);
}
__attribute__((constructor)) static void logger_entry(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{ start(); });
}
