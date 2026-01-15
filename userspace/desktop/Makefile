TARGET := desktop

LIBC_DIR := ../libs/libc
LIBC_INCLUDE_DIR := $(LIBC_DIR)/include
LIBC_STATIC := $(BUILD_DIR)/libs/libc.a

LIBGFX_DIR := ../libs/libgfx
LIBGFX_INCLUDE_DIR := $(LIBGFX_DIR)/include
LIBGFX_STATIC := $(BUILD_DIR)/libs/libgfx.a

SRC := $(wildcard *.c)
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

CFLAGS += -O2 -g -I $(LIBC_INCLUDE_DIR) -I $(LIBGFX_INCLUDE_DIR) -Wall -Wextra
LDFLAGS := -nostdlib -nodefaultlibs -Wl,-e,main
LIBS := $(LIBGFX_STATIC) $(LIBC_STATIC)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ) $(LIBGFX_STATIC) $(LIBC_STATIC)
	@echo "Linking $(TARGET) for $(CPU_ARCH) architecture"
	@mkdir -p $(INITRD_DIR)
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc $(LDFLAGS) -o $(OBJ_DIR)/$(TARGET) $(OBJ) $(LIBS)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "Compiling $<"
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(LIBC_STATIC):
	@$(MAKE) -C ../libs/libc BUILD_DIR=$(BUILD_DIR) TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		CPU_ARCH=$(CPU_ARCH) CROSS_PREFIX=$(CROSS_PREFIX) INITRD_DIR=$(INITRD_DIR)

$(LIBGFX_STATIC):
	@$(MAKE) -C ../libs/libgfx BUILD_DIR=$(BUILD_DIR) TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		CPU_ARCH=$(CPU_ARCH) CROSS_PREFIX=$(CROSS_PREFIX) INITRD_DIR=$(INITRD_DIR)

clean:
	@echo "Cleaning $(TARGET)"
	@rm -rf $(OBJ_DIR)
	@rm -f $(INITRD_DIR)/$(TARGET)
