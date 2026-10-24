TARGET := iphone:clang:latest:15.0
ARCHS := arm64

RECOIL_HOOK_DIR := Recoil/recoil_hook
RECOIL_GUM_DIR := Recoil/vendor/frida-gum

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find Recoil -name "*.cpp" | sort)
Recoil_FILES += $(shell find $(RECOIL_HOOK_DIR) -name "*.c" -o -name "*.mm" 2>/dev/null | sort)

RECOIL_INCLUDE_FLAGS := -IRecoil -I$(RECOIL_HOOK_DIR) -I$(RECOIL_GUM_DIR)/include
RECOIL_WARNING_FLAGS := -Wall -Wextra

Recoil_CFLAGS := $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_CCFLAGS := -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_OBJCFLAGS := -fobjc-arc -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_LDFLAGS := -L$(RECOIL_GUM_DIR)/lib -lfrida-gum -Wl,-undefined,dynamic_lookup

Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
