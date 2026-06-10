# IA-32 architecture configuration.
# Included by the top-level Makefile and the arch/pc/ia32/Makefile.

TOOLCHAIN_TARGET := i386-monolith
NASM_FORMAT := elf32
QEMU_SYSTEM := qemu-system-i386
ARCH_CPU := ia32

ARCH_CFLAGS := -m32
LDFLAGS_ARCH := -T $(PROJECT_ROOT)/boot/pc/ia32/linker.ld -z max-page-size=0x1000
LIMINE_CONF := $(PROJECT_ROOT)/boot/pc/ia32/limine.conf

# ISO build configuration
ISO_BIOS_LIMINE_FILES := limine-bios.sys limine-bios-cd.bin
ISO_EFI_LIMINE_FILES :=
ISO_EFI_BOOT_IMAGE :=
ISO_XORRISO_EFI_FLAGS :=
