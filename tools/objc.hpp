#pragma once

#include <cstdint>
#include <cstddef>
#include <objc/runtime.h>

// Objective-C runtime helpers.
//
// Thin, allocation-free wrappers around the runtime so the rest of the tweak can
// look up classes and swap method implementations without pulling in Foundation.

namespace tools::objc
{
    Class find_class(const char *name);

    // Instance method, resolved through the class hierarchy.
    Method find_method(Class cls, const char *selector, bool classMethod = false);

    IMP implementation(Class cls, const char *selector, bool classMethod = false);

    // Installs `replacement` for cls/selector and returns the previous IMP.
    // Adds the method to the class when it only exists on a superclass.
    IMP swap(Class cls, const char *selector, IMP replacement, bool classMethod = false);

    // Restores an IMP captured by swap().
    bool restore(Class cls, const char *selector, IMP original, bool classMethod = false);

    // Argument type encoding of an existing method ("v@:@@"), or nullptr.
    const char *type_encoding(Class cls, const char *selector, bool classMethod = false);
}
