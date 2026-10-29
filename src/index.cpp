#include "./core/scan.h"
#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>
#include <stdint.h>

namespace
{

constexpr int64_t kNanosecondsPerSecond = 1000000000LL;
constexpr int64_t kSupervisorInitialIntervalSeconds = 20;
constexpr int64_t kSupervisorMaximumIntervalSeconds = 120;
constexpr int64_t kPollBudgetSeconds = 620;

struct supervisor_progress
{
    int64_t waited_seconds;
    int64_t interval_seconds;
};

void schedule_supervisor_tick(supervisor_progress progress);

void run_supervisor_tick(supervisor_progress progress)
{
    if (setup_done)
    {
        return;
    }
    if (progress.waited_seconds >= kPollBudgetSeconds)
    {
        poll_for_game(0);
        progress.waited_seconds = 0;
    }
    progress.waited_seconds += progress.interval_seconds;
    progress.interval_seconds =
        progress.interval_seconds * 2 > kSupervisorMaximumIntervalSeconds
            ? kSupervisorMaximumIntervalSeconds
            : progress.interval_seconds * 2;
    schedule_supervisor_tick(progress);
}

void schedule_supervisor_tick(supervisor_progress progress)
{
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      progress.interval_seconds * kNanosecondsPerSecond),
        dispatch_get_main_queue(),
        ^{ run_supervisor_tick(progress); });
}

void start_supervisor()
{
    if (setup_done)
    {
        return;
    }
    poll_for_game(0);
    supervisor_progress progress{kSupervisorInitialIntervalSeconds,
                                 kSupervisorInitialIntervalSeconds};
    schedule_supervisor_tick(progress);
}

void on_image_added(const struct mach_header*, intptr_t)
{
    if (setup_done || base)
    {
        return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      if (setup_done || base)
      {
          return;
      }
      poll_for_game(0);
    });
}

}

__attribute__((constructor)) static void start()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      _dyld_register_func_for_add_image(&on_image_added);
      dispatch_async(dispatch_get_main_queue(), ^{ start_supervisor(); });
    });
}
