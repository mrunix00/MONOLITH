ifndef TARGET
$(error TARGET must be set before including common-app.mk)
endif

APP_OPTFLAGS ?= -O2
APP_LIB_DEPS ?= libc
EXTRA_INCLUDE_DIRS ?=
POST_INSTALL_CMD ?= @:
SHARED_INCLUDE_DIR ?= $(abspath ../../shared/include/monolith)

SRC := $(wildcard *.c)
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

APP_LIB_TARGETS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))
APP_INCLUDE_DIRS := $(addprefix ../libs/,$(addsuffix /include,$(APP_LIB_DEPS))) $(SHARED_INCLUDE_DIR) $(EXTRA_INCLUDE_DIRS)

CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
CFLAGS += $(APP_OPTFLAGS) $(addprefix -I,$(APP_INCLUDE_DIRS)) -Wall -Wextra
LDFLAGS := -B$(BUILD_DIR)/libs -nodefaultlibs
LIBS ?= $(APP_LIB_TARGETS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ) $(APP_LIB_TARGETS)
	@echo "Linking $(TARGET) for x86_64"
	@mkdir -p $(INITRD_DIR)
	$(CC) $(LDFLAGS) -o $(OBJ_DIR)/$(TARGET) $(OBJ) $(LIBS)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	$(POST_INSTALL_CMD)
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

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
