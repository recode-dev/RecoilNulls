#include "./core/scan.h"
#include <dispatch/dispatch.h>

namespace
{

constexpr double kFirstAttemptDelaySeconds = 1.0;
constexpr double kSupervisorIntervalSeconds = 30.0;
constexpr double kPollBudgetSeconds = 620.0;

void supervise(double waited_seconds)
{
    if (setup_done)
    {
        return;
    }
    if (!base && waited_seconds >= kPollBudgetSeconds)
    {
        poll_for_game(0);
        waited_seconds = 0.0;
    }
    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
            static_cast<int64_t>(kSupervisorIntervalSeconds * NSEC_PER_SEC)),
        dispatch_get_main_queue(),
        ^{ supervise(waited_seconds + kSupervisorIntervalSeconds); });
}

void kick()
{
    if (setup_done)
    {
        return;
    }
    if (!base)
    {
        poll_for_game(0);
    }
    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
            static_cast<int64_t>(kSupervisorIntervalSeconds * NSEC_PER_SEC)),
        dispatch_get_main_queue(),
        ^{ supervise(kSupervisorIntervalSeconds); });
}

}

__attribute__((constructor)) static void start()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      dispatch_after(
          dispatch_time(
              DISPATCH_TIME_NOW,
              static_cast<int64_t>(kFirstAttemptDelaySeconds * NSEC_PER_SEC)),
          dispatch_get_main_queue(),
          ^{ kick(); });
    });
}
