TARGET := iphone:clang:latest:15.0
ARCHS := arm64

FRIDA_VERSION ?= 17.24.3

SRC_DIR := src
GUM_DIR := vendor/frida-gum
GUM_INC := $(firstword $(wildcard $(GUM_DIR)/include $(GUM_DIR)))
GUM_LIB := $(firstword $(wildcard $(GUM_DIR)/lib $(GUM_DIR)))

ifeq ($(strip $(GUM_INC)),)
GUM_INC := $(GUM_DIR)/include
endif

ifeq ($(strip $(GUM_LIB)),)
GUM_LIB := $(GUM_DIR)/lib
endif

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find $(SRC_DIR) -name "*.cpp" | sort)

RECOIL_OPTIMIZE := -Os -fomit-frame-pointer -fvisibility=hidden -fvisibility-inlines-hidden \
                   -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables
RECOIL_INCLUDE := -I$(SRC_DIR) -I$(GUM_INC)
RECOIL_WARNINGS := -Wall -Wextra -Wno-unused-parameter

RECOIL_FLAGS := $(RECOIL_OPTIMIZE) $(RECOIL_WARNINGS) $(RECOIL_INCLUDE)

Recoil_CFLAGS := $(RECOIL_FLAGS)
Recoil_CCFLAGS := -std=c++17 $(RECOIL_FLAGS)
Recoil_OBJCFLAGS := -fobjc-arc -std=c++17 $(RECOIL_FLAGS)
Recoil_LDFLAGS := -L$(GUM_LIB) -lfrida-gum -Wl,-undefined,dynamic_lookup -Wl,-dead_strip -Wl,-x -Wl,-S

include $(THEOS_MAKE_PATH)/tweak.mk

.PHONY: vendor
vendor:
	@mkdir -p $(GUM_DIR)
	@curl -fsSL -o /tmp/frida-gum-devkit.tar.xz https://github.com/frida/frida/releases/download/$(FRIDA_VERSION)/frida-gum-devkit-$(FRIDA_VERSION)-ios-arm64.tar.xz
	@tar -xJf /tmp/frida-gum-devkit.tar.xz -C $(GUM_DIR)
	@ls -la $(GUM_DIR)
