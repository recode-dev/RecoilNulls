ARCHS = arm64
TARGET = iphone:clang:16.5:15.0
INSTALL_TARGET_PROCESSES = SpringBoard

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = Recoil

Recoil_FILES = index.cpp \
               core/functions.cpp \
               core/scstring.cpp \
               core/commands.cpp \
               core/scanner.cpp \
               utils/crypto.cpp \
               utils/walls.cpp \
               features/autododge.cpp \
               features/aimbot.cpp

Recoil_CFLAGS = -std=c++17 -I$(THEOS_PROJECT_DIR)/vendor/include
Recoil_LDFLAGS = -L$(THEOS_PROJECT_DIR)/vendor/lib
Recoil_LIBRARIES = gum
Recoil_FRAMEWORKS = Foundation

include $(THEOS_MAKE_PATH)/tweak.mk
