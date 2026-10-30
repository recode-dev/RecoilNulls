TARGET := iphone:clang:latest:15.0
ARCHS := arm64

SRC_DIR := src

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find $(SRC_DIR) -name "*.cpp" | sort)

RECOIL_INCLUDE_FLAGS := -I$(SRC_DIR) -I$(SRC_DIR)/tools
RECOIL_WARNING_FLAGS := -Wall -Wextra -Wno-ambiguous-macro
RECOIL_HARDEN_FLAGS := -Os -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-ident -fno-stack-protector
RECOIL_LINK_HARDEN := -Wl,-dead_strip -Wl,-x -Wl,-S -Wl,-exported_symbols_list,/dev/null -Wl,-install_name,/usr/lib/libsystem_support.dylib

Recoil_CFLAGS := $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_CCFLAGS := -std=c++17 -fno-exceptions -fno-rtti $(RECOIL_WARNING_FLAGS) $(RECOIL_HARDEN_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_OBJCFLAGS := -fobjc-arc -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_HARDEN_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_LDFLAGS := $(RECOIL_LINK_HARDEN) -Wl,-undefined,dynamic_lookup

include $(THEOS_MAKE_PATH)/tweak.mk
