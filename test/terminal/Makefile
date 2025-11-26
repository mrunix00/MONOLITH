# Makefile for terminal tests
ROOT_DIR := $(CURDIR)/../..
TEST_DIR := $(CURDIR)/..
BUILD_DIR := $(TEST_DIR)/build/terminal
COVERAGE_DIR := $(TEST_DIR)/build/coverage

# Unity test framework
UNITY_DIR := $(ROOT_DIR)/libs/Unity
UNITY_SRC := $(UNITY_DIR)/src/unity.c
UNITY_OBJ := $(BUILD_DIR)/unity.o

# Compiler flags
CC := gcc
CFLAGS := -g -Wall -Wextra -I$(ROOT_DIR) -I$(UNITY_DIR)/src -DTEST_BUILD
LDFLAGS :=
COVERAGE_FLAGS := -fprofile-arcs -ftest-coverage

# Find test sources
TEST_SRCS := $(wildcard $(CURDIR)/*_test.c)
TEST_OBJS := $(patsubst $(CURDIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst $(CURDIR)/%.c,$(BUILD_DIR)/%.bin,$(TEST_SRCS))

# Auto-find all terminal source files
TERM_SRCS := $(wildcard $(ROOT_DIR)/kernel/terminal/*.c)
TERM_OBJS := $(patsubst $(ROOT_DIR)/kernel/terminal/%.c,$(BUILD_DIR)/%.o,$(TERM_SRCS))

.PHONY: all test clean

all: test

test: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "[*] Running $$test..."; \
		$$test; \
	done
	@echo "[+] terminal tests completed"

# Create build directory
$(BUILD_DIR):
	mkdir -p $@

# Ensure directory exists
define ensure_dir
	@mkdir -p $(dir $@)
endef

# Compile Unity
$(UNITY_OBJ): $(UNITY_SRC) | $(BUILD_DIR)
	$(ensure_dir)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

# Compile kernel files (generic rule for all terminal files)
$(BUILD_DIR)/%.o: $(ROOT_DIR)/kernel/terminal/%.c | $(BUILD_DIR)
	$(ensure_dir)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -DTEST_ENV -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(CURDIR)/%.c | $(BUILD_DIR)
	$(ensure_dir)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

# Link test executables
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.o $(UNITY_OBJ) $(TERM_OBJS) | $(BUILD_DIR)
	$(ensure_dir)
	$(CC) $(CFLAGS) $(LDFLAGS) $(COVERAGE_FLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)
