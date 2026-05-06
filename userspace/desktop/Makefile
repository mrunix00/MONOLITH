TARGET := desktop

APP_OPTFLAGS := -Ofast
APP_LIB_DEPS := libgfx libc
EXTRA_INCLUDE_DIRS := ../libs/libdesktop/include
LIBS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))

include ../common-app.mk
