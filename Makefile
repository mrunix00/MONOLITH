PROJECT_ROOT := $(abspath $(CURDIR))
ARCH ?= pc/x64
SUPPORTED_ARCHES := pc/x64 pc/ia32
ifneq ($(filter $(ARCH),$(SUPPORTED_ARCHES)),$(ARCH))
$(error Unsupported ARCH '$(ARCH)'. Supported: $(SUPPORTED_ARCHES))
endif

include $(PROJECT_ROOT)/kernel/arch/$(ARCH)/config.mk

TOOLCHAIN_BASE_DIR := $(PROJECT_ROOT)/toolchain
TOOLCHAIN_DIR := $(TOOLCHAIN_BASE_DIR)/$(TOOLCHAIN_TARGET)
TOOLCHAIN_BIN := $(TOOLCHAIN_DIR)/bin
CROSS_PREFIX := $(TOOLCHAIN_TARGET)-
SHARED_INCLUDE_DIR := $(PROJECT_ROOT)/shared/include/monolith
CC := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)gcc
LD := $(TOOLCHAIN_BIN)/$(CROSS_PREFIX)ld
KERNEL_LIBGCC = $$($(CC) $(ARCH_CFLAGS) -print-libgcc-file-name)

BUILD_ROOT := $(abspath build)
BUILD_DIR := $(BUILD_ROOT)/$(subst /,_,$(ARCH))
ISO_DIR := $(BUILD_DIR)/isodir
OBJ_DIR := $(BUILD_DIR)/obj
INITRD_DIR := $(BUILD_DIR)/initrd
CACHE_DIR := $(PROJECT_ROOT)/.cache

# Limine bootloader release
LIMINE_VERSION := v12.3.3
LIMINE_URL := https://github.com/Limine-Bootloader/Limine/releases/download/$(LIMINE_VERSION)/limine-binary.tar.gz
LIMINE_CACHE_DIR := $(CACHE_DIR)/limine-$(LIMINE_VERSION)
LIMINE_TARBALL := $(CACHE_DIR)/limine-binary-$(LIMINE_VERSION).tar.gz
LIMINE_READY := $(LIMINE_CACHE_DIR)/.ready

KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_BIN_GZ := $(KERNEL_BIN).gz
INITRD_TAR := $(BUILD_DIR)/initrd.tar
INITRD_TAR_GZ := $(INITRD_TAR).gz
ISO_FILE := $(BUILD_DIR)/monolith-$(ARCH_CPU).iso

CFLAGS := -ffreestanding -Wall -Wextra -I$(PROJECT_ROOT) -std=c99 -fno-stack-protector -fno-stack-check -fno-PIC $(ARCH_CFLAGS)
LDFLAGS := $(LDFLAGS_ARCH) -nostdlib -static --build-id=none

FLANTERM_SOURCES := libs/flanterm/src/flanterm.c libs/flanterm/src/flanterm_backends/fb.c
FLANTERM_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(FLANTERM_SOURCES))
FLANTERM_DEPS := $(FLANTERM_OBJECTS:.o=.d)

# Variables exported to the per-arch kernel Makefile
KERN_MAKEFLAGS := \
	PROJECT_ROOT='$(PROJECT_ROOT)' \
	BUILD_DIR='$(BUILD_DIR)' \
	OBJ_DIR='$(OBJ_DIR)' \
	TOOLCHAIN_BIN='$(TOOLCHAIN_BIN)' \
	TOOLCHAIN_DIR='$(TOOLCHAIN_DIR)' \
	TOOLCHAIN_BASE_DIR='$(TOOLCHAIN_BASE_DIR)' \
	TOOLCHAIN_TARGET='$(TOOLCHAIN_TARGET)' \
	CROSS_PREFIX='$(CROSS_PREFIX)' \
	ARCH_CFLAGS='$(ARCH_CFLAGS)' \
	NASM_FORMAT='$(NASM_FORMAT)' \
	LDFLAGS_ARCH='$(LDFLAGS_ARCH)'

# Variables passed to the userspace build
USERSPACE_MAKEFLAGS := \
	BUILD_DIR='$(BUILD_DIR)' \
	TOOLCHAIN_BIN='$(TOOLCHAIN_BIN)' \
	TOOLCHAIN_TARGET='$(TOOLCHAIN_TARGET)' \
	CROSS_PREFIX='$(CROSS_PREFIX)' \
	NASM_FORMAT='$(NASM_FORMAT)' \
	ARCH_CFLAGS='$(ARCH_CFLAGS)' \
	SHARED_INCLUDE_DIR='$(SHARED_INCLUDE_DIR)' \
	INITRD_DIR='$(INITRD_DIR)'

export PROJECT_ROOT ARCH ARCH_CPU TOOLCHAIN_BIN TOOLCHAIN_DIR TOOLCHAIN_BASE_DIR TOOLCHAIN_TARGET CROSS_PREFIX SHARED_INCLUDE_DIR NASM_FORMAT ARCH_CFLAGS CFLAGS

-include $(FLANTERM_DEPS)

.PHONY: all clean toolchain limine iso run run-headless run-debug run-debug-headless kernel flanterm initrd userspace FORCE rebuild-toolchain debug-toolchain

all: | $(BUILD_DIR)
	$(MAKE) toolchain
	$(MAKE) kernel
	$(MAKE) flanterm
	$(MAKE) userspace
	$(MAKE) $(KERNEL_BIN)
	$(MAKE) $(KERNEL_BIN_GZ)
	$(MAKE) $(INITRD_TAR_GZ)
	$(MAKE) $(ISO_FILE)

toolchain:
	@if [ ! -f "$(CC)" ] || [ ! -f "$(LD)" ]; then \
		echo "Building $(TOOLCHAIN_TARGET) toolchain in $(TOOLCHAIN_DIR)..."; \
		TOOLCHAIN_TARGET=$(TOOLCHAIN_TARGET) PROJECT_ROOT=$(PROJECT_ROOT) bash ./scripts/build-toolchain.sh; \
	else \
		echo "Toolchain already exists at $(TOOLCHAIN_DIR)"; \
	fi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(OBJ_DIR)

$(CACHE_DIR):
	mkdir -p $(CACHE_DIR)

kernel: toolchain | $(BUILD_DIR)
	$(MAKE) -C kernel/arch/$(ARCH) $(KERN_MAKEFLAGS)

flanterm: $(FLANTERM_OBJECTS)

$(OBJ_DIR)/libs/flanterm/src/%.o: libs/flanterm/src/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(KERNEL_BIN): kernel flanterm | toolchain
	$(LD) $(LDFLAGS) -o $@ $(shell find $(OBJ_DIR)/arch $(OBJ_DIR)/kernel $(OBJ_DIR)/libs -type f -name "*.o" 2>/dev/null) $(KERNEL_LIBGCC)

$(KERNEL_BIN_GZ): $(KERNEL_BIN)
	gzip -n -c $< > $@

userspace: | $(BUILD_DIR) $(INITRD_DIR)
	@if [ ! -f "$(CC)" ] || [ ! -f "$(LD)" ]; then \
		echo "Toolchain not found. Building it first..."; \
		$(MAKE) toolchain; \
	fi
	$(MAKE) -C userspace $(USERSPACE_MAKEFLAGS)

$(INITRD_DIR): | $(BUILD_DIR)
	@mkdir -p $(INITRD_DIR)

initrd: $(INITRD_TAR)

$(INITRD_TAR): FORCE userspace $(INITRD_DIR) | $(BUILD_DIR)
	@echo "Creating initrd archive in ustar format..."
	@mkdir -p $(INITRD_DIR)/assets
	@cp -f assets/wallpaper.jpg $(INITRD_DIR)/assets/wallpaper.jpg
	@cp -f assets/IBMPlexSans_Condensed-Medium.ttf $(INITRD_DIR)/assets/IBMPlexSans_Condensed-Medium.ttf
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

limine: $(LIMINE_READY)

$(ISO_FILE): $(KERNEL_BIN_GZ) $(INITRD_TAR_GZ) $(LIMINE_CONF) $(LIMINE_READY)
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/boot $(ISO_DIR)/EFI/BOOT
	cp -v $(KERNEL_BIN_GZ) $(INITRD_TAR_GZ) $(ISO_DIR)/boot
	cp -v $(LIMINE_CONF) \
	    $(addprefix $(LIMINE_CACHE_DIR)/,$(ISO_BIOS_LIMINE_FILES)) \
	    $(ISO_DIR)/boot/limine
	@if [ -n "$(ISO_EFI_LIMINE_FILES)" ]; then \
		cp -v $(addprefix $(LIMINE_CACHE_DIR)/,$(ISO_EFI_LIMINE_FILES)) $(ISO_DIR)/boot/limine; \
	fi
	@if [ -n "$(ISO_EFI_BOOT_IMAGE)" ]; then \
		cp -v $(LIMINE_CACHE_DIR)/$(ISO_EFI_BOOT_IMAGE) $(ISO_DIR)/EFI/BOOT; \
	fi
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    $(ISO_XORRISO_EFI_FLAGS) --protective-msdos-label \
	    $(ISO_DIR) -o $(ISO_FILE)

iso: $(ISO_FILE)

run: all
	$(QEMU_SYSTEM) -cdrom $(ISO_FILE) -serial stdio -enable-kvm

run-headless: all
	$(QEMU_SYSTEM) -cdrom $(ISO_FILE) -nographic

run-debug: all
	$(QEMU_SYSTEM) -cdrom $(ISO_FILE) -serial stdio -s -S

run-debug-headless: all
	$(QEMU_SYSTEM) -cdrom $(ISO_FILE) -nographic -s -S

clean:
	rm -rf $(BUILD_ROOT)

distclean: clean
	rm -rf $(TOOLCHAIN_BASE_DIR) $(BUILD_ROOT)

rebuild-toolchain:
	@echo "Removing toolchain directory at $(TOOLCHAIN_DIR)..."
	rm -rf $(TOOLCHAIN_DIR)
	@echo "Building $(TOOLCHAIN_TARGET) toolchain in $(TOOLCHAIN_DIR)..."
	TOOLCHAIN_TARGET=$(TOOLCHAIN_TARGET) PROJECT_ROOT=$(PROJECT_ROOT) ./scripts/build-toolchain.sh

debug-toolchain:
	@echo "ARCH: $(ARCH)"
	@echo "Architecture CPU: $(ARCH_CPU)"
	@echo "Toolchain target: $(TOOLCHAIN_TARGET)"
	@echo "Cross prefix: $(CROSS_PREFIX)"
	@echo "Shared include dir: $(SHARED_INCLUDE_DIR)"
	@echo "Expected compiler: $(CC)"
	@echo "Expected linker: $(LD)"
	@echo "NASM format: $(NASM_FORMAT)"
	@echo "Build dir: $(BUILD_DIR)"
	@ls -la $(TOOLCHAIN_DIR)/bin/ || echo "Directory does not exist"
