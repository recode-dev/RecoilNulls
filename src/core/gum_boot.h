#ifndef GUM_BOOT_H
#define GUM_BOOT_H
#include <frida-gum.h>
int gum_enumerate_modules(GumFoundModuleFunc callback, gpointer userData);
const char* gum_last_error(void);
#endif
