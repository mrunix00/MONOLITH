TARGET := doom

LIBC_DIR := ../libs/libc
LIBC_INCLUDE_DIR := $(LIBC_DIR)/include
LIBC_STATIC := $(BUILD_DIR)/libs/libc.a

LIBDESKTOP_DIR := ../libs/libdesktop
LIBDESKTOP_INCLUDE_DIR := $(LIBDESKTOP_DIR)/include
LIBDESKTOP_STATIC := $(BUILD_DIR)/libs/libdesktop.a

LIBGFX_DIR := ../libs/libgfx
LIBGFX_INCLUDE_DIR := $(LIBGFX_DIR)/include
LIBGFX_STATIC := $(BUILD_DIR)/libs/libgfx.a

PUREDOOM_INCLUDE_DIR := ../../libs/PureDOOM

SRC := $(wildcard *.c)
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

CFLAGS += -Ofast -g -I $(LIBC_INCLUDE_DIR) -I $(LIBDESKTOP_INCLUDE_DIR) -I $(LIBGFX_INCLUDE_DIR) -I $(PUREDOOM_INCLUDE_DIR) -Wall -Wextra
LDFLAGS := -nostdlib -nodefaultlibs -Wl,-e,main
LIBS := $(LIBDESKTOP_STATIC) $(LIBGFX_STATIC) $(LIBC_STATIC)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ) $(LIBDESKTOP_STATIC) $(LIBGFX_STATIC) $(LIBC_STATIC)
	@echo "Linking $(TARGET) for $(CPU_ARCH) architecture"
	@mkdir -p $(INITRD_DIR)
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc $(LDFLAGS) -o $(OBJ_DIR)/$(TARGET) $(OBJ) $(LIBS)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	@mkdir -p $(INITRD_DIR)/assets
	@cp ../../libs/PureDOOM/*.wad $(INITRD_DIR)/assets/ 2>/dev/null || true
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "Compiling $<"
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(LIBC_STATIC):
	@$(MAKE) -C ../libs/libc BUILD_DIR=$(BUILD_DIR) TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		CPU_ARCH=$(CPU_ARCH) CROSS_PREFIX=$(CROSS_PREFIX) INITRD_DIR=$(INITRD_DIR)

$(LIBDESKTOP_STATIC):
	@$(MAKE) -C ../libs/libdesktop BUILD_DIR=$(BUILD_DIR) TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		CPU_ARCH=$(CPU_ARCH) CROSS_PREFIX=$(CROSS_PREFIX) INITRD_DIR=$(INITRD_DIR)

$(LIBGFX_STATIC):
	@$(MAKE) -C ../libs/libgfx BUILD_DIR=$(BUILD_DIR) TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		CPU_ARCH=$(CPU_ARCH) CROSS_PREFIX=$(CROSS_PREFIX) INITRD_DIR=$(INITRD_DIR)

clean:
	@echo "Cleaning $(TARGET)"
	@rm -rf $(OBJ_DIR)
	@rm -f $(INITRD_DIR)/$(TARGET)
