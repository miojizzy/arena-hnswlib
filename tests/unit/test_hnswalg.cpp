#include "gtest/gtest.h"
#include "arena-hnswlib/hnswalg.h"
#include "arena-hnswlib/space_ip.h"
#include <vector>

using namespace arena_hnswlib;

TEST(HierarchicalNSWTest, LinkListInitialization) {
    size_t M = 2;
    size_t elementSize = 5;
    LinkLists linkLists(M, elementSize);
    // floor(5/2)=2，floor(2/2)=1（停止），强制顶层=1
    // level_sizes = [5, 1]，maxLevel=1（0层5个，1层1个）
    EXPECT_EQ(linkLists.getMaxLevel(), 1);
    // id=0 是唯一的顶层入口节点
    EXPECT_EQ(linkLists.getLevel(0), 1);
    EXPECT_EQ(linkLists.getLevel(1), 0);
    EXPECT_EQ(linkLists.getLevel(2), 0);
    EXPECT_EQ(linkLists.getLevel(3), 0);
    EXPECT_EQ(linkLists.getLevel(4), 0);
}

TEST(HierarchicalNSWTest, LinkListInitializationM4) {
    size_t M = 4;
    size_t elementSize = 20;
    LinkLists linkLists(M, elementSize);
    // floor(20/4)=5, floor(5/4)=1（停止），顶层已是1无需强制
    // level_sizes = [20, 5, 1]，maxLevel=2（0层20个，1层5个，2层1个）
    EXPECT_EQ(linkLists.getMaxLevel(), 2);
    // 链表大小：level0 = 2*M+1 = 9，level1/2 = M+1 = 5
    EXPECT_EQ(linkLists.getLinkListSize(0), 2 * M + 1);
    EXPECT_EQ(linkLists.getLinkListSize(1), M + 1);
    EXPECT_EQ(linkLists.getLinkListSize(2), M + 1);
    // id=0 是唯一的顶层(level 2)入口节点
    EXPECT_EQ(linkLists.getLevel(0), 2);
    // id=1..4 在 level 1
    for (size_t i = 1; i <= 4; ++i) {
        EXPECT_EQ(linkLists.getLevel(i), 1) << "id=" << i;
    }
    // id=5..19 在 level 0
    for (size_t i = 5; i < elementSize; ++i) {
        EXPECT_EQ(linkLists.getLevel(i), 0) << "id=" << i;
    }
}

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
