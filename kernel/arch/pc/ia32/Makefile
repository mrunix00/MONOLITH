PROJECT_ROOT := $(abspath $(CURDIR)/../../..)

include $(PROJECT_ROOT)/kernel/arch/pc/ia32/config.mk

OBJ_DIR := $(BUILD_DIR)/obj
KERNEL_DIR := $(PROJECT_ROOT)/kernel
ARCH_DIR := $(PROJECT_ROOT)/kernel/arch/pc/ia32
PC_DIR := $(PROJECT_ROOT)/kernel/arch/pc

CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
KERNEL_LIBGCC := $(shell $(CC) $(ARCH_CFLAGS) -print-libgcc-file-name)

# Sources
PER_ARCH_ASM := $(shell find $(ARCH_DIR) -maxdepth 1 -name "*.asm" 2>/dev/null)
PER_ARCH_C := $(shell find $(ARCH_DIR) -maxdepth 1 -name "*.c" 2>/dev/null)

PC_SHARED_ASM := $(shell find $(PC_DIR) -maxdepth 1 -name "*.asm" 2>/dev/null)
PC_SHARED_C := $(shell find $(PC_DIR) -maxdepth 1 -name "*.c" 2>/dev/null)

GENERIC_ASM := $(shell find $(KERNEL_DIR) -path $(KERNEL_DIR)/arch -prune -o -name "*.asm" -print 2>/dev/null)
GENERIC_C := $(shell find $(KERNEL_DIR) -path $(KERNEL_DIR)/arch -prune -o -name "*.c" -print 2>/dev/null)

PER_ARCH_ASM_OBJ := $(patsubst $(ARCH_DIR)/%.asm,$(OBJ_DIR)/kernel/arch/pc/ia32/%.asm.o,$(PER_ARCH_ASM))
PER_ARCH_C_OBJ := $(patsubst $(ARCH_DIR)/%.c,$(OBJ_DIR)/kernel/arch/pc/ia32/%.c.o,$(PER_ARCH_C))

PC_SHARED_ASM_OBJ := $(patsubst $(PC_DIR)/%.asm,$(OBJ_DIR)/kernel/arch/pc/%.asm.o,$(PC_SHARED_ASM))
PC_SHARED_C_OBJ := $(patsubst $(PC_DIR)/%.c,$(OBJ_DIR)/kernel/arch/pc/%.c.o,$(PC_SHARED_C))

GENERIC_ASM_OBJ := $(patsubst $(KERNEL_DIR)/%.asm,$(OBJ_DIR)/kernel/%.asm.o,$(GENERIC_ASM))
GENERIC_C_OBJ := $(patsubst $(KERNEL_DIR)/%.c,$(OBJ_DIR)/kernel/%.c.o,$(GENERIC_C))

KERNEL_OBJ := $(PER_ARCH_ASM_OBJ) $(PER_ARCH_C_OBJ) $(PC_SHARED_ASM_OBJ) $(PC_SHARED_C_OBJ) $(GENERIC_ASM_OBJ) $(GENERIC_C_OBJ)

CFLAGS := -ffreestanding -g -Wall -Wextra -I$(PROJECT_ROOT) -I$(ARCH_DIR) -std=c99 -fno-stack-protector -fno-stack-check -fno-PIC -fmacro-prefix-map=$(PROJECT_ROOT)/= $(ARCH_CFLAGS)
LDFLAGS := $(LDFLAGS_ARCH) -nostdlib -static --build-id=none

.PHONY: all clean

all: $(KERNEL_OBJ)

$(OBJ_DIR)/kernel/arch/pc/ia32/%.asm.o: $(ARCH_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm -f $(NASM_FORMAT) $< -o $@

$(OBJ_DIR)/kernel/arch/pc/ia32/%.c.o: $(ARCH_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/arch/pc/%.asm.o: $(PC_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm -f $(NASM_FORMAT) $< -o $@

$(OBJ_DIR)/kernel/arch/pc/%.c.o: $(PC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/%.asm.o: $(KERNEL_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm -f $(NASM_FORMAT) $< -o $@

$(OBJ_DIR)/kernel/%.c.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(KERNEL_OBJ)
