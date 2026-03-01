LIB_DIRS := $(shell find . -mindepth 1 -maxdepth 1 -type d -exec test -f {}/Makefile \; -print | sort)

LIBS_OUTPUT_DIR ?= $(BUILD_DIR)/libs

export BUILD_DIR TOOLCHAIN_BIN TOOLCHAIN_DIR CPU_ARCH CROSS_PREFIX INITRD_DIR LIBS_OUTPUT_DIR

.PHONY: all clean $(LIB_DIRS)

all: $(LIB_DIRS)

$(LIB_DIRS):
	@echo "Building shared library: $(@F)"
	@$(MAKE) -C $@ \
		BUILD_DIR=$(BUILD_DIR) \
		CPU_ARCH=$(CPU_ARCH) \
		CROSS_PREFIX=$(CROSS_PREFIX) \
		TOOLCHAIN_BIN=$(TOOLCHAIN_BIN) \
		TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
		INITRD_DIR=$(INITRD_DIR) \
		LIBS_OUTPUT_DIR=$(LIBS_OUTPUT_DIR)

clean:
	@echo "Cleaning shared libraries"
	@for dir in $(LIB_DIRS); do \
		echo "Cleaning $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
