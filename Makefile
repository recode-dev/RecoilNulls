TARGET := iphone:clang:latest:15.0
ARCHS := arm64

FRIDA_VERSION ?= 17.24.3

RECOIL_HOOK_DIR := Recoil/recoil_hook
RECOIL_GUM_DIR := Recoil/vendor/frida-gum
RECOIL_GUM_INC := $(firstword $(wildcard $(RECOIL_GUM_DIR)/include $(RECOIL_GUM_DIR)))
RECOIL_GUM_LIB := $(firstword $(wildcard $(RECOIL_GUM_DIR)/lib $(RECOIL_GUM_DIR)))

ifeq ($(strip $(RECOIL_GUM_INC)),)
RECOIL_GUM_INC := $(RECOIL_GUM_DIR)/include
endif

ifeq ($(strip $(RECOIL_GUM_LIB)),)
RECOIL_GUM_LIB := $(RECOIL_GUM_DIR)/lib
endif

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find Recoil -name "*.cpp" | sort)
Recoil_FILES += $(shell find $(RECOIL_HOOK_DIR) -name "*.c" -o -name "*.mm" 2>/dev/null | sort)

RECOIL_INCLUDE_FLAGS := -IRecoil -I$(RECOIL_HOOK_DIR) -I$(RECOIL_GUM_INC)
RECOIL_WARNING_FLAGS := -Wall -Wextra

Recoil_CFLAGS := $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_CCFLAGS := -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_OBJCFLAGS := -fobjc-arc -std=c++17 $(RECOIL_WARNING_FLAGS) $(RECOIL_INCLUDE_FLAGS)
Recoil_LDFLAGS := -L$(RECOIL_GUM_LIB) -lfrida-gum -Wl,-undefined,dynamic_lookup

Recoil_FRAMEWORKS := Foundation

include $(THEOS_MAKE_PATH)/tweak.mk

.PHONY: vendor
vendor:
	@mkdir -p $(RECOIL_GUM_DIR)
	@curl -fsSL -o /tmp/frida-gum-devkit.tar.xz https://github.com/frida/frida/releases/download/$(FRIDA_VERSION)/frida-gum-devkit-$(FRIDA_VERSION)-ios-arm64.tar.xz
	@tar -xJf /tmp/frida-gum-devkit.tar.xz -C $(RECOIL_GUM_DIR)
	@ls -la $(RECOIL_GUM_DIR)
