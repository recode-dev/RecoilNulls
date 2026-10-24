TARGET := iphone:clang:latest:15.0
ARCHS := arm64

RECOIL_DIR := Recoil
RECOIL_GUM_DIR := $(RECOIL_DIR)/vendor/frida-gum

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find $(RECOIL_DIR) -name "*.cpp" | sort)

RECOIL_INCLUDE_FLAGS := -I$(RECOIL_DIR) -I$(RECOIL_GUM_DIR)/include
RECOIL_WARNING_FLAGS := -Wall -Wextra

Recoil_CFLAGS := $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_CCFLAGS := -std=c++17 -fno-exceptions $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_LDFLAGS := -L$(RECOIL_GUM_DIR)/lib -lfrida-gum -Wl,-undefined,dynamic_lookup

Recoil_FRAMEWORKS := Foundation CoreFoundation

include $(THEOS_MAKE_PATH)/tweak.mk
