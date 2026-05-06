TARGET := gfxdemo

APP_OPTFLAGS := -Ofast
APP_LIB_DEPS := libdesktop libgfx libc
LIBS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))

include ../common-app.mk
