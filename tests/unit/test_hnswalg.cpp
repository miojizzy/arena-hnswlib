#include "gtest/gtest.h"
#include "arena-hnswlib/hnswalg.h"
#include "arena-hnswlib/space_ip.h"
#include <vector>

using namespace arena_hnswlib;

TEST(HierarchicalNSWTest, Initialization) {
    size_t dim = 3;
    size_t max_elements = 3;
    size_t M = 2;
    size_t ef = 2;
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(InnerProductSpace<float>(dim), max_elements, M, ef, 42);

    // 构造数据点
    std::vector<std::vector<float>> points = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
        {7.0f, 8.0f, 9.0f}
    };
    std::vector<uint32_t> labels = {10, 20, 30};

    for (size_t i = 0; i < points.size(); ++i) {
        hnsw.addPoint(points[i].data(), labels[i]);
    }
    //hnsw.print();
}

TEST(HierarchicalNSWTest, AddPointAndSearch) {
    size_t dim = 3;
    size_t max_elements = 10;
    size_t M = 2;
    size_t ef = 2;
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(InnerProductSpace<float>(dim), max_elements, M, ef, 42);

    // 构造数据点
    std::vector<std::vector<float>> points = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
        {7.0f, 8.0f, 9.0f}
    };
    std::vector<uint32_t> labels = {10, 20, 30};

    for (size_t i = 0; i < points.size(); ++i) {
        hnsw.addPoint(points[i].data(), labels[i]);
    }

    // 查询第一个点
    float query[3] = {1.0f, 2.0f, 3.0f};
    auto result = hnsw.searchKnn(query, 2);
    ASSERT_EQ(result.size(), 2);
    std::vector<std::pair<float, uint32_t>> resvec;
    while (!result.empty()) {
        resvec.push_back(result.top());
        result.pop();
    }
    // 检查最近的点label
    EXPECT_TRUE(resvec[0].second == 20u || resvec[0].second == 30u);
    EXPECT_TRUE(resvec[1].second == 10u || resvec[1].second == 20u || resvec[1].second == 30u);
    
}

TEST(HierarchicalNSWTest, AddPointExceedsMaxElements) {
    size_t dim = 3;
    size_t max_elements = 1;
    size_t M = 2;
    size_t ef = 2;
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(InnerProductSpace<float>(dim), max_elements, M, ef, 42);

    float point1[3] = {1.0f, 2.0f, 3.0f};
    float point2[3] = {4.0f, 5.0f, 6.0f};
    hnsw.addPoint(point1, 0);
    EXPECT_THROW(hnsw.addPoint(point2, 1), std::runtime_error);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
