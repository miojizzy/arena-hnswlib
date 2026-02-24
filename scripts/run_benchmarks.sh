#!/bin/bash
#
# Usage:
#   sh scripts/run_benchmarks.sh              # run & save to logs/benchmark_tmp.txt
#   sh scripts/run_benchmarks.sh --save       # run & overwrite canonical file
#                                             #   (logs/benchmark_debug.txt or benchmark_release.txt
#                                             #    detected automatically from CMakeCache.txt)
#
# Convention:
#   - During development, compare against logs/benchmark_tmp.txt (not committed).
#   - Before committing, run with --save to refresh the canonical file for the
#     current build type.
#   - benchmark_debug.txt and benchmark_release.txt are the only committed files.

SAVE_CANONICAL=0
if [ "$1" = "--save" ]; then
    SAVE_CANONICAL=1
fi

# Ensure the arena hnswlib is built
if [ ! -d "build" ]; then
  echo "Build directory not found. Please run build.sh first."
  exit 1
fi

# Detect build type from CMakeCache
BUILD_TYPE=$(grep -m1 "^CMAKE_BUILD_TYPE:STRING=" build/CMakeCache.txt 2>/dev/null | cut -d= -f2)
BUILD_TYPE=${BUILD_TYPE:-Debug}

if [ "$SAVE_CANONICAL" -eq 1 ]; then
    if [ "$BUILD_TYPE" = "Release" ]; then
        OUTPUT_FILE="logs/benchmark_release.txt"
    else
        OUTPUT_FILE="logs/benchmark_debug.txt"
    fi
    echo "Saving results to canonical file: $OUTPUT_FILE  (build type: $BUILD_TYPE)"
else
    OUTPUT_FILE="logs/benchmark_tmp.txt"
    echo "Saving results to temp file: $OUTPUT_FILE  (build type: $BUILD_TYPE)"
    echo "  (use --save to overwrite the canonical benchmark file)"
fi

# Run benchmarks
cd build

{
echo "========================================"
echo "Running math utils benchmarks..."
echo "========================================"
./benchmark_math_utils

echo ""
echo "========================================"
echo "Running index benchmarks..."
echo "========================================"
./benchmark_index
} 2>&1 | tee "../$OUTPUT_FILE"
