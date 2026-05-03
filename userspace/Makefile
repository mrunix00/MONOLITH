# Find all userspace program directories (must contain a Makefile)
USERSPACE_DIRS := $(shell find . -mindepth 1 -maxdepth 1 -type d -exec test -f {}/Makefile \; -print | sort)
LIBS_DIR := ./libs
PROGRAM_DIRS := $(filter-out $(LIBS_DIR),$(USERSPACE_DIRS))

# Export variables for subdirectory makefiles
export BUILD_DIR TOOLCHAIN_BIN CROSS_PREFIX INITRD_DIR SHARED_INCLUDE_DIR

.PHONY: all clean $(USERSPACE_DIRS) $(PROGRAM_DIRS)

all: $(LIBS_DIR) $(PROGRAM_DIRS)

$(PROGRAM_DIRS): $(LIBS_DIR)

# Build each userspace program by calling its Makefile
$(USERSPACE_DIRS):
	@echo "Building userspace program: $(@F)"
	@$(MAKE) -C $@

# Clean all userspace program build files by calling their clean targets
clean:
	@echo "Cleaning userspace build files"
	@for dir in $(USERSPACE_DIRS); do \
		echo "Cleaning $$dir"; \
		$(MAKE) -C $$dir clean; \
	done
