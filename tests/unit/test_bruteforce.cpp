#include "gtest/gtest.h"
#include "arena-hnswlib/bruteforce.h"
#include "arena-hnswlib/space_ip.h"

using namespace arena_hnswlib;

TEST(BruteForceSearchTest, AddPoint) {
    auto space = std::make_unique<InnerProductSpace<float>>(3);
    BruteForceSearch<float> bfs(std::move(space), 10);

    float point[3] = {1.0f, 2.0f, 3.0f};
    bfs.addPoint(point, 0);

    // 验证点被正确添加（通过 searchKnn 查询）
    float query[3] = {1.0f, 2.0f, 3.0f};
    auto result = bfs.searchKnn(query, 1);
    ASSERT_EQ(result.size(), 1);
    auto top = result.top();
    EXPECT_EQ(top.second, 0u); // label
    EXPECT_NEAR(top.first, 0.0f, 1e-5); // 距离应为0
}
TEST(BruteForceSearchTest, SearchKnnMultiplePoints) {
    auto space = std::make_unique<InnerProductSpace<float>>(3);
    BruteForceSearch<float> bfs(std::move(space), 10);

    float p0[3] = {1.0f, 2.0f, 3.0f};
    float p1[3] = {4.0f, 5.0f, 6.0f};
    float p2[3] = {7.0f, 8.0f, 9.0f};
    bfs.addPoint(p0, 10);
    bfs.addPoint(p1, 20);
    bfs.addPoint(p2, 30);

    float query[3] = {1.0f, 2.0f, 3.0f};
    auto result = bfs.searchKnn(query, 2);
    ASSERT_EQ(result.size(), 2);

    // priority_queue: top() 是距离较大的那个
    std::vector<std::pair<float, uint32_t>> resvec;
    while (!result.empty()) {
        resvec.push_back(result.top());
        result.pop();
    }
    // 距离最近的点应该是 label=10, 其次是20
    EXPECT_EQ(resvec[0].second, 20u);
    EXPECT_EQ(resvec[1].second, 10u);
    EXPECT_LT(resvec[1].first, resvec[0].first);
}

TEST(BruteForceSearchTest, AddPointExceedsMaxElements) {
    auto space = std::make_unique<InnerProductSpace<float>>(3);
    BruteForceSearch<float> bfs(std::move(space), 1);

    float point1[3] = {1.0f, 2.0f, 3.0f};
    float point2[3] = {4.0f, 5.0f, 6.0f};

    bfs.addPoint(point1, 0);
    EXPECT_THROW(bfs.addPoint(point2, 1), std::runtime_error);
}