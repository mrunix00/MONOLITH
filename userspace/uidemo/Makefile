TARGET := uidemo

APP_OPTFLAGS := -O3
APP_LIB_DEPS := libui libdesktop libgfx libc
LIBS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))

include ../common-app.mk
