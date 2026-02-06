# TASK003 Benchmark Example Usage

## Quick Test with Smaller Dataset

To quickly test the benchmarks with a smaller dataset for faster results:

```bash
cd build
# Run only BruteForce benchmarks
./benchmark_index --benchmark_filter="BruteForce.*" --benchmark_min_time=0.1s

# Run only parameterized K-value benchmarks
./benchmark_index --benchmark_filter=".*VaryingK" --benchmark_min_time=0.1s
```

## Example Output Interpretation

```
BruteForceFixture/BuildIndex         618815 ns   items_per_second=16.1546M/s
BruteForceFixture/QuerySingle       5348989 ns   items_per_second=186.951/s
BruteForceFixture/QueryBatch      541077391 ns   items_per_second=184.822/s
```

This shows:
- **BuildIndex**: Building the index takes ~619μs for 10,000 vectors (very fast)
- **QuerySingle**: Each single query takes ~5.3ms (scans all 10,000 vectors)
- **QueryBatch**: 100 queries take ~541ms (~5.4ms per query, consistent with single)

## Key Features Implemented

1. ✅ **Seedable Random Vector Generation**
   ```cpp
   VectorGenerator gen(dim, seed, normalize);
   auto vectors = gen.generateBatch(count);
   ```

2. ✅ **L2 Normalization Toggle**
   ```cpp
   VectorGenerator gen(128, 42, true);  // normalized=true
   ```

3. ✅ **Comprehensive Benchmarks**
   - Build time measurement
   - Single query performance
   - Batch query performance
   - Varying K (1, 5, 10, 50, 100)
   - Varying dataset size (100, 500, 1000, 5000, 10000)
   - Normalized vs raw vectors

4. ✅ **L2 Distance Only**
   - All benchmarks use L2Space
   - Inner product available but not benchmarked

5. ✅ **Google Benchmark Integration**
   - JSON output: `--benchmark_format=json`
   - CSV output: `--benchmark_format=csv`
   - Custom filters: `--benchmark_filter="pattern"`

## Configuration Tips

For development/testing, edit `benchmark_index.cpp`:
```cpp
// Reduce these for faster iteration
static constexpr size_t DATA_SIZE = 1000;   // down from 10000
static constexpr size_t QUERY_SIZE = 10;    // down from 100
```

For production benchmarks, build in Release mode:
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
