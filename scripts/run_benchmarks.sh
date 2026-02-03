#!/bin/bash

# Ensure the arena hnswlib is built
if [ ! -d "build" ]; then
  echo "Build directory not found. Please run build_and_test.sh first."
  exit 1
fi

# Run benchmarks
cd build
./benchmark_math_utils