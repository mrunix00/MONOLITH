TARGET := gui

SRC := *.c libc/*.c ../../libs/microui/src/microui.c
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)

MICROUI := ../../libs/microui/src
CFLAGS += -static -nostdlib -nodefaultlibs -ffreestanding -O0 -mstackrealign -g -I $(MICROUI) -Ilibc/ -Wl,-e,main

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "Compiling $(TARGET) for $(CPU_ARCH) architecture"
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(INITRD_DIR)
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc $(CFLAGS) -o $(OBJ_DIR)/$(TARGET) $(SRC)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

clean:
	@echo "Cleaning $(TARGET)"
	@rm -f $(OBJ_DIR)/$(TARGET)
	@rm -f $(INITRD_DIR)/$(TARGET)
