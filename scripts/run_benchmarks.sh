#!/bin/bash

# Ensure the arena hnswlib is built
if [ ! -d "build" ]; then
  echo "Build directory not found. Please run build.sh first."
  exit 1
fi

# Run benchmarks
cd build

echo "========================================"
echo "Running math utils benchmarks..."
echo "========================================"
./benchmark_math_utils

echo ""
echo "========================================"
echo "Running index benchmarks..."
echo "========================================"
./benchmark_index