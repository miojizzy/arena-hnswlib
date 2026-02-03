#!/bin/bash

# Build the project
cmake -B build && \
    cmake --build build && \
    ctest --output-on-failure --test-dir build
