# Arena-HNSWLIB Benchmarks

This directory contains benchmark suites for evaluating the performance of arena-hnswlib algorithms.

## Available Benchmarks

### benchmark_math_utils
Basic mathematical utility function benchmarks.

### benchmark_index
Comprehensive comparison between BruteForce and HNSW index implementations.

## Benchmark Index Features

The `benchmark_index` suite provides:

1. **Random Vector Generation**
   - Seedable random number generator for reproducibility
   - Configurable L2 normalization
   - Support for generating single or batch vectors

2. **Index Comparison**
   - BruteForce index build/query performance
   - HNSW index build/query performance
   - Single and batch query benchmarks

3. **Parameterized Tests**
   - Varying K values (nearest neighbors)
   - Varying dataset sizes
   - Normalized vs raw vector comparison

## Running Benchmarks

### Run All Benchmarks
```bash
sh scripts/run_benchmarks.sh
```

### Run Specific Benchmark
```bash
cd build
./benchmark_index
```

### Run with Custom Options
```bash
# Run with shorter minimum time (faster, less accurate)
./benchmark_index --benchmark_min_time=0.1s

# Run specific benchmarks matching a regex
./benchmark_index --benchmark_filter="BruteForce.*"

# Output results in JSON format
./benchmark_index --benchmark_format=json --benchmark_out=results.json

# Output results in CSV format
./benchmark_index --benchmark_format=csv --benchmark_out=results.csv
```

## Benchmark Configuration

The default configuration in `benchmark_index.cpp`:

```cpp
// BruteForceFixture & HNSWFixture
DIM = 128              // Vector dimension
DATA_SIZE = 1000       // Number of vectors in index (已下调，适合开发机)
QUERY_SIZE = 10        // Number of query vectors (已下调)
K = 10                 // Number of nearest neighbors to find
SEED = 42              // Random seed for reproducibility
NORMALIZE = false      // L2 normalization flag

// HNSW specific
M = 16                 // Maximum number of connections per layer
EF_CONSTRUCTION = 100  // Size of dynamic candidate list during construction (已下调)
```

## Adjusting for Faster Benchmarks

For quicker development testing, the default parameters are already reduced:

```cpp
static constexpr size_t DATA_SIZE = 1000;   // Default (was 10000)
static constexpr size_t QUERY_SIZE = 10;    // Default (was 100)
static constexpr size_t EF_CONSTRUCTION = 100; // Default (was 200)
```
If you need even faster runs, you can further reduce DATA_SIZE/QUERY_SIZE in benchmark_index.cpp.

## Understanding Results

### Metrics
- **Time**: Wall clock time per iteration
- **CPU**: CPU time per iteration
- **Iterations**: Number of times the benchmark was run
- **items_per_second**: Throughput metric

### Key Comparisons
1. **Build Time**: HNSW takes longer to build but provides faster queries
2. **Query Time**: HNSW queries are typically much faster than BruteForce for large datasets
3. **Scalability**: HNSW performance degrades logarithmically with data size, while BruteForce degrades linearly

### Benchmark Size Recommendations
- For most development/CI: DATA_SIZE ≤ 1000, QUERY_SIZE ≤ 10, EF_CONSTRUCTION ≤ 100
- For quick smoke tests: DATA_SIZE 100~500
- For full performance: increase参数量，但注意单项时间

## Distance Metrics

Currently, all benchmarks use **L2 (Euclidean) distance** only. Inner product support is available in the library but not benchmarked (as per TASK003 requirements).

## Reproducibility

All benchmarks use fixed random seeds (default: 42) to ensure reproducible results across runs. This is critical for:
- Performance regression testing
- Comparing optimization attempts
- Validating algorithmic improvements

## Notes

- Benchmarks are built in DEBUG mode by default, which may affect timings
- For production-quality benchmarks, build in RELEASE mode with optimizations
- The library warns if built in DEBUG mode during benchmark execution
