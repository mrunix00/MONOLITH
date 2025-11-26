#!/bin/bash

set -e

if [ $# -lt 3 ]; then
    echo "Usage: $0 BUILD_DIR COVERAGE_DIR ROOT_DIR"
    exit 1
fi

BUILD_DIR="$1"
COVERAGE_DIR="$2"
ROOT_DIR="$3"

# Make sure coverage directories exist
rm -rf "$COVERAGE_DIR"
mkdir -p "$COVERAGE_DIR"

echo "Generating coverage report using lcov..."

# Do NOT reset counters as the tests have already generated the coverage data
# Simply capture what we already have

# Create initial coverage data file with error handling
lcov --capture --directory "$BUILD_DIR" \
     --output-file "$COVERAGE_DIR/coverage_all.info" \
     --ignore-errors source \
     --ignore-errors graph \
     --ignore-errors empty

# Now properly filter to ONLY include kernel files
lcov --extract "$COVERAGE_DIR/coverage_all.info" "*kernel/*" "*boot/*" \
     --output-file "$COVERAGE_DIR/coverage_kernel.info" \
     --ignore-errors source \
     --ignore-errors empty \
     --ignore-errors unused

# Explicitly remove test files and Unity from the report
lcov --remove "$COVERAGE_DIR/coverage_kernel.info" \
     --output-file "$COVERAGE_DIR/coverage_final.info" \
     --ignore-errors source \
     --ignore-errors empty \
     --ignore-errors unused

# Generate HTML report with filtering for kernel files only
genhtml --output-directory="$COVERAGE_DIR/html" \
        --title="MONOLITH Kernel Coverage" \
        --legend \
        --show-details \
        --prefix="$ROOT_DIR" \
        --ignore-errors source \
        --ignore-errors empty \
        --demangle-cpp \
        --function-coverage \
        --rc genhtml_med_limit=50 \
        --rc genhtml_hi_limit=75 \
        "$COVERAGE_DIR/coverage_final.info"

echo "HTML coverage report generated in $COVERAGE_DIR/html"
echo "You can open it with: xdg-open $COVERAGE_DIR/html/index.html"
