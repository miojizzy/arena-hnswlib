#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <cmath>
#include "arena-hnswlib/bruteforce.h"
#include "arena-hnswlib/hnswalg.h"
#include "arena-hnswlib/space_l2.h"
#include "arena-hnswlib/space_ip.h"

namespace arena_hnswlib {

// ========== Random Vector Generator ==========

class VectorGenerator {
private:
    size_t dim_;
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;
    bool normalize_;

public:
    VectorGenerator(size_t dim, uint32_t seed = 42, bool normalize = false)
        : dim_(dim), rng_(seed), dist_(0.0f, 1.0f), normalize_(normalize) {}

    // Generate a single random vector
    std::vector<float> generate() {
        std::vector<float> vec(dim_);
        for (size_t i = 0; i < dim_; ++i) {
            vec[i] = dist_(rng_);
        }
        if (normalize_) {
            normalizeL2(vec);
        }
        return vec;
    }

    // Generate multiple random vectors
    std::vector<std::vector<float>> generateBatch(size_t count) {
        std::vector<std::vector<float>> batch;
        batch.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            batch.push_back(generate());
        }
        return batch;
    }

private:
    // Normalize vector to unit L2 norm
    void normalizeL2(std::vector<float>& vec) {
        float sum_sq = 0.0f;
        for (float v : vec) {
            sum_sq += v * v;
        }
        float norm = std::sqrt(sum_sq);
        if (norm > 1e-10f) {  // Avoid division by zero
            for (float& v : vec) {
                v /= norm;
            }
        }
    }
};

// ========== Benchmark Fixtures ==========

class BruteForceFixture : public benchmark::Fixture {
public:
    static constexpr size_t DIM = 128;
    static constexpr size_t DATA_SIZE = 1000;   // 降低数据量
    static constexpr size_t QUERY_SIZE = 10;    // 降低查询量
    static constexpr size_t K = 10;
    static constexpr uint32_t SEED = 42;
    static constexpr bool NORMALIZE = false;

    std::unique_ptr<BruteForceSearch<float>> index;
    std::vector<std::vector<float>> data_points;
    std::vector<std::vector<float>> query_points;

    void SetUp(const ::benchmark::State& state) override {
        // Generate data
        VectorGenerator gen(DIM, SEED, NORMALIZE);
        data_points = gen.generateBatch(DATA_SIZE);
        query_points = gen.generateBatch(QUERY_SIZE);

        // Create index
        auto space = std::make_unique<L2Space<float>>(DIM);
        index = std::make_unique<BruteForceSearch<float>>(std::move(space), DATA_SIZE);
    }

    void TearDown(const ::benchmark::State& state) override {
        index.reset();
        data_points.clear();
        query_points.clear();
    }
};

class HNSWFixture : public benchmark::Fixture {
public:
    static constexpr size_t DIM = 128;
    static constexpr size_t DATA_SIZE = 1000;   // 降低数据量
    static constexpr size_t QUERY_SIZE = 10;    // 降低查询量
    static constexpr size_t K = 10;
    static constexpr size_t M = 16;
    static constexpr size_t EF_CONSTRUCTION = 100; // 降低构建参数
    static constexpr uint32_t SEED = 42;
    static constexpr bool NORMALIZE = false;

    std::unique_ptr<HierarchicalNSW<float, L2Space<float>>> index;
    std::vector<std::vector<float>> data_points;
    std::vector<std::vector<float>> query_points;

    void SetUp(const ::benchmark::State& state) override {
        // Generate data
        VectorGenerator gen(DIM, SEED, NORMALIZE);
        data_points = gen.generateBatch(DATA_SIZE);
        query_points = gen.generateBatch(QUERY_SIZE);

        // Create index
        index = std::make_unique<HierarchicalNSW<float, L2Space<float>>>(L2Space<float>(DIM), DATA_SIZE, M, EF_CONSTRUCTION, SEED);
    }

    void TearDown(const ::benchmark::State& state) override {
        index.reset();
        data_points.clear();
        query_points.clear();
    }
};

// ========== BruteForce Benchmarks ==========

BENCHMARK_F(BruteForceFixture, BuildIndex)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto space = std::make_unique<L2Space<float>>(DIM);
        auto local_index = std::make_unique<BruteForceSearch<float>>(std::move(space), DATA_SIZE);
        state.ResumeTiming();

        for (size_t i = 0; i < DATA_SIZE; ++i) {
            local_index->addPoint(data_points[i].data(), i);
        }
        
        benchmark::DoNotOptimize(local_index);
    }
    state.SetItemsProcessed(state.iterations() * DATA_SIZE);
}

BENCHMARK_F(BruteForceFixture, QuerySingle)(benchmark::State& state) {
    // Build index once
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    size_t query_idx = 0;
    for (auto _ : state) {
        auto result = index->searchKnn(query_points[query_idx].data(), K);
        benchmark::DoNotOptimize(result);
        query_idx = (query_idx + 1) % QUERY_SIZE;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(BruteForceFixture, QueryBatch)(benchmark::State& state) {
    // Build index once
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    for (auto _ : state) {
        for (size_t i = 0; i < QUERY_SIZE; ++i) {
            auto result = index->searchKnn(query_points[i].data(), K);
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * QUERY_SIZE);
}

// ========== HNSW Benchmarks ==========

BENCHMARK_F(HNSWFixture, BuildIndex)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto local_index = std::make_unique<HierarchicalNSW<float, L2Space<float>>>(L2Space<float>(DIM), DATA_SIZE, M, EF_CONSTRUCTION, SEED);
        state.ResumeTiming();

        for (size_t i = 0; i < DATA_SIZE; ++i) {
            local_index->addPoint(data_points[i].data(), i);
        }
        
        benchmark::DoNotOptimize(local_index);
    }
    state.SetItemsProcessed(state.iterations() * DATA_SIZE);
}

BENCHMARK_F(HNSWFixture, QuerySingle)(benchmark::State& state) {
    // Build index once
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    size_t query_idx = 0;
    for (auto _ : state) {
        auto result = index->searchKnn(query_points[query_idx].data(), K);
        benchmark::DoNotOptimize(result);
        query_idx = (query_idx + 1) % QUERY_SIZE;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_F(HNSWFixture, QueryBatch)(benchmark::State& state) {
    // Build index once
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    for (auto _ : state) {
        for (size_t i = 0; i < QUERY_SIZE; ++i) {
            auto result = index->searchKnn(query_points[i].data(), K);
            benchmark::DoNotOptimize(result);
        }
    }
    state.SetItemsProcessed(state.iterations() * QUERY_SIZE);
}

// ========== Parameterized Benchmarks ==========

static void BM_BruteForce_QueryVaryingK(benchmark::State& state) {
    const size_t dim = 128;
    const size_t data_size = 500;
    const size_t k = state.range(0);
    const uint32_t seed = 42;

    // Generate data
    VectorGenerator gen(dim, seed, false);
    auto data_points = gen.generateBatch(data_size);
    auto query = gen.generate();

    // Build index
    auto space = std::make_unique<L2Space<float>>(dim);
    auto index = std::make_unique<BruteForceSearch<float>>(std::move(space), data_size);
    for (size_t i = 0; i < data_size; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    // Benchmark query
    for (auto _ : state) {
        auto result = index->searchKnn(query.data(), k);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_BruteForce_QueryVaryingK)
    ->Arg(1)->Arg(5)->Arg(10)->Arg(50)->Arg(100)
    ->Unit(benchmark::kMicrosecond);

static void BM_HNSW_QueryVaryingK(benchmark::State& state) {
    const size_t dim = 128;
    const size_t data_size = 500;
    const size_t k = state.range(0);
    const size_t M = 16;
    const size_t ef_construction = 100;
    const uint32_t seed = 42;

    // Generate data
    VectorGenerator gen(dim, seed, false);
    auto data_points = gen.generateBatch(data_size);
    auto query = gen.generate();

    // Build index
    auto index = std::make_unique<HierarchicalNSW<float, L2Space<float>>>(L2Space<float>(dim), data_size, M, ef_construction, seed);
    for (size_t i = 0; i < data_size; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    // Benchmark query
    for (auto _ : state) {
        auto result = index->searchKnn(query.data(), k);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_HNSW_QueryVaryingK)
    ->Arg(1)->Arg(5)->Arg(10)->Arg(50)->Arg(100)
    ->Unit(benchmark::kMicrosecond);

static void BM_BruteForce_VaryingDataSize(benchmark::State& state) {
    const size_t dim = 128;
    const size_t data_size = state.range(0);
    const size_t k = 10;
    const uint32_t seed = 42;

    // Generate data
    VectorGenerator gen(dim, seed, false);
    auto data_points = gen.generateBatch(data_size);
    auto query = gen.generate();

    // Build index
    auto space = std::make_unique<L2Space<float>>(dim);
    auto index = std::make_unique<BruteForceSearch<float>>(std::move(space), data_size);
    for (size_t i = 0; i < data_size; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    // Benchmark query
    for (auto _ : state) {
        auto result = index->searchKnn(query.data(), k);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_BruteForce_VaryingDataSize)
    ->Arg(100)->Arg(250)->Arg(500)->Arg(1000)
    ->Unit(benchmark::kMicrosecond);

static void BM_HNSW_VaryingDataSize(benchmark::State& state) {
    const size_t dim = 128;
    const size_t data_size = state.range(0);
    const size_t k = 10;
    const size_t M = 16;
    const size_t ef_construction = 100;
    const uint32_t seed = 42;

    // Generate data
    VectorGenerator gen(dim, seed, false);
    auto data_points = gen.generateBatch(data_size);
    auto query = gen.generate();

    // Build index
    auto index = std::make_unique<HierarchicalNSW<float, L2Space<float>>>(L2Space<float>(dim), data_size, M, ef_construction, seed);
    for (size_t i = 0; i < data_size; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    // Benchmark query
    for (auto _ : state) {
        auto result = index->searchKnn(query.data(), k);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_HNSW_VaryingDataSize)
    ->Arg(100)->Arg(250)->Arg(500)->Arg(1000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Normalized_vs_Raw(benchmark::State& state) {
    const size_t dim = 128;
    const size_t data_size = 500;
    const size_t k = 10;
    const uint32_t seed = 42;
    const bool normalize = state.range(0) == 1;

    // Generate data
    VectorGenerator gen(dim, seed, normalize);
    auto data_points = gen.generateBatch(data_size);
    auto query = gen.generate();

    // Build index
    auto space = std::make_unique<L2Space<float>>(dim);
    auto index = std::make_unique<BruteForceSearch<float>>(std::move(space), data_size);
    for (size_t i = 0; i < data_size; ++i) {
        index->addPoint(data_points[i].data(), i);
    }

    // Benchmark query
    for (auto _ : state) {
        auto result = index->searchKnn(query.data(), k);
        benchmark::DoNotOptimize(result);
    }
    state.SetLabel(normalize ? "Normalized" : "Raw");
}
BENCHMARK(BM_Normalized_vs_Raw)
    ->Arg(0)  // Raw
    ->Arg(1)  // Normalized
    ->Unit(benchmark::kMicrosecond);

} // namespace arena_hnswlib

BENCHMARK_MAIN();
