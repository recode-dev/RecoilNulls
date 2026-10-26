TARGET := iphone:clang:latest:15.0
ARCHS := arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

FILES := $(shell find src -name "*.cpp" | sort)
FILES += $(shell find tools \( -name "*.c" -o -name "*.cpp" \) | sort)

INCLUDE_FLAGS := -Isrc -Itools

CFLAGS := -Wall -Wextra $(INCLUDE_FLAGS)
CCFLAGS := -std=c++17 -fblocks -Wall -Wextra $(INCLUDE_FLAGS)

FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
