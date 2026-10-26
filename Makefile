TARGET := iphone:clang:latest:15.0
ARCHS := arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find src -name "*.cpp" | sort)
Recoil_FILES += $(shell find tools \( -name "*.c" -o -name "*.cpp" \) | sort)

RECOIL_INCLUDE_FLAGS := -Isrc -Itools

Recoil_CFLAGS := -Wall -Wextra $(RECOIL_INCLUDE_FLAGS)
# -fblocks keeps the dispatch block literals compiling in plain C++.
Recoil_CCFLAGS := -std=c++17 -fblocks -Wall -Wextra $(RECOIL_INCLUDE_FLAGS)

# Foundation stays linked only so libobjc (used by tools/objc.cpp) resolves;
# no Objective-C source remains in the tweak.
Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
