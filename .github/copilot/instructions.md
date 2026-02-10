---
description: Project-specific instructions for arena-hnswlib C++ library
applyTo: "**"
---

# GitHub Copilot Instructions for arena-hnswlib

## Project Overview
This is a high-performance vector search library based on the HNSW algorithm, implemented in C++17.

## Key Development Guidelines

### Code Style
- Use 4 spaces for indentation
- camelCase for variables/functions, PascalCase for classes, UPPER_SNAKE_CASE for constants
- Header-only preferred for core algorithms
- Use `namespace hnswlib` for all library code

### Build & Test
```bash
# Build
sh scripts/build.sh

# Test
sh scripts/run_test.sh

# Benchmark
sh scripts/run_benchmarks.sh
```

### Memory Bank
Always read memory-bank/ directory at the start of every task to understand project context.

### Agent Usage
Use the Planner agent (.github/agents/Planner.agent.md) for strategic planning and architecture analysis before implementation.
