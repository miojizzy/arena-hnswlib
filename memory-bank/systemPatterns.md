## System Architecture
- Header-only design: all core algorithms are implemented under include/arena-hnswlib/
- Main components: Data storage (data_store.h), Brute-force search (bruteforce.h), HNSW algorithm core (arena_hnswlib.h), Distance metrics (math_utils.h, space_ip.h, space_l2.h)

## Key Technical Decisions
- C++17 standard, compatible with mainstream compilers
- CMake build system, supports GoogleTest/Google Benchmark
- Unit tests and benchmarks are separated

## Design Patterns
- Interface/implementation separation
- Favor composition over inheritance
- Use of templates for generic data type support

## Component Relationships
- arena_hnswlib.h depends on data_store.h and math_utils.h
- Tests and benchmarks link to the header-only library

## Directory Structure
- include: library header files
    - arena-hnswlib: optimized version of hnswlib
- src: library source code (currently empty, header-only)
- test: test cases
    - utest: unit tests
- script: scripts for build, test, and benchmarking, to be run from project root