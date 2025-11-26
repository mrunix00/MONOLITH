# Directories
ROOT_DIR := $(CURDIR)/..
BUILD_DIR := $(CURDIR)/build
COVERAGE_DIR := $(BUILD_DIR)/coverage

# Find all test directories with Makefiles
TEST_DIRS := $(shell find $(CURDIR) -type f -name "Makefile" | grep -v "^$(CURDIR)/Makefile" | xargs dirname)

# Compiler and coverage tools
GCOV := gcov
LCOV := lcov
GENHTML := genhtml

.PHONY: all test coverage-report clean

all: test

# Build directory setup
$(BUILD_DIR) $(COVERAGE_DIR):
	mkdir -p $@

# Main test target - runs all tests in subdirectories
test: | $(BUILD_DIR)
	@echo "[*] Running all tests..."
	@for dir in $(TEST_DIRS); do \
		echo "[*] Running tests in $$dir"; \
		$(MAKE) -C $$dir; \
	done
	@echo "[+] All tests completed"

# Generate HTML coverage report
coverage-report: test | $(COVERAGE_DIR)
	@echo "[*] Generating combined coverage report..."
	../scripts/generate_coverage_report.sh $(BUILD_DIR) $(COVERAGE_DIR) $(ROOT_DIR)

# Clean build artifacts
clean:
	@echo "[*] Cleaning build artifacts..."
	@for dir in $(TEST_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -rf $(BUILD_DIR)
	rm -f $(CURDIR)/*.gcda $(CURDIR)/*.gcno
	rm -f $(CURDIR)/**/*.gcda $(CURDIR)/**/*.gcno
