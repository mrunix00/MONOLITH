TOOLCHAIN_TARGET := x86_64-monolith
TOOLCHAIN_BASE_DIR := $(CURDIR)/toolchain
TOOLCHAIN_DIR := $(TOOLCHAIN_BASE_DIR)/$(TOOLCHAIN_TARGET)
TOOLCHAIN_BIN := $(TOOLCHAIN_DIR)/bin
CROSS_PREFIX := $(TOOLCHAIN_TARGET)-
SHARED_INCLUDE_DIR := $(CURDIR)/shared/include/monolith
CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
LD := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)ld
AS := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)as

# Determine grub-mkrescue command to use
GRUB_MKRESCUE=$(shell command -v grub2-mkrescue)
ifeq (, $(GRUB_MKRESCUE))
	GRUB_MKRESCUE=$(shell command -v grub-mkrescue)
endif

# Directories
BUILD_DIR := $(abspath build)
ISO_DIR := $(BUILD_DIR)/isodir
OBJ_DIR := $(BUILD_DIR)/obj
INITRD_DIR := $(BUILD_DIR)/initrd
INITRD_ASSETS_DIR := $(INITRD_DIR)/assets
CACHE_DIR := $(BUILD_DIR)/cache

# Limine bootloader release
LIMINE_VERSION := v12.3.1
LIMINE_URL := https://github.com/Limine-Bootloader/Limine/releases/download/$(LIMINE_VERSION)/limine-binary.tar.gz
LIMINE_CACHE_DIR := $(CACHE_DIR)/limine-$(LIMINE_VERSION)
LIMINE_TARBALL := $(CACHE_DIR)/limine-binary-$(LIMINE_VERSION).tar.gz
LIMINE_READY := $(LIMINE_CACHE_DIR)/.ready

# Output files
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_BIN_GZ := $(KERNEL_BIN).gz
INITRD_TAR := $(BUILD_DIR)/initrd.tar
INITRD_TAR_GZ := $(INITRD_TAR).gz
ISO_FILE := $(BUILD_DIR)/monolith.iso

# Export variables for submakes
export BUILD_DIR TOOLCHAIN_BIN TOOLCHAIN_DIR TOOLCHAIN_BASE_DIR TOOLCHAIN_TARGET CROSS_PREFIX INITRD_DIR SHARED_INCLUDE_DIR

# Compiler and linker flags
CFLAGS := -ffreestanding -Wall -Wextra -I./ -std=c99 -fno-stack-protector -fno-stack-check -fno-PIC -mno-red-zone -mcmodel=kernel
LDFLAGS += -T boot/pc/x86_64/linker.ld -nostdlib -z max-page-size=0x1000 -static --build-id=none

FLANTERM_SOURCES := libs/flanterm/src/flanterm.c libs/flanterm/src/flanterm_backends/fb.c
FLANTERM_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(FLANTERM_SOURCES))

.PHONY: all clean toolchain iso run run-debug kernel flanterm initrd userspace FORCE

# Default target
all: | $(BUILD_DIR)
	$(MAKE) kernel
	$(MAKE) flanterm
	$(MAKE) userspace
	$(MAKE) initrd
	$(MAKE) $(KERNEL_BIN)
	$(MAKE) $(KERNEL_BIN_GZ)
	$(MAKE) $(INITRD_TAR_GZ)
	$(MAKE) $(ISO_FILE)

# Ensure toolchain is built
toolchain:
	@if [ ! -f "$(CC)" ] || [ ! -f "$(LD)" ]; then \
		echo "Building x86_64 toolchain in $(TOOLCHAIN_DIR)..."; \
		bash ./scripts/build-toolchain.sh; \
	else \
		echo "Toolchain already exists at $(TOOLCHAIN_DIR)"; \
	fi

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(BUILD_DIR)/obj

$(CACHE_DIR): | $(BUILD_DIR)
	mkdir -p $(CACHE_DIR)

$(ISO_DIR)/boot/grub:
	mkdir -p $(ISO_DIR)/boot/grub

# Build kernel components
kernel: toolchain | $(BUILD_DIR)
	$(MAKE) -C kernel

# Build FlanTerm library
flanterm: toolchain | $(BUILD_DIR)
	@mkdir -p $(OBJ_DIR)/libs/flanterm/src/flanterm_backends
	$(CC) $(CFLAGS) -c libs/flanterm/src/flanterm.c -o $(OBJ_DIR)/libs/flanterm/src/flanterm.o
	$(CC) $(CFLAGS) -c libs/flanterm/src/flanterm_backends/fb.c -o $(OBJ_DIR)/libs/flanterm/src/flanterm_backends/fb.o

# Link
$(KERNEL_BIN): kernel flanterm | toolchain
	$(LD) $(LDFLAGS) -o $@ $(shell find $(OBJ_DIR)/kernel $(OBJ_DIR)/libs -type f -name "*.o" 2>/dev/null)

$(KERNEL_BIN_GZ): $(KERNEL_BIN)
	gzip -n -c $< > $@

# Build userspace programs
userspace: | $(BUILD_DIR) $(INITRD_DIR)
	@if [ ! -f "$(CC)" ] || [ ! -f "$(LD)" ]; then \
		echo "Toolchain not found. Building it first..."; \
		$(MAKE) toolchain; \
	fi
	$(MAKE) -C userspace

# Create initrd directory
$(INITRD_DIR): | $(BUILD_DIR)
	@mkdir -p $(INITRD_DIR)

# Create initrd tarball in ustar format
initrd: $(INITRD_TAR)

$(INITRD_TAR): FORCE $(INITRD_DIR) | $(BUILD_DIR)
	@echo "Creating initrd archive in ustar format..."
	@mkdir -p $(INITRD_ASSETS_DIR)
	@cp -f assets/wallpaper.jpg $(INITRD_ASSETS_DIR)/wallpaper.jpg
	@cp -f assets/IBMPlexSans_Condensed-Medium.ttf $(INITRD_ASSETS_DIR)/IBMPlexSans_Condensed-Medium.ttf
	tar --format=ustar -cf $(INITRD_TAR) -C $(INITRD_DIR) .

$(INITRD_TAR_GZ): $(INITRD_TAR)
	gzip -n -c $< > $@

FORCE:

$(LIMINE_TARBALL): | $(CACHE_DIR)
	curl -L --fail --show-error -o $@.tmp $(LIMINE_URL)
	mv $@.tmp $@

$(LIMINE_READY): $(LIMINE_TARBALL)
	rm -rf $(LIMINE_CACHE_DIR)
	mkdir -p $(LIMINE_CACHE_DIR)
	tar -xzf $(LIMINE_TARBALL) --strip-components=1 -C $(LIMINE_CACHE_DIR)
	touch $@

# Create ISO
$(ISO_FILE): $(KERNEL_BIN_GZ) $(INITRD_TAR_GZ) boot/pc/limine.conf $(LIMINE_READY)
	rm -rf build/iso
	mkdir -p build/iso/boot/limine
	cp -v $(KERNEL_BIN_GZ) build/iso/boot
	cp -v $(INITRD_TAR_GZ) build/iso/boot
	cp -v boot/pc/limine.conf $(LIMINE_CACHE_DIR)/limine-bios.sys $(LIMINE_CACHE_DIR)/limine-bios-cd.bin \
	    $(LIMINE_CACHE_DIR)/limine-uefi-cd.bin build/iso/boot/limine
	mkdir -p build/iso/EFI/BOOT
	cp -v $(LIMINE_CACHE_DIR)/BOOTX64.EFI build/iso/EFI/BOOT
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		--efi-boot-part --efi-boot-image --protective-msdos-label \
		build/iso/ -o $(ISO_FILE)

# Run in QEMU
run: all
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial stdio -enable-kvm

run-headless: all
	qemu-system-x86_64 -cdrom $(ISO_FILE) -nographic

run-debug: all
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial stdio -s -S

run-debug-headless: all
	qemu-system-x86_64 -cdrom $(ISO_FILE) -nographic -s -S

# Clean build artifacts but keep toolchain
clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C userspace clean
	rm -rf $(BUILD_DIR)

# Clean everything including toolchain
distclean: clean
	rm -rf $(TOOLCHAIN_BASE_DIR) .cache

# Force rebuild of the toolchain
rebuild-toolchain:
	@echo "Removing toolchain directory at $(TOOLCHAIN_DIR)..."
	rm -rf $(TOOLCHAIN_DIR)
	@echo "Building x86_64 toolchain in $(TOOLCHAIN_DIR)..."
	./scripts/build-toolchain.sh

# Debugging target
debug-toolchain:
	@echo "Architecture: x86_64"
	@echo "Toolchain target: $(TOOLCHAIN_TARGET)"
	@echo "Cross prefix: $(CROSS_PREFIX)"
	@echo "Shared include dir: $(SHARED_INCLUDE_DIR)"
	@echo "Expected compiler: $(CC)"
	@echo "Expected linker: $(LD)"
	@echo "Expected assembler: $(AS)"
	@echo "Toolchain directory contents:"
	@ls -la $(TOOLCHAIN_DIR)/bin/ || echo "Directory does not exist"
