#include "./core/scan.h"
#include <dispatch/dispatch.h>
#include <stdint.h>

namespace
{

constexpr int64_t kNanosecondsPerSecond = 1000000000LL;
constexpr int64_t kRetryInitialIntervalSeconds = 20;
constexpr int64_t kRetryMaximumIntervalSeconds = 120;
constexpr int64_t kImageLookupBudgetSeconds = 620;

struct retry_schedule
{
    int64_t elapsed_seconds;
    int64_t interval_seconds;
};

void schedule_retry(retry_schedule schedule);

void run_retry(retry_schedule schedule)
{
    if (setup_done)
    {
        return;
    }
    if (schedule.elapsed_seconds >= kImageLookupBudgetSeconds)
    {
        poll_for_game(0);
        schedule.elapsed_seconds = 0;
    }
    schedule.elapsed_seconds += schedule.interval_seconds;
    schedule.interval_seconds = schedule.interval_seconds * 2 > kRetryMaximumIntervalSeconds
                                    ? kRetryMaximumIntervalSeconds
                                    : schedule.interval_seconds * 2;
    schedule_retry(schedule);
}

void schedule_retry(retry_schedule schedule)
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 schedule.interval_seconds * kNanosecondsPerSecond),
                   dispatch_get_main_queue(),
                   ^{ run_retry(schedule); });
}

void bootstrap()
{
    if (setup_done || base)
    {
        return;
    }
    poll_for_game(0);
    retry_schedule schedule{kRetryInitialIntervalSeconds, kRetryInitialIntervalSeconds};
    schedule_retry(schedule);
}

}

__attribute__((constructor)) static void start()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ dispatch_async(dispatch_get_main_queue(), ^{ bootstrap(); }); });
}
