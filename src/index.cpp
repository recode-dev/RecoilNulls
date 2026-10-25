#include <cstdint>
#include <cstddef>
#include <pthread.h>
#include <unistd.h>
#include <objc/runtime.h>

#include "../tools/hook.hpp"
#include "../tools/image.hpp"
#include "../tools/memory.hpp"
#include "../tools/swizzle.hpp"
#include "module.hpp"
#include "core/offsets.hpp"
#include "core/functions.hpp"
#include "core/scan.hpp"
#include "features/autoaim.hpp"
#include "features/autododge.hpp"
#include "utils/walls.hpp"

namespace
{
    constexpr uint32_t POLL_INTERVAL_MS = 500;
    constexpr uint32_t POLL_LIMIT = 1200;      // ~10 minutes
    constexpr uint32_t PUMP_MIN_GAP_MS = 4;    // <=250 fps of work

    using SlotFn = uint64_t (*)(void *, uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t, uint64_t);

    bool g_installed = false;
    uintptr_t g_base = 0;
    uintptr_t g_lastBattle = 0;
    uint32_t g_lastPump = 0;

    SlotFn g_origTick = nullptr;
    IMP g_origRender = nullptr;

    void pump_locked(uintptr_t client)
    {
        if (g_base == 0) return;

        // Combat filter: state must be 5, otherwise nothing but the cheap read
        // below ever runs.
        if (scanner::getBattleMode() == 0) return;

        const uint32_t now = scanner::nowMs();
        if (g_lastPump != 0 && (now - g_lastPump) < PUMP_MIN_GAP_MS) return;
        g_lastPump = now;

        if (client == 0) client = scanner::getBattleMode();
        if (client == 0) return;

        if (client != g_lastBattle)
        {
            g_lastBattle = client;
            scanner::reset();
            autododge::reset();
            autoaim::resetAutoaim();
            walls::notifyBattleModeChanged(client);
        }

        scanner::update(client, now, module::state::autododge);

        if (module::state::autoaim) autoaim::updateAutoaim(true);
        walls::maybeRefreshWallCache(client, now);
        if (module::state::autododge) autododge::update(now);
    }

    // Slot replacement for LogicBattleModeClient::update.
    uint64_t tick(void *self, uint64_t a1, uint64_t a2, uint64_t a3,
                  uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
    {
        pump_locked((uintptr_t)self);

        if (g_origTick != nullptr) return g_origTick(self, a1, a2, a3, a4, a5, a6, a7);

        return 0;
    }

    // Fallback driver: the view's render pass, used when the update slot cannot
    // be swapped on this build.
    void render_hook(id self, SEL cmd)
    {
        pump_locked((uintptr_t)self);

        if (g_origRender != nullptr) ((void (*)(id, SEL))g_origRender)(self, cmd);
    }

    void *poll_thread(void *)
    {
        for (uint32_t tickIndex = 0; tickIndex < POLL_LIMIT; tickIndex++)
        {
            const uintptr_t base = tools::image::base_named("nulls");

            if (base != 0)
            {
                module::install(base);
                return nullptr;
            }

            usleep(POLL_INTERVAL_MS * 1000);
        }

        return nullptr;
    }
}

bool module::install(uintptr_t base)
{
    if (g_installed || base == 0) return false;

    g_base = base;

    if (!functions::init(base)) return false;

    scanner::init(base);

    // Frame driver. Preferred: swap the battle client's update slot.
    g_origTick = reinterpret_cast<SlotFn>(
        tools::hook::install(base, offsets::LogicBattleModeClient_update, reinterpret_cast<void *>(&tick)));

    // Fallback (and extra frame source): the ObjC view render pass.
    if (g_origTick == nullptr)
    {
        g_origRender = (IMP)tools::swizzle::install("NullView", "render", (void *)&render_hook);
    }

    scanner::enableProjectileTracking();
    walls::watchTileChanges(base);
    autododge::setup();
    autoaim::setupAutoaim(base);

    g_installed = true;

    return true;
}

bool module::is_installed()
{
    return g_installed;
}

uintptr_t module::base_address()
{
    return g_base;
}

void module::pump(uintptr_t client)
{
    pump_locked(client);
}

void module::pump()
{
    pump_locked(0);
}

__attribute__((constructor)) static void module_entry()
{
    // Never block the loader: the game image is not mapped yet at constructor
    // time, so wait for it on a detached thread.
    pthread_t thread;
    if (pthread_create(&thread, nullptr, poll_thread, nullptr) == 0)
    {
        pthread_detach(thread);
    }
}
