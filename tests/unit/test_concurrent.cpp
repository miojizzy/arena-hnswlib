#include "gtest/gtest.h"
#include "arena-hnswlib/bruteforce.h"
#include "arena-hnswlib/hnswalg.h"
#include "arena-hnswlib/space_l2.h"
#include "arena-hnswlib/space_ip.h"

#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <numeric>
#include <cmath>

using namespace arena_hnswlib;

// -----------------------------------------------------------------------
// 辅助：生成 dim 维归一化随机向量（固定种子以保证可重复）
// -----------------------------------------------------------------------
static std::vector<float> makeVector(size_t dim, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    std::vector<float> v(dim);
    for (auto& x : v) x = dist(rng);
    float norm = 0.f;
    for (auto x : v) norm += x * x;
    norm = std::sqrt(norm);
    for (auto& x : v) x /= norm;
    return v;
}

// -----------------------------------------------------------------------
// 测试1：多线程并发搜索 BruteForceSearch（纯读，天然线程安全）
// -----------------------------------------------------------------------
TEST(ConcurrentTest, BruteForceParallelSearch) {
    constexpr size_t DIM         = 64;
    constexpr size_t NUM_POINTS  = 500;
    constexpr size_t NUM_QUERIES = 8;   // 每线程一个 query，共 8 线程
    constexpr size_t K           = 5;
    constexpr int    NUM_THREADS = 8;

    // 构建索引（单线程）
    BruteForceSearch<float> bf(std::make_unique<L2Space<float>>(DIM), NUM_POINTS);

    for (size_t i = 0; i < NUM_POINTS; ++i) {
        auto v = makeVector(DIM, static_cast<unsigned>(i));
        bf.addPoint(v.data(), static_cast<LabelType>(i));
    }

    // 准备查询向量（与 bf 中某些点完全相同，最近邻应为自身）
    std::vector<std::vector<float>> queries(NUM_QUERIES);
    for (int t = 0; t < NUM_THREADS; ++t) {
        queries[t] = makeVector(DIM, static_cast<unsigned>(t * 10)); // 与索引中第 10*t 号点相同
    }

    std::vector<LabelType> nearest_labels(NUM_THREADS, static_cast<LabelType>(-1));
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            auto result = bf.searchKnn(queries[t].data(), K);
            // 收集最近邻 label（priority_queue 顶为距离最大，故取最小需看全部）
            float best_dist = std::numeric_limits<float>::max();
            LabelType best_label = static_cast<LabelType>(-1);
            while (!result.empty()) {
                auto [d, label] = result.top();
                result.pop();
                if (d < best_dist) {
                    best_dist = d;
                    best_label = label;
                }
            }
            nearest_labels[t] = best_label;
        });
    }

    for (auto& th : threads) th.join();

    // 每个线程的最近邻应是对应的索引点（距离接近 0）
    for (int t = 0; t < NUM_THREADS; ++t) {
        EXPECT_EQ(nearest_labels[t], static_cast<LabelType>(t * 10))
            << "Thread " << t << " got wrong nearest neighbor";
    }
}

// -----------------------------------------------------------------------
// 测试2：多线程分别独立构建各自的 HNSW 索引，验证互不干扰
// -----------------------------------------------------------------------
TEST(ConcurrentTest, HNSWParallelIndependentBuild) {
    constexpr size_t DIM        = 32;
    constexpr size_t NUM_POINTS = 200;
    constexpr size_t M          = 8;
    constexpr size_t EF         = 16;
    constexpr int    NUM_THREADS = 4;
    constexpr size_t K          = 3;

    // 预生成所有向量，供各线程共享（只读）
    std::vector<std::vector<float>> vectors(NUM_POINTS);
    for (size_t i = 0; i < NUM_POINTS; ++i) {
        vectors[i] = makeVector(DIM, static_cast<unsigned>(i + 1000));
    }

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            // 每个线程独立创建自己的 HNSW 索引
            InnerProductSpace<float> space(DIM);
            HierarchicalNSW<float, InnerProductSpace<float>> hnsw(
                space, NUM_POINTS, M, EF, static_cast<size_t>(42 + t));

            for (size_t i = 0; i < NUM_POINTS; ++i) {
                hnsw.addPoint(vectors[i].data(), static_cast<LabelType>(i));
            }

            // 用第 0 号向量查询，结果中应包含 label 0（完全匹配）
            auto result = hnsw.searchKnn(vectors[0].data(), K);
            bool found_self = false;
            while (!result.empty()) {
                if (result.top().second == 0) { found_self = true; }
                result.pop();
            }
            if (!found_self) {
                failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : threads) th.join();

    EXPECT_EQ(failures.load(), 0)
        << "One or more HNSW threads failed to find the exact-match nearest neighbor";
}

// -----------------------------------------------------------------------
// 测试3：多线程真正并发搜索同一个只读 HNSW 索引
//        searchKnn 现在使用请求粒度的局部 VisitedTable，无数据竞争
// -----------------------------------------------------------------------
TEST(ConcurrentTest, HNSWParallelConcurrentSearch) {
    constexpr size_t DIM         = 32;
    constexpr size_t NUM_POINTS  = 300;
    constexpr size_t M           = 8;
    constexpr size_t EF          = 16;
    constexpr int    NUM_THREADS = 8;
    constexpr size_t K           = 1;

    // 预生成向量
    std::vector<std::vector<float>> vectors(NUM_POINTS);
    for (size_t i = 0; i < NUM_POINTS; ++i) {
        vectors[i] = makeVector(DIM, static_cast<unsigned>(i + 3000));
    }

    // 单线程构建索引
    InnerProductSpace<float> space(DIM);
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(space, NUM_POINTS, M, EF, 77);
    for (size_t i = 0; i < NUM_POINTS; ++i) {
        hnsw.addPoint(vectors[i].data(), static_cast<LabelType>(i));
    }

    // 多线程同时并发搜索：每个线程查询各自的 query（精确匹配索引中的某个点）
    // 由于 VisitedTable 已改为请求粒度局部变量，此处无数据竞争
    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            // 每个线程查询不同的点，期望精确找回自身
            size_t query_idx = static_cast<size_t>(t) * (NUM_POINTS / NUM_THREADS);
            auto result = hnsw.searchKnn(vectors[query_idx].data(), K);
            if (result.empty() || result.top().second != static_cast<LabelType>(query_idx)) {
                mismatches.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : threads) th.join();

    EXPECT_EQ(mismatches.load(), 0)
        << "Concurrent HNSW searches produced incorrect results";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
