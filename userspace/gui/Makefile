TARGET := gui

LIBC_DIR := ../libs/libc
LIBC_INCLUDE_DIR := $(LIBC_DIR)/include
LIBC_STATIC := $(BUILD_DIR)/libs/libc.a

SRC := $(wildcard *.c) ../../libs/microui/src/microui.c
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)
MICROUI := ../../libs/microui/src

CFLAGS += -nostdlib -nodefaultlibs -ffreestanding -O0 -mstackrealign -g -I $(MICROUI) -I $(LIBC_INCLUDE_DIR) -fPIC -Wl,-e,main
LIBS := $(LIBC_STATIC)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC) $(LIBC_STATIC)
	@echo "Compiling $(TARGET) for $(CPU_ARCH) architecture"
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(INITRD_DIR)
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc $(CFLAGS) -o $(OBJ_DIR)/$(TARGET) $(SRC) $(LIBS)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

$(LIBC_STATIC):
	@$(MAKE) -C ../libs BUILD_DIR=$(BUILD_DIR) TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		CPU_ARCH=$(CPU_ARCH) CROSS_PREFIX=$(CROSS_PREFIX) INITRD_DIR=$(INITRD_DIR)

clean:
	@echo "Cleaning $(TARGET)"
	@rm -f $(OBJ_DIR)/$(TARGET)
	@rm -f $(INITRD_DIR)/$(TARGET)
