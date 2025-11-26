OBJ_DIR := $(BUILD_DIR)/obj

# Note: CPU_ARCH and CROSS_PREFIX are now imported from parent Makefile
# No need to recompute them here

# Use the imported TOOLCHAIN_BIN
CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
AS := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)as

# Find all kernel source files, excluding arch directory
ASM_SRC := $(shell find . -path "./arch" -prune -o -name "*.asm" -print 2>/dev/null)
C_SRC := $(shell find . -path "./arch" -prune -o -name "*.c" -print 2>/dev/null)

# Find architecture-specific source files
ARCH_ASM_SRC := $(shell find ./arch/pc/$(CPU_ARCH) -name "*.asm" 2>/dev/null)
ARCH_C_SRC := $(shell find ./arch/pc/$(CPU_ARCH) -name "*.c" 2>/dev/null)
ARCH_PC_ASM_SRC := $(shell find ./arch/pc -maxdepth 1 -name "*.asm" 2>/dev/null)
ARCH_PC_C_SRC := $(shell find ./arch/pc -maxdepth 1 -name "*.c" 2>/dev/null)

# Generate object file paths
ASM_OBJ := $(patsubst %.asm,$(OBJ_DIR)/kernel/%.asm.o,$(ASM_SRC))
C_OBJ := $(patsubst %.c,$(OBJ_DIR)/kernel/%.c.o,$(C_SRC))
ARCH_ASM_OBJ := $(patsubst %.asm,$(OBJ_DIR)/kernel/%.asm.o,$(ARCH_ASM_SRC))
ARCH_C_OBJ := $(patsubst %.c,$(OBJ_DIR)/kernel/%.c.o,$(ARCH_C_SRC))
ARCH_PC_ASM_OBJ := $(patsubst %.asm,$(OBJ_DIR)/kernel/%.asm.o,$(ARCH_PC_ASM_SRC))
ARCH_PC_C_OBJ := $(patsubst %.c,$(OBJ_DIR)/kernel/%.c.o,$(ARCH_PC_C_SRC))
KERNEL_OBJ := $(ASM_OBJ) $(C_OBJ) $(ARCH_ASM_OBJ) $(ARCH_C_OBJ) $(ARCH_PC_ASM_OBJ) $(ARCH_PC_C_OBJ) $(PANIC_IMG_OBJ)

# Compiler flags
CFLAGS := -ffreestanding -g -Wall -Wextra -I../ -std=c99 -fno-stack-protector -fno-stack-check -fno-PIC
ifeq ($(CPU_ARCH),x86_64)
    CFLAGS += -mno-red-zone -mcmodel=kernel
endif

.PHONY: all clean show-info

all: $(KERNEL_OBJ)
	@echo "Kernel files for $(CPU_ARCH) built successfully"

# Create object output directories
define create_dir
	@mkdir -p $(dir $@)
endef

# Compile assembly
$(OBJ_DIR)/kernel/%.asm.o: %.asm
	$(create_dir)
	nasm -f elf64 $< -o $@

# Compile C files
$(OBJ_DIR)/kernel/%.c.o: %.c
	$(create_dir)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(ASM_OBJ) $(C_OBJ) $(ARCH_ASM_OBJ) $(ARCH_C_OBJ) $(ARCH_PC_ASM_OBJ) $(ARCH_PC_C_OBJ) $(PANIC_IMG_OBJ)

# Display toolchain info
show-info:
	@echo "Architecture: $(ARCH)"
	@echo "CPU Architecture: $(CPU_ARCH)"
	@echo "CROSS_PREFIX: $(CROSS_PREFIX)"
	@echo "TOOLCHAIN_BIN: $(TOOLCHAIN_BIN)"
	@echo "CC: $(CC)"
	@echo "AS: $(AS)"
