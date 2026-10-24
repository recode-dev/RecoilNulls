TARGET := iphone:clang:latest:15.0
ARCHS := arm64

SRC_DIR := src
RECOIL_GUM_DIR := vendor/frida-gum

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find $(SRC_DIR) -name "*.cpp" | sort)

RECOIL_INCLUDE_FLAGS := -I$(SRC_DIR) -I$(RECOIL_GUM_DIR) -I$(RECOIL_GUM_DIR)/include
RECOIL_WARNING_FLAGS := -Wall -Wextra
RECOIL_HARDEN_FLAGS := -Os -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-ident -fno-asynchronous-unwind-tables
RECOIL_LINK_HARDEN := -Wl,-dead_strip -Wl,-x -Wl,-S -Wl,-exported_symbol,_recoil_nonexport

Recoil_CFLAGS := $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_CCFLAGS := -std=c++17 -fno-exceptions -fno-rtti $(RECOIL_WARNING_FLAGS) $(RECOIL_HARDEN_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_OBJCFLAGS := -fobjc-arc -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_HARDEN_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_LDFLAGS := $(RECOIL_LINK_HARDEN) -L$(RECOIL_GUM_DIR) -L$(RECOIL_GUM_DIR)/lib -lfrida-gum -Wl,-undefined,dynamic_lookup

Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
