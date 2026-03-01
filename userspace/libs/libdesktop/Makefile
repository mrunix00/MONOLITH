LIB_NAME := libdesktop
SRC_DIR := src
INCLUDE_DIR := include
LIBC_INCLUDE_DIR := ../libc/include
LIBGFX_INCLUDE_DIR := ../libgfx/include
LIB_OBJ_ROOT := $(BUILD_DIR)/shared_libs/obj
OBJ_DIR := $(LIB_OBJ_ROOT)/$(LIB_NAME)
LIBS_OUTPUT_DIR ?= $(BUILD_DIR)/libs
LIB_OUTPUT_DIR := $(LIBS_OUTPUT_DIR)
LIB_TARGET := $(LIB_OUTPUT_DIR)/$(LIB_NAME).a
STB_DIR := ../../../libs/stb

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
AR := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)ar
LIB_CFLAGS := -ffreestanding -nostdlib -nostdinc -nodefaultlibs -Wall -Wextra -I$(INCLUDE_DIR) -I$(LIBC_INCLUDE_DIR) -I$(LIBGFX_INCLUDE_DIR) -I$(STB_DIR) -Ofast -g
ARFLAGS := rcs

.PHONY: all clean

all: $(LIB_TARGET)

$(LIB_TARGET): $(OBJECTS)
	@mkdir -p $(LIB_OUTPUT_DIR)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@rm -f $(LIB_TARGET)
