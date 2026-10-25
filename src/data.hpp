#pragma once

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <mach/mach.h>
#import <mach/vm_map.h>
#import <mach/mach_time.h>
#import <mach-o/dyld.h>
#import <mach-o/loader.h>
#import <dispatch/dispatch.h>
#import <math.h>
#import <stdarg.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#include "./core/offsets.h"
#include "hook.h"

#if __has_include(<ptrauth.h>)
#import <ptrauth.h>
#endif

#include "./utils/crypto.h"
#include "./utils/walls.h"

#include "objc.h"
#include "./core/scan.h"

#include "./features/autododge.h"

#include "./core/commands.h"
