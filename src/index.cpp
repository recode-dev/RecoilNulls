#include <cstdint>
#include <pthread.h>
#include <unistd.h>
#include <objc/runtime.h>

#include "../tools/hook.hpp"
#include "../tools/image.hpp"
#include "../tools/memory.hpp"
#include "../tools/swizzle.hpp"
#include "../tools/module.hpp"
#include "core/offsets.hpp"
#include "core/functions.hpp"
#include "core/scan.hpp"
#include "features/autoaim.hpp"
#include "features/autododge.hpp"
#include "utils/walls.hpp"

namespace
{
    constexpr uint32_t POLL_INTERVAL_MS = 500;
    constexpr uint32_t POLL_LIMIT = 1200;
    constexpr uint32_t PUMP_MIN_GAP_MS = 4;

    using SlotFn = uint64_t (*)(void *, uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t, uint64_t);

    bool g_installed = false;
    uintptr_t g_base = 0;
    uintptr_t g_lastBattle = 0;
    uint32_t g_lastPump = 0;

    SlotFn g_origTick = nullptr;
    IMP g_origRender = nullptr;

    void pump(uintptr_t client)
    {
        if (g_base == 0) return;

        if (!scanner::isObject(client)) client = scanner::getBattleMode();
        if (!scanner::isObject(client)) return;

        const uint32_t now = scanner::nowMs();
        if (g_lastPump != 0 && (now - g_lastPump) < PUMP_MIN_GAP_MS) return;
        g_lastPump = now;

        if (client != g_lastBattle)
        {
            g_lastBattle = client;
            scanner::reset();
            autododge::reset();
            autoaim::resetAutoaim();
            if (module::state::walls) walls::notifyBattleModeChanged(client);
        }

        scanner::update(client, now, module::state::autododge);

        if (module::state::autoaim) autoaim::updateAutoaim(true);
        if (module::state::walls) walls::maybeRefreshWallCache(client, now);
        if (module::state::autododge) autododge::update(now);
    }

    uint64_t tick(void *self, uint64_t a1, uint64_t a2, uint64_t a3,
                  uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
    {
        pump((uintptr_t)self);

        if (g_origTick != nullptr) return g_origTick(self, a1, a2, a3, a4, a5, a6, a7);

        return 0;
    }

    void render_hook(id self, SEL cmd)
    {
        pump(0);

        if (g_origRender != nullptr) ((void (*)(id, SEL))g_origRender)(self, cmd);
    }

    void *poll_thread(void *)
    {
        for (uint32_t tickIndex = 0; tickIndex < POLL_LIMIT; tickIndex++)
        {
            const uintptr_t base = tools::image::base_exact("Nulls Brawl");

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

    g_origTick = reinterpret_cast<SlotFn>(
        tools::hook::install(base, offsets::LogicBattleModeClient_update, reinterpret_cast<void *>(&tick)));

    g_origRender = (IMP)tools::swizzle::install("MetalView", "render", (void *)&render_hook);
    if (g_origRender == nullptr)
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

__attribute__((constructor)) static void module_entry()
{
    pthread_t poll;
    if (pthread_create(&poll, nullptr, poll_thread, nullptr) == 0)
    {
        pthread_detach(poll);
    }
}
