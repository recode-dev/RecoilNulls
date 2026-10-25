TARGET := iphone:clang:latest:15.0
ARCHS := arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

# src/   — game logic (core, features, utils)
# tools/ — hooking primitives (image, memory, hook, objc, swizzle)
Recoil_FILES := $(shell find src tools -name "*.cpp" | sort)

RECOIL_INCLUDE_FLAGS := -Isrc -Itools
RECOIL_WARNING_FLAGS := -Wall -Wextra
RECOIL_HARDEN_FLAGS := -Os -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-ident -fno-stack-protector
RECOIL_LINK_HARDEN := -Wl,-dead_strip -Wl,-x -Wl,-S -Wl,-exported_symbols_list,/dev/null

Recoil_CCFLAGS := -std=c++17 -fno-exceptions -fno-rtti $(RECOIL_WARNING_FLAGS) $(RECOIL_HARDEN_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_LDFLAGS := $(RECOIL_LINK_HARDEN)
Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
