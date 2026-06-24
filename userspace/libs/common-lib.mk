ifndef LIB_NAME
$(error LIB_NAME must be set before including common-lib.mk)
endif

SRC_DIR ?= src
SHARED_INCLUDE_DIR ?= $(abspath ../../../shared/include/monolith)
INCLUDE_DIR ?= include
LIB_OBJ_ROOT ?= $(BUILD_DIR)/shared_libs/obj
LIBS_OUTPUT_DIR ?= $(BUILD_DIR)/libs
LIB_OUTPUT_DIR := $(LIBS_OUTPUT_DIR)
OBJ_DIR := $(LIB_OBJ_ROOT)/$(LIB_NAME)
LIB_TARGET := $(LIB_OUTPUT_DIR)/$(LIB_NAME).a

LIB_INCLUDE_DIRS ?= $(INCLUDE_DIR)
LIB_INCLUDE_DIRS += $(SHARED_INCLUDE_DIR)
LIB_OPTFLAGS ?=
LIB_EXTRA_CFLAGS ?=
ARFLAGS ?= rcs

SOURCES ?= $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
AR := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)ar
LIB_CFLAGS := -ffreestanding -Wall -Wextra -fmacro-prefix-map=$(PROJECT_ROOT)/= $(addprefix -I,$(LIB_INCLUDE_DIRS)) $(LIB_OPTFLAGS) $(LIB_EXTRA_CFLAGS)

.PHONY: all clean

-include $(DEPS)

all: $(LIB_TARGET)

$(LIB_TARGET): $(OBJECTS)
	@mkdir -p $(LIB_OUTPUT_DIR)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

$(OBJ_DIR)/%.o: $(CURDIR)/$(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(LIB_CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(LIB_TARGET) $(EXTRA_CLEAN)
