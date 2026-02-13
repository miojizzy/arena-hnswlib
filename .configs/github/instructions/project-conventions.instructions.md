---
applyTo: '**/*.{cpp,h,hpp}'
---

# Project Coding Conventions

## C++ Development Guidelines

### Header Files (.h, .hpp)
- Use header guards or `#pragma once`
- Keep headers self-contained
- Include only what's necessary
- Document public APIs clearly

### Source Files (.cpp)
- Implement corresponding header declarations
- Keep functions focused and concise
- Follow CMake build system conventions
- Use GoogleTest for unit tests

### Code Style
- Use 4 spaces for indentation
- Use camelCase for variables and functions
- Use PascalCase for classes
- Use UPPER_SNAKE_CASE for constants
- Add meaningful comments for complex logic

### Project Specifics
- Target C++17 standard
- Header-only preferred for core algorithms
- Use `namespace hnswlib` for all library code
- Follow HNSW algorithm patterns
- Distance metrics support: Euclidean, Inner Product

### Testing
- Write unit tests in `tests/unit/` directory
- Use GoogleTest framework
- Tests should be fast and independent
- Benchmark critical components in `benchmarks/`

### Documentation
- Update `memory-bank/` when making significant changes
- Document new patterns in instruction files
- Keep README.md updated with quick start commands

## Build Commands
```bash
# Build project
sh scripts/build.sh

# Run tests
sh scripts/run_test.sh

# Run benchmarks
sh scripts/run_benchmarks.sh
```

## Dependencies
- GoogleTest (for unit testing)
- Google Benchmark (for performance testing)
- CMake (build system)
