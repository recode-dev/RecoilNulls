#include "swizzle.hpp"
#include "objc.hpp"

#include <string.h>

namespace
{
    tools::swizzle::Entry g_targets[tools::swizzle::MAX_TARGETS];

    tools::swizzle::Entry *entry_alloc(Class cls, SEL sel, bool classMethod)
    {
        for (int i = 0; i < tools::swizzle::MAX_TARGETS; i++)
        {
            if (!g_targets[i].used) continue;
            if (g_targets[i].cls != cls) continue;
            if (g_targets[i].classMethod != classMethod) continue;
            if (g_targets[i].sel != sel) continue;

            return &g_targets[i];
        }

        for (int i = 0; i < tools::swizzle::MAX_TARGETS; i++)
        {
            if (g_targets[i].used) continue;

            memset(&g_targets[i], 0, sizeof(g_targets[i]));
            g_targets[i].used = true;
            g_targets[i].cls = cls;
            g_targets[i].sel = sel;
            g_targets[i].classMethod = classMethod;

            return &g_targets[i];
        }

        return nullptr;
    }
}

namespace tools::swizzle
{
    int install(const Target *targets, int count, void **originals)
    {
        if (targets == nullptr || count <= 0) return 0;

        int armed = 0;

        for (int i = 0; i < count && i < MAX_TARGETS; i++)
        {
            if (originals != nullptr) originals[i] = nullptr;

            if (targets[i].cls == nullptr || targets[i].sel == nullptr) continue;
            if (targets[i].replacement == nullptr) continue;

            Class cls = tools::objc::find_class(targets[i].cls);
            if (cls == nullptr) continue;

            const SEL sel = sel_registerName(targets[i].sel);
            if (sel == nullptr) continue;

            Entry *entry = entry_alloc(cls, sel, targets[i].classMethod);
            if (entry == nullptr) continue;

            const IMP previous = tools::objc::swap(cls, targets[i].sel,
                                                   (IMP)targets[i].replacement,
                                                   targets[i].classMethod);
            if (previous == nullptr) continue;

            entry->original = previous;
            entry->replacement = (IMP)targets[i].replacement;
            entry->control = targets[i].control;

            if (originals != nullptr) originals[i] = (void *)previous;

            armed++;
        }

        return armed;
    }

    void *install(const char *cls, const char *sel, void *replacement, bool classMethod)
    {
        Target target;
        target.cls = cls;
        target.sel = sel;
        target.classMethod = classMethod;
        target.replacement = replacement;
        target.control = 0;

        void *originals[1] = { nullptr };
        const int armed = tools::swizzle::install(&target, 1, originals);

        return armed > 0 ? originals[0] : nullptr;
    }

    int uninstall()
    {
        int restored = 0;

        for (int i = 0; i < MAX_TARGETS; i++)
        {
            if (!g_targets[i].used) continue;
            if (g_targets[i].original == nullptr) continue;

            const char *name = sel_getName(g_targets[i].sel);
            if (name == nullptr) continue;

            if (tools::objc::restore(g_targets[i].cls, name, g_targets[i].original, g_targets[i].classMethod))
            {
                restored++;
            }

            memset(&g_targets[i], 0, sizeof(g_targets[i]));
        }

        return restored;
    }

    bool armed(const char *cls, const char *sel)
    {
        if (cls == nullptr || sel == nullptr) return false;

        const Class found = tools::objc::find_class(cls);
        if (found == nullptr) return false;

        for (int i = 0; i < MAX_TARGETS; i++)
        {
            if (!g_targets[i].used) continue;
            if (g_targets[i].cls != found) continue;
            if (strcmp(sel_getName(g_targets[i].sel), sel) != 0) continue;

            return true;
        }

        return false;
    }

    const Entry *entry_at(int index)
    {
        if (index < 0 || index >= MAX_TARGETS) return nullptr;
        if (!g_targets[index].used) return nullptr;

        return &g_targets[index];
    }
}
