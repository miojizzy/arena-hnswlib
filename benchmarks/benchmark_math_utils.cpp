#include <benchmark/benchmark.h>
#include "arena-hnswlib/math_utils.h"

static void BM_Add(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(arena_hnswlib::add(123, 456));
    }
}
BENCHMARK(BM_Add);

static void BM_Multiply(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(arena_hnswlib::multiply(123, 456));
    }
}
BENCHMARK(BM_Multiply);

BENCHMARK_MAIN();