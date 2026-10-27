#include "../core/scan.h"
#include "../features/autododge.h"
#include "../utils/walls.h"
#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <csignal>
#include <sys/_types/_ucontext64.h>
#include <unistd.h>

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
constexpr float PROJ_NEAR = 1600.0f;
constexpr float PROJ_NEAR_SQ = PROJ_NEAR * PROJ_NEAR;
FILE* file = nullptr;
dispatch_source_t timer = nullptr;
unsigned long long ticks = 0;
uint64_t lastMs = 0;
uint64_t lastScan = 0;
int rate = 0;
char path[1024];
void document_path(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    out[0] = 0;
    if (home != nullptr) snprintf(out, cap, "%s/Documents", home);
}
void rotate_if_needed(void)
{
    long long size = 0;
    FILE* probe = nullptr;
    probe = fopen(path, "rb");
    if (probe == nullptr) return;
    fseek(probe, 0, SEEK_END);
    size = ftell(probe);
    fclose(probe);
    if (size <= LOG_MAX_BYTES) return;
    char backup[1100];
    snprintf(backup, sizeof(backup), "%s.1", path);
    rename(path, backup);
}
bool in_battle(void)
{
    return base != 0 && own_elem != 0;
}
bool open_log(void)
{
    char dir[1024];
    document_path(dir, sizeof(dir));
    if (dir[0] == 0) return false;
    snprintf(path, sizeof(path), "%s/Recoil.log", dir);
    file = fopen(path, "a");
    if (file == nullptr) return false;
    fseek(file, 0, SEEK_END);
    if (ftell(file) == 0)
    {
        fprintf(file, "=== log ===\n");
        fprintf(file, "documents: %s\n", dir);
    }
    return true;
}
void line(const char* format, ...)
{
    va_list args;
    if (file == nullptr) return;
    va_start(args, format);
    vfprintf(file, format, args);
    fflush(file);
    va_end(args);
}
void write_crash_marks(const char* format, ...)
{
    va_list args;
    FILE* f = nullptr;
    if (path[0] == 0)
    {
        char dir[1024];
        document_path(dir, sizeof(dir));
        if (dir[0] != 0) snprintf(path, sizeof(path), "%s/Recoil.log", dir);
    }
    if (file != nullptr) fflush(file);
    f = fopen(path, "a");
    if (f == nullptr) return;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fflush(f);
    fclose(f);
}
void crash_signal(int sig, siginfo_t* info, void* ctx)
{
    char buf[256];
    uintptr_t pc = 0;
    uintptr_t lr = 0;
    uintptr_t fault = 0;
    int len = 0;
    Dl_info di;
    if (ctx != nullptr)
    {
        ucontext64_t* uc = static_cast<ucontext64_t*>(ctx);
        pc = static_cast<uintptr_t>(uc->uc_mcontext64->__ss.__pc);
        lr = static_cast<uintptr_t>(uc->uc_mcontext64->__ss.__lr);
    }
    if (info != nullptr) fault = (uintptr_t)info->si_addr;
    len = snprintf(buf, sizeof(buf),
                   "[crash-signal] sig=%d fault=%p pc=%p lr=%p\n",
                   sig,
                   (void*)fault,
                   (void*)pc,
                   (void*)lr);
    if (len > 0) write(STDERR_FILENO, buf, static_cast<size_t>(len));
    if (path[0] != 0)
    {
        int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd >= 0)
        {
            if (len > 0) write(fd, buf, static_cast<size_t>(len));
            if (dladdr((void*)pc, &di) && di.dli_fname)
            {
                len = snprintf(buf, sizeof(buf),
                               "[crash-signal] pc in %s off=%lx sym=%s\n",
                               di.dli_fname,
                               static_cast<unsigned long>(pc - (uintptr_t)di.dli_fbase),
                               di.dli_sname ? di.dli_sname : "?");
                if (len > 0) write(fd, buf, static_cast<size_t>(len));
            }
            close(fd);
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void install_crash_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGTRAP, &sa, nullptr);
}

void dump_header(void)
{
    line("\n########## header ##########\n");
    line("base=%p setup=%d ticks=%llu rate=%d/s\n",
         (void*)base,
         static_cast<int>(setup_done),
         ticks,
         rate);
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
    line("rva charspeed=%p charradius=%p projbeam=%p\n",
         (void*)addr_charspeed,
         (void*)addr_charradius,
         (void*)addr_projbeam);
    line("off state=%llx stateenum=%llx scene=%llx client=%llx mgr=%llx "
         "array=%llx count=%llx\n",
         static_cast<unsigned long long>(STATE_RVA),
         static_cast<unsigned long long>(STATE_ENUM_OFF),
         static_cast<unsigned long long>(SCENE_OFF),
         static_cast<unsigned long long>(CLIENT_OFF),
         static_cast<unsigned long long>(MODE_MANAGER_OFF),
         static_cast<unsigned long long>(MGR_ARRAY_OFF),
         static_cast<unsigned long long>(MGR_COUNT_OFF));
    line("off x=%llx y=%llx team=%llx owner=%llx "
         "hp=%llx dead=%llx\n",
         static_cast<unsigned long long>(OBJ_X_OFF),
         static_cast<unsigned long long>(OBJ_Y_OFF),
         static_cast<unsigned long long>(OBJ_TEAM_OFF),
         static_cast<unsigned long long>(OBJ_OWNERINDEX_OFF),
         static_cast<unsigned long long>(OBJ_HP_OFF),
         static_cast<unsigned long long>(OBJ_DEADFLAG_OFF));
    line("off projdata vtable=%llx\n",
         static_cast<unsigned long long>(VTABLE_PROJ_DATA_RVA));
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
static int live_projectiles(int* nearOut)
{
    int i = 0;
    int n = 0;
    int near = 0;
    uintptr_t projVt = 0;
    float ox = static_cast<float>(own_x);
    float oy = static_cast<float>(own_y);
    if (!base || obj_count <= 0)
    {
        return 0;
    }
    projVt = base + static_cast<uintptr_t>(VTABLE_PROJ_DATA_RVA);
    for (i = 0; i < obj_count; i++)
    {
        uintptr_t obj = obj_snapshot[i];
        void* data = nullptr;
        void* vtable = nullptr;
        int32_t px = 0;
        int32_t py = 0;
        int32_t team = -1;
        float dx = 0.0f;
        float dy = 0.0f;
        if (!obj || !read_ptr(obj + static_cast<uintptr_t>(ELEM_DEF_OFF), &data) || !data)
        {
            continue;
        }
        if (!read_ptr((uintptr_t)data, &vtable) || (uintptr_t)vtable != projVt)
        {
            continue;
        }
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_TEAM_OFF), &team) ||
            team == own_team_b)
        {
            continue;
        }
        n++;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_X_OFF), &px) ||
            !read_int(obj + static_cast<uintptr_t>(OBJ_Y_OFF), &py))
        {
            continue;
        }
        dx = static_cast<float>(px) - ox;
        dy = static_cast<float>(py) - oy;
        if (dx * dx + dy * dy <= PROJ_NEAR_SQ)
        {
            near++;
        }
    }
    if (nearOut)
    {
        *nearOut = near;
    }
    return n;
}
int scan_stage(void)
{
    if (!base) return 0;
    if (!scene_object) return 1;
    if (!own_elem) return 2;
    if (!manager_ptr) return 3;
    if (!obj_count) return 4;
    return 6;
}
void dump_ad(void)
{
    int near = 0;
    int proj = live_projectiles(&near);
    line("adg scan=%d active=%d moving=%d tgt=(%d,%d) proj=%d near=%d "
         "wall=%d %dx%d holes=%d\n",
         scan_stage(),
         active,
         moving,
         static_cast<int>(ad_target_x),
         static_cast<int>(ad_target_y),
         proj,
         near,
         wall_ready(),
         wall_cache_w(),
         wall_cache_h(),
         wall_holes());
}
void dump_raw(void)
{
    uintptr_t addr = 0;
    uintptr_t off = 0;
    if (!own_elem) return;
    line("own raw dump (%llu bytes)\n", static_cast<unsigned long long>(RAW_BYTES));
    for (off = 0; off < RAW_BYTES; off += 0x10)
    {
        uint32_t w0 = 0;
        uint32_t w1 = 0;
        uint32_t w2 = 0;
        uint32_t w3 = 0;
        addr = own_elem + off;
        if (!read_int(addr + 0x0, reinterpret_cast<int32_t*>(&w0))) break;
        if (!read_int(addr + 0x4, reinterpret_cast<int32_t*>(&w1))) break;
        if (!read_int(addr + 0x8, reinterpret_cast<int32_t*>(&w2))) break;
        if (!read_int(addr + 0xC, reinterpret_cast<int32_t*>(&w3))) break;
        line("  %03llx %08x %08x %08x %08x | %d %d %d %d\n",
             static_cast<unsigned long long>(off),
             w0,
             w1,
             w2,
             w3,
             static_cast<int32_t>(w0),
             static_cast<int32_t>(w1),
             static_cast<int32_t>(w2),
             static_cast<int32_t>(w3));
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
        if (!read_ptr(array + static_cast<uintptr_t>(i) * 8ULL, &raw) || !raw) continue;
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
             static_cast<unsigned long long>(vt >= base ? vt - base : vt),
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
    if (file == nullptr)
    {
        if (!open_log()) return;
        line("\n##### session #####\n");
    }
    ticks++;
    if (!in_battle())
    {
        line("idle t=%llu base=%p own=%p\n",
             ticks,
             (void*)base,
             (void*)own_elem);
        if ((ticks % 20) == 0) dump_header();
        lastMs = 0;
        lastScan = 0;
        rate = 0;
        fflush(file);
        rotate_if_needed();
        return;
    }
    if (lastMs == 0)
    {
        line("\n##### match #####\n");
        dump_header();
    }
    now = static_cast<uint64_t>(g_get_real_time() / 1000);
    scan = ticks_a;
    if (lastMs && now > lastMs)
    {
        rate = static_cast<int>(((scan - lastScan) * 1000ULL) / (now - lastMs));
    }
    line("\n--- t=%llu ms=%.3f dt=%llu scan=%llu rate=%d/s ---\n",
         ticks,
         static_cast<double>(g_get_real_time()) / 1000.0,
         static_cast<unsigned long long>(lastMs ? now - lastMs : 0),
         static_cast<unsigned long long>(scan),
         rate);
    write_crash_marks("[crash-mark] tick=%llu stage=chain\n", ticks);
    dump_chain();
    write_crash_marks("[crash-mark] tick=%llu stage=own\n", ticks);
    dump_own();
    write_crash_marks("[crash-mark] tick=%llu stage=ad\n", ticks);
    dump_ad();
    if ((ticks % 4) == 0)
    {
        write_crash_marks("[crash-mark] tick=%llu stage=objects\n", ticks);
        dump_objects();
    }
    if ((ticks % RAW_EVERY) == 0)
    {
        write_crash_marks("[crash-mark] tick=%llu stage=raw\n", ticks);
        dump_raw();
    }
    write_crash_marks("[crash-mark] tick=%llu stage=done\n", ticks);
    if ((ticks % 50) == 0) dump_header();
    lastMs = now;
    lastScan = scan;
    fflush(file);
    rotate_if_needed();
}
void start(void)
{
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (!timer) return;
    dispatch_source_set_timer(
        timer,
        dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(150 * NSEC_PER_MSEC)),
        static_cast<uint64_t>(LOG_INTERVAL_MS * NSEC_PER_MSEC),
        static_cast<uint64_t>(10 * NSEC_PER_MSEC));
    dispatch_source_set_event_handler(timer, ^{ tick(); });
    dispatch_resume(timer);
}
}
__attribute__((constructor)) static void logger_entry(void)
{
    write_crash_marks("[crash-mark] process start\n");
    install_crash_handler();
    dispatch_async(dispatch_get_main_queue(), ^{ start(); });

}
