#include "../core/scan.h"
#include "../features/autododge.h"
#include "../utils/walls.h"
#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <atomic>
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
static FILE* file = nullptr;
static dispatch_source_t timer = nullptr;
static std::atomic<unsigned long long> ticks{0};
static std::atomic<uint64_t> lastMs{0};
static std::atomic<int> rate{0};
static char path[1024];

void document_path(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    out[0] = 0;
    if (home != nullptr) snprintf(out, cap, "%s/Documents", home);
}

void rotate_if_needed()
{
    long long size = 0;
    FILE* probe = fopen(path, "rb");
    if (probe == nullptr) return;
    fseek(probe, 0, SEEK_END);
    size = ftell(probe);
    fclose(probe);
    if (size <= LOG_MAX_BYTES) return;
    char backup[1100];
    snprintf(backup, sizeof(backup), "%s.1", path);
    rename(path, backup);
}

bool open_log()
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
        fprintf(file, "=== autododge log ===\n");
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
                   sig, (void*)fault, (void*)pc, (void*)lr);
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

void install_crash_handler()
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

void dump_autododge()
{
    // Эти функции должны быть реализованы в autododge.cpp и экспортированы
    extern bool autododge_get_debug_info(char* buf, size_t cap);
    
    char buf[8192];
    if (autododge_get_debug_info(buf, sizeof(buf)))
    {
        line("%s", buf);
    }
    else
    {
        line("[autododge] debug info unavailable\n");
    }
}

void tick()
{
    uint64_t now = 0;
    if (file == nullptr)
    {
        if (!open_log()) return;
        line("\n##### autododge session #####\n");
    }
    ticks.fetch_add(1, std::memory_order_relaxed);
    if (!base || !own_elem || dead)
    {
        line("idle t=%llu base=%p own=%p dead=%d\n",
             ticks.load(), (void*)base, (void*)own_elem, dead);
        if ((ticks.load() % 20) == 0) dump_autododge();
        lastMs.store(0);
        rate.store(0);
        fflush(file);
        rotate_if_needed();
        return;
    }

    const uint64_t lastMsV = lastMs.load();
    if (lastMsV == 0)
    {
        line("\n##### match start #####\n");
    }
    now = static_cast<uint64_t>(g_get_real_time() / 1000);
    
    line("\n--- t=%llu ms=%.3f ---\n",
         ticks.load(),
         static_cast<double>(g_get_real_time()) / 1000.0);
    
    write_crash_marks("[crash-mark] t=%llu stage=ad\n", ticks.load());
    dump_autododge();
    
    if ((ticks.load() % 10) == 0) dump_autododge(); // Дополнительный дамп каждые 2 секунды
    write_crash_marks("[crash-mark] t=%llu stage=done\n", ticks.load());
    
    lastMs.store(now);
    fflush(file);
    rotate_if_needed();
}

void start()
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

__attribute__((constructor)) static void logger_entry()
{
    write_crash_marks("[crash-mark] process start\n");
    install_crash_handler();
    dispatch_async(dispatch_get_main_queue(), ^{ start(); });
}