#include "./core/offsets.h"
#include "hook.h"
#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/vm_map.h>
#include <math.h>
#include <objc/runtime.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if __has_include(<ptrauth.h>)
#    include <ptrauth.h>
#endif
#include "./core/commands.h"
#include "./core/scan.h"
#include "./features/autododge.h"
#include "./utils/crypto.h"
#include "./utils/walls.h"
#include "objc.h"

__attribute__((constructor)) void start(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{ poll_for_game(0); });
}
