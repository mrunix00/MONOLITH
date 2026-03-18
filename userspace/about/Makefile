TARGET := about

APP_OPTFLAGS := -O2 -g
APP_LIB_DEPS := libdesktop libgfx libc
LIBS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))

include ../common-app.mk
