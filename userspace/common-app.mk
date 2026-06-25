ifndef TARGET
$(error TARGET must be set before including common-app.mk)
endif
.DEFAULT_GOAL := all

APP_OPTFLAGS ?= -O2
APP_LIB_DEPS ?= libc
EXTRA_INCLUDE_DIRS ?=
POST_INSTALL_CMD ?= @:
SHARED_INCLUDE_DIR ?= $(abspath ../../shared/include/monolith)
STB_DIR ?= ../../libs/stb

SRC := $(wildcard *.c)
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

APP_LIB_TARGETS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))
APP_INCLUDE_DIRS := $(addprefix ../libs/,$(addsuffix /include,$(APP_LIB_DEPS))) $(SHARED_INCLUDE_DIR) $(STB_DIR) $(EXTRA_INCLUDE_DIRS)

CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
CFLAGS += $(APP_OPTFLAGS) $(addprefix -I,$(APP_INCLUDE_DIRS)) -Wall -Wextra -fmacro-prefix-map=$(PROJECT_ROOT)/=
LDFLAGS := -B$(BUILD_DIR)/libs -nodefaultlibs
LIBS ?= $(APP_LIB_TARGETS)
LIBGCC := $(shell $(CC) -print-libgcc-file-name)

.PHONY: all clean

-include $(DEP)

all: $(TARGET)

$(TARGET): $(OBJ) $(APP_LIB_TARGETS)
	@echo "Linking $(TARGET) for $(ARCH_CPU)"
	@mkdir -p $(INITRD_DIR)
	$(CC) $(LDFLAGS) -o $(OBJ_DIR)/$(TARGET) $(OBJ) $(LIBS) $(LIBGCC)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	$(POST_INSTALL_CMD)
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

$(OBJ_DIR)/%.o: $(CURDIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

define userspace_lib_rule
$(BUILD_DIR)/libs/$(1).a:
	@$(MAKE) -C ../libs/$(1)
endef

$(foreach lib,$(APP_LIB_DEPS),$(eval $(call userspace_lib_rule,$(lib))))

clean:
	@echo "Cleaning $(TARGET)"
	@rm -rf $(OBJ_DIR)
	@rm -f $(INITRD_DIR)/$(TARGET)
