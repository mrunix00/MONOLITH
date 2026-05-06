TARGET := doom

APP_OPTFLAGS := -Ofast --std=c99
APP_LIB_DEPS := libdesktop libgfx libc
EXTRA_INCLUDE_DIRS := ../../libs/PureDOOM
POST_INSTALL_CMD := @mkdir -p $(INITRD_DIR)/assets && cp ../../libs/PureDOOM/*.wad $(INITRD_DIR)/assets/ 2>/dev/null || true
LIBS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))

include ../common-app.mk
