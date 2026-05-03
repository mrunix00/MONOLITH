LIB_DIRS := $(shell find . -mindepth 1 -maxdepth 1 -type d -exec test -f {}/Makefile \; -print | sort)

LIBS_OUTPUT_DIR ?= $(BUILD_DIR)/libs

export BUILD_DIR TOOLCHAIN_BIN CROSS_PREFIX INITRD_DIR SHARED_INCLUDE_DIR LIBS_OUTPUT_DIR

.PHONY: all clean $(LIB_DIRS)

all: $(LIB_DIRS)

$(LIB_DIRS):
	@echo "Building shared library: $(@F)"
	@$(MAKE) -C $@

clean:
	@echo "Cleaning shared libraries"
	@for dir in $(LIB_DIRS); do \
		echo "Cleaning $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
