#include "./core/scan.h"
#include <mach-o/dyld.h>
static bool started;
static void poll_for_image(const mach_header*, intptr_t)
{
    if (started || setup_done) return;
    started = true;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{ poll_for_game(0); });
}
__attribute__((constructor)) static void start_on_image_load()
{
    _dyld_register_func_for_add_image(poll_for_image);
}
