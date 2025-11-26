TARGET := helloworld

SRC := main.c
OBJ_DIR := $(BUILD_DIR)/obj/userspace/$(TARGET)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "Compiling $(TARGET) for $(CPU_ARCH) architecture"
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(INITRD_DIR)
	$(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc -nostdlib -nostartfiles -nodefaultlibs -ffreestanding -O3 -o $(OBJ_DIR)/$(TARGET) $(SRC)
	@cp $(OBJ_DIR)/$(TARGET) $(INITRD_DIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(INITRD_DIR)"

clean:
	@echo "Cleaning $(TARGET)"
	@rm -f $(OBJ_DIR)/$(TARGET)
	@rm -f $(INITRD_DIR)/$(TARGET)
