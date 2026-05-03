ROOT_DIR := $(abspath ..)
BUILD_DIR := $(CURDIR)/build
BIN_DIR := $(BUILD_DIR)/bin
COVERAGE_DIR := $(BUILD_DIR)/coverage

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -g
LDFLAGS ?=

ifeq ($(COVERAGE),1)
  CFLAGS += -O0 -fprofile-arcs -ftest-coverage
  LDFLAGS += -fprofile-arcs -ftest-coverage
endif

UNITY_SRC := $(ROOT_DIR)/libs/Unity/src/unity.c

KERNEL_TEST_BIN := $(BIN_DIR)/kernel_tests
USERSPACE_TEST_BIN := $(BIN_DIR)/userspace_tests

KERNEL_TEST_SOURCES := \
	$(CURDIR)/kernel/main.c \
	$(CURDIR)/kernel/mocks.c \
	$(CURDIR)/kernel/ipc/ipc_tests.c \
	$(CURDIR)/kernel/klibc/memory_tests.c \
	$(CURDIR)/kernel/klibc/string_tests.c \
	$(CURDIR)/kernel/vfs/vfs_tests.c \
	$(UNITY_SRC) \
	$(ROOT_DIR)/kernel/tasking/ipc.c \
	$(ROOT_DIR)/kernel/fs/vfs.c \
	$(ROOT_DIR)/kernel/klibc/string.c \
	$(ROOT_DIR)/kernel/klibc/memory.c

USERSPACE_TEST_SOURCES := \
	$(CURDIR)/userspace/main.c \
	$(CURDIR)/userspace/libc/stdio_tests.c \
	$(CURDIR)/userspace/libc/math_tests.c \
	$(CURDIR)/userspace/libc/stdlib_tests.c \
	$(CURDIR)/userspace/libc/string_tests.c \
	$(CURDIR)/userspace/libc/ipc_tests.c \
	$(UNITY_SRC) \
	$(ROOT_DIR)/userspace/libs/libc/src/stdio.c \
	$(ROOT_DIR)/userspace/libs/libc/src/math.c \
	$(ROOT_DIR)/userspace/libs/libc/src/stdlib.c \
	$(ROOT_DIR)/userspace/libs/libc/src/string.c \
	$(ROOT_DIR)/userspace/libs/libc/src/ipc.c

KERNEL_TEST_CFLAGS := $(CFLAGS) -I$(ROOT_DIR) -I$(CURDIR) -DUNITY_INCLUDE_CONFIG_H
USERSPACE_TEST_CFLAGS := $(CFLAGS) -I$(ROOT_DIR) -I$(ROOT_DIR)/userspace/libs/libc/include -I$(ROOT_DIR)/shared/include/monolith -I$(CURDIR) -DTEST_ENV -DUNITY_INCLUDE_CONFIG_H -DUNITY_EXCLUDE_MATH_H -fno-builtin

.PHONY: all kernel userspace run run-kernel run-userspace clean coverage-report

all: run

kernel: $(KERNEL_TEST_BIN)

userspace: $(USERSPACE_TEST_BIN)

run: run-kernel run-userspace

run-kernel: $(KERNEL_TEST_BIN)
	$(KERNEL_TEST_BIN)

run-userspace: $(USERSPACE_TEST_BIN)
	$(USERSPACE_TEST_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(KERNEL_TEST_BIN): $(KERNEL_TEST_SOURCES) | $(BIN_DIR)
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $(KERNEL_TEST_SOURCES) $(LDFLAGS)

$(USERSPACE_TEST_BIN): $(USERSPACE_TEST_SOURCES) | $(BIN_DIR)
	$(CC) $(USERSPACE_TEST_CFLAGS) -o $@ $(USERSPACE_TEST_SOURCES) $(LDFLAGS)

coverage-report:
	$(MAKE) clean
	$(MAKE) COVERAGE=1 all
	$(ROOT_DIR)/scripts/generate_coverage_report.sh "$(BUILD_DIR)" "$(COVERAGE_DIR)" "$(ROOT_DIR)"

clean:
	rm -rf $(BUILD_DIR)
