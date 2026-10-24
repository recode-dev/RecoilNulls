#include <cstdint>
#include <cstddef>
#include <frida-gum.h>

#include "index.hpp"
#include "core/offsets.hpp"
#include "core/functions.hpp"
#include "core/scanner.hpp"
#include "utils/walls.hpp"
#include "features/autododge.hpp"
#include "features/aimbot.hpp"

namespace
{
    bool g_installed = false;
    uintptr_t g_base = 0;
    uintptr_t g_lastBattle = 0;

    bool containsNoCase(const char *haystack, const char *needle)
    {
        if (haystack == nullptr || needle == nullptr || *needle == 0) return false;
        for (const char *p = haystack; *p != 0; p++)
        {
            const char *a = p;
            const char *b = needle;
            while (*a != 0 && *b != 0)
            {
                char ca = *a;
                char cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
                if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
                if (ca != cb) break;
                a++;
                b++;
            }
            if (*b == 0) return true;
        }
        return false;
    }

    gboolean findModuleCallback(GumModule *module, gpointer userData)
    {
        if (module == nullptr) return TRUE;
        if (!containsNoCase(gum_module_get_name(module), "nulls")) return TRUE;

        const GumMemoryRange *range = gum_module_get_range(module);
        if (range == nullptr) return TRUE;

        *(uintptr_t *)userData = (uintptr_t)range->base_address;
        return FALSE;
    }

    uintptr_t findBase()
    {
        uintptr_t found = 0;
        gum_process_enumerate_modules(findModuleCallback, &found);
        return found;
    }

    typedef struct _UpdateListener UpdateListener;
    typedef struct _UpdateListenerClass UpdateListenerClass;

    struct _UpdateListener
    {
        GObject parent;
    };

    struct _UpdateListenerClass
    {
        GObjectClass parent_class;
    };

    static void update_listener_iface_init(gpointer g_iface, gpointer iface_data);

    static void update_listener_init(UpdateListener *self)
    {
        (void)self;
    }

    static void update_listener_class_init(UpdateListenerClass *klass)
    {
        (void)klass;
    }

    static void update_listener_on_enter(GumInvocationListener *listener, GumInvocationContext *context)
    {
        (void)listener;

        const uintptr_t battleMode = (uintptr_t)gum_invocation_context_get_nth_argument(context, 0);
        if (battleMode == 0) return;

        const uint32_t now = scanner::nowMs();

        if (battleMode != g_lastBattle)
        {
            g_lastBattle = battleMode;
            scanner::reset();
            autododge::reset();
            aimbot::resetAimbot();
            walls::notifyBattleModeChanged(battleMode);
        }

        walls::maybeRefreshWallCache(battleMode, now);
        scanner::update(battleMode, now, module::state::autododge);
        if (module::state::autododge) autododge::update(now);
        aimbot::updateAimbot(module::state::aimbot);
    }

    static void update_listener_iface_init(gpointer g_iface, gpointer iface_data)
    {
        (void)iface_data;

        GumInvocationListenerInterface *iface = static_cast<GumInvocationListenerInterface *>(g_iface);
        iface->on_enter = update_listener_on_enter;
    }

    G_DEFINE_TYPE_EXTENDED(UpdateListener, update_listener, G_TYPE_OBJECT, 0,
        G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER, update_listener_iface_init))

    UpdateListener *g_updateListener = nullptr;
}

bool module::install()
{
    if (g_installed) return true;

    gum_init_embedded();

    const uintptr_t base = findBase();
    if (base == 0) return false;
    if (!functions::init(base)) return false;

    g_base = base;
    scanner::init(base);
    scanner::enableProjectileTracking();
    walls::watchTileChanges(base);
    autododge::setup();
    aimbot::setupAimbot(base);

    GumInterceptor *interceptor = gum_interceptor_obtain();
    g_updateListener = reinterpret_cast<UpdateListener *>(g_object_new(update_listener_get_type(), NULL));
    const GumAttachReturn attachResult = gum_interceptor_attach(interceptor,
        GSIZE_TO_POINTER(base + offsets::LogicBattleModeClient_update),
        GUM_INVOCATION_LISTENER(g_updateListener),
        NULL);

    if (attachResult != GUM_ATTACH_OK) return false;

    g_installed = true;
    return true;
}

bool module::isInstalled()
{
    return g_installed;
}

uintptr_t module::baseAddress()
{
    return g_base;
}

__attribute__((constructor)) static void module_entry()
{
    module::install();
}

extern "C" __attribute__((used, visibility("default"))) int _init_module()
{
    return 0;
}
