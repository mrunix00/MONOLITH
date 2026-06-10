# x86_64 architecture configuration.
# Included by the top-level Makefile and the arch/pc/x64/Makefile.

TOOLCHAIN_TARGET := x86_64-monolith
NASM_FORMAT := elf64
QEMU_SYSTEM := qemu-system-x86_64
ARCH_CPU := x64

ARCH_CFLAGS := -mno-red-zone -mcmodel=kernel
LDFLAGS_ARCH := -T $(PROJECT_ROOT)/boot/pc/x64/linker.ld -z max-page-size=0x1000
LIMINE_CONF := $(PROJECT_ROOT)/boot/pc/x64/limine.conf

# ISO build configuration
ISO_BIOS_LIMINE_FILES := limine-bios.sys limine-bios-cd.bin
ISO_EFI_LIMINE_FILES := limine-uefi-cd.bin
ISO_EFI_BOOT_IMAGE := BOOTX64.EFI
ISO_XORRISO_EFI_FLAGS := --efi-boot boot/limine/limine-uefi-cd.bin --efi-boot-part --efi-boot-image
