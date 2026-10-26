TARGET := iphone:clang:latest:15.0
ARCHS := arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find src -name "*.cpp" | sort)
Recoil_FILES += $(shell find tools \( -name "*.c" -o -name "*.cpp" \) | sort)

INCLUDE_FLAGS := -Isrc -Itools

Recoil_CFLAGS := -Wall -Wextra $(INCLUDE_FLAGS)
Recoil_CCFLAGS := -std=c++17 -fblocks -Wall -Wextra $(INCLUDE_FLAGS)

Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
