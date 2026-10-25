TARGET := iphone:clang:latest:15.0
ARCHS := arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find src -name "*.mm" | sort)
Recoil_FILES += $(shell find tools \( -name "*.c" -o -name "*.mm" \) | sort)

RECOIL_INCLUDE_FLAGS := -Isrc -Itools
RECOIL_WARNING_FLAGS := -Wall -Wextra

Recoil_CFLAGS := $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_OBJCFLAGS := -fobjc-arc -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)

Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
