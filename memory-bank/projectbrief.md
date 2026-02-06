## Project Name
arena-hnswlib

## Project Goals
Develop a high-performance vector search library based on the HNSW (Hierarchical Navigable Small World) algorithm, supporting efficient Approximate Nearest Neighbor (ANN) search. The project uses C++17 and aims for ease of use, extensibility, and superior performance, suitable for both academic and industrial scenarios.

## Core Requirements
- Provide efficient interfaces for vector insertion, deletion, and search
- Support multiple distance metrics (e.g., Euclidean, Inner Product)
- Provide unit tests and benchmarks
- Compatible with mainstream C++ build systems (CMake)

## Scope
- Only implements core algorithms and data structures
- Does not include advanced features such as distributed or persistent storage
- Primarily header-only for easy integration

## Quick Start Commands
- build: `sh script/build.sh`
- test: `sh script/run_test.sh`
- benchmark: `sh script/run_benchmarks.sh`