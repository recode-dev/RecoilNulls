#include "objc.hpp"

#include <objc/message.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    Class class_handle(Class cls, bool classMethod)
    {
        if (cls == nullptr) return nullptr;
        if (!classMethod) return cls;

        // Class methods live on the metaclass.
        return object_getClass((id)cls);
    }

    // True when the class itself declares the selector (not a superclass).
    bool class_owns(Class host, SEL sel)
    {
        if (host == nullptr || sel == nullptr) return false;

        unsigned count = 0;
        Method *list = class_copyMethodList(host, &count);
        if (list == nullptr) return false;

        bool found = false;
        for (unsigned i = 0; i < count; i++)
        {
            if (method_getName(list[i]) == sel)
            {
                found = true;
                break;
            }
        }

        free(list);
        return found;
    }

    Method lookup(Class cls, const char *selector, bool classMethod)
    {
        if (cls == nullptr || selector == nullptr || *selector == 0) return nullptr;

        const SEL sel = sel_registerName(selector);
        if (sel == nullptr) return nullptr;

        if (classMethod) return class_getClassMethod(cls, sel);

        return class_getInstanceMethod(cls, sel);
    }
}

namespace tools::objc
{
    Class find_class(const char *name)
    {
        if (name == nullptr || *name == 0) return nullptr;

        Class cls = objc_getClass(name);
        if (cls != nullptr) return cls;

        return objc_lookUpClass(name);
    }

    Method find_method(Class cls, const char *selector, bool classMethod)
    {
        return lookup(cls, selector, classMethod);
    }

    IMP implementation(Class cls, const char *selector, bool classMethod)
    {
        Method method = lookup(cls, selector, classMethod);
        if (method == nullptr) return nullptr;

        return method_getImplementation(method);
    }

    IMP swap(Class cls, const char *selector, IMP replacement, bool classMethod)
    {
        if (cls == nullptr || selector == nullptr || replacement == nullptr) return nullptr;

        const Class host = class_handle(cls, classMethod);
        if (host == nullptr) return nullptr;

        const SEL sel = sel_registerName(selector);
        if (sel == nullptr) return nullptr;

        Method method = lookup(cls, selector, classMethod);
        if (method == nullptr) return nullptr;

        if (class_owns(host, sel))
        {
            return method_setImplementation(method, replacement);
        }

        // Inherited method: adding an override keeps the superclass untouched
        // and returns the IMP we are shadowing.
        const IMP previous = method_getImplementation(method);
        const char *types = method_getTypeEncoding(method);
        if (types == nullptr) return nullptr;
        if (!class_addMethod(host, sel, replacement, types)) return nullptr;

        return previous;
    }

    bool restore(Class cls, const char *selector, IMP original, bool classMethod)
    {
        if (original == nullptr) return false;

        Method method = lookup(cls, selector, classMethod);
        if (method == nullptr) return false;

        method_setImplementation(method, original);
        return true;
    }

    const char *type_encoding(Class cls, const char *selector, bool classMethod)
    {
        Method method = lookup(cls, selector, classMethod);
        if (method == nullptr) return nullptr;

        return method_getTypeEncoding(method);
    }
}
