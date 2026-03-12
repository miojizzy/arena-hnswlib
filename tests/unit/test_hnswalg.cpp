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

// ============================================================================
// Layer0NeighborMode 测试
// ============================================================================

TEST(HierarchicalNSWTest, Layer0NeighborModeDoubleM) {
    // 模式1: 0层使用 2*M 个邻居（默认模式）
    size_t M = 4;
    size_t elementSize = 100;
    LinkLists linkLists(M, elementSize, Layer0NeighborMode::kDoubleM);
    
    // 0层邻居容量应为 2*M
    EXPECT_EQ(linkLists.getM(0), 2 * M);
    // 其他层邻居容量应为 M
    if (linkLists.getMaxLevel() > 0) {
        EXPECT_EQ(linkLists.getM(1), M);
    }
    EXPECT_EQ(linkLists.getMode(), Layer0NeighborMode::kDoubleM);
}

TEST(HierarchicalNSWTest, Layer0NeighborModeHeuristicOnly) {
    // 模式2: 0层只使用 M 个邻居
    size_t M = 4;
    size_t elementSize = 100;
    LinkLists linkLists(M, elementSize, Layer0NeighborMode::kHeuristicOnly);
    
    // 0层邻居容量应为 M
    EXPECT_EQ(linkLists.getM(0), M);
    // 其他层邻居容量应为 M
    if (linkLists.getMaxLevel() > 0) {
        EXPECT_EQ(linkLists.getM(1), M);
    }
    EXPECT_EQ(linkLists.getMode(), Layer0NeighborMode::kHeuristicOnly);
}

TEST(HierarchicalNSWTest, Layer0NeighborModeHeuristicPlusClosest) {
    // 模式3: 0层使用 M 个启发式 + M 个最近邻居 = 2*M
    size_t M = 4;
    size_t elementSize = 100;
    LinkLists linkLists(M, elementSize, Layer0NeighborMode::kHeuristicPlusClosest);
    
    // 0层邻居容量应为 2*M
    EXPECT_EQ(linkLists.getM(0), 2 * M);
    // 其他层邻居容量应为 M
    if (linkLists.getMaxLevel() > 0) {
        EXPECT_EQ(linkLists.getM(1), M);
    }
    EXPECT_EQ(linkLists.getMode(), Layer0NeighborMode::kHeuristicPlusClosest);
}

TEST(HierarchicalNSWTest, DefaultModeIsDoubleM) {
    // 默认模式应为 kDoubleM，保持向后兼容
    size_t M = 4;
    size_t elementSize = 100;
    LinkLists linkLists(M, elementSize);  // 不指定模式
    
    EXPECT_EQ(linkLists.getM(0), 2 * M);
    EXPECT_EQ(linkLists.getMode(), Layer0NeighborMode::kDoubleM);
}

TEST(HierarchicalNSWTest, BuildIndexWithDifferentModes) {
    size_t dim = 8;
    size_t max_elements = 50;
    size_t M = 4;
    size_t ef = 10;
    
    // 构造测试数据
    std::vector<std::vector<float>> points;
    for (size_t i = 0; i < max_elements; ++i) {
        std::vector<float> point(dim);
        for (size_t j = 0; j < dim; ++j) {
            point[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        points.push_back(point);
    }
    
    // 测试三种模式都能正常构建索引并搜索
    for (int mode_idx = 0; mode_idx < 3; ++mode_idx) {
        Layer0NeighborMode mode;
        switch (mode_idx) {
            case 0: mode = Layer0NeighborMode::kDoubleM; break;
            case 1: mode = Layer0NeighborMode::kHeuristicOnly; break;
            case 2: mode = Layer0NeighborMode::kHeuristicPlusClosest; break;
        }
        
        HierarchicalNSW<float, InnerProductSpace<float>> hnsw(
            InnerProductSpace<float>(dim), max_elements, M, ef, 42, mode);
        
        for (size_t i = 0; i < points.size(); ++i) {
            hnsw.addPoint(points[i].data(), static_cast<uint32_t>(i));
        }
        
        // 验证搜索功能正常
        auto result = hnsw.searchKnn(points[0].data(), 5);
        EXPECT_GE(result.size(), 1);
        
        // 查询点本身应该是最接近的点之一
        bool found_self = false;
        while (!result.empty()) {
            if (result.top().second == 0) {
                found_self = true;
                break;
            }
            result.pop();
        }
        EXPECT_TRUE(found_self) << "Mode " << mode_idx << " failed to find self";
    }
}

// ============================================================================
// 搜索调试统计测试
// ============================================================================

TEST(HierarchicalNSWTest, SearchWithStats) {
    size_t dim = 8;
    size_t max_elements = 50;
    size_t M = 4;
    size_t ef = 10;
    
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(
        InnerProductSpace<float>(dim), max_elements, M, ef, 42);
    
    // 构造测试数据
    std::vector<std::vector<float>> points;
    for (size_t i = 0; i < max_elements; ++i) {
        std::vector<float> point(dim);
        for (size_t j = 0; j < dim; ++j) {
            point[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        points.push_back(point);
        hnsw.addPoint(point.data(), static_cast<uint32_t>(i));
    }
    
    // 使用带统计的搜索
    auto [result, stats] = hnsw.searchKnnWithStats(points[0].data(), 5);
    
    // 验证搜索结果
    EXPECT_GE(result.size(), 1);
    EXPECT_LE(result.size(), 5);
    
    // 验证统计信息
    EXPECT_GT(stats.visited_nodes, 0);
    EXPECT_GT(stats.base_level.dist_calcs, 0);
    EXPECT_EQ(stats.visited_nodes, stats.visited_in_level0.size());
    
    // 高层统计（如果有高层）
    if (stats.high_level_layers > 0) {
        EXPECT_GT(stats.high_level_total.dist_calcs, 0);
    }
}

TEST(HierarchicalNSWTest, AnalyzeMissedPoint) {
    size_t dim = 8;
    size_t max_elements = 50;
    size_t M = 4;
    size_t ef = 5;  // 较小的 ef，容易产生缺失
    
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(
        InnerProductSpace<float>(dim), max_elements, M, ef, 42);
    
    // 构造测试数据
    std::vector<std::vector<float>> points;
    for (size_t i = 0; i < max_elements; ++i) {
        std::vector<float> point(dim);
        for (size_t j = 0; j < dim; ++j) {
            point[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        points.push_back(point);
        hnsw.addPoint(point.data(), static_cast<uint32_t>(i));
    }
    
    // 搜索并分析
    auto [result, stats] = hnsw.searchKnnWithStats(points[0].data(), 5);
    
    // 分析一个缺失点（假设点 1 是缺失的）
    float dist = InnerProductSpace<float>::distFunc(
        points[0].data(), points[1].data(), dim);
    auto analysis = hnsw.analyzeMissedPoint(1, stats, static_cast<double>(dist));
    
    // 验证分析结果
    EXPECT_EQ(analysis.point_id, 1);
    // 注意：InnerProduct 距离可能为负，只需验证已被计算
    EXPECT_NE(analysis.dist, 0.0);
    
    // 如果被访问过，min_ef_to_reach 应该是 0
    // 如果未被访问，min_ef_to_reach 应该大于 0
    if (analysis.was_visited) {
        EXPECT_EQ(analysis.min_ef_to_reach, 0);
    } else if (analysis.reachable) {
        EXPECT_GT(analysis.min_ef_to_reach, 0);
    }
}

TEST(HierarchicalNSWTest, ConnectivityAnalysis) {
    size_t dim = 8;
    size_t max_elements = 50;
    size_t M = 4;
    size_t ef = 10;
    
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(
        InnerProductSpace<float>(dim), max_elements, M, ef, 42);
    
    // 构造测试数据
    for (size_t i = 0; i < max_elements; ++i) {
        std::vector<float> point(dim);
        for (size_t j = 0; j < dim; ++j) {
            point[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        hnsw.addPoint(point.data(), static_cast<uint32_t>(i));
    }
    
    // 分析连通性
    auto report = hnsw.analyzeConnectivity();
    
    // 验证报告结构
    EXPECT_EQ(report.from_entry.size(), report.from_upper_layer.size());
    EXPECT_GT(report.from_entry.size(), 0);
    
    // 每层都应该有统计信息
    for (const auto& level_stats : report.from_entry) {
        EXPECT_GT(level_stats.total_nodes, 0);
        EXPECT_EQ(level_stats.reachable_nodes + level_stats.unreachable_nodes,
                  level_stats.total_nodes);
    }
}

TEST(HierarchicalNSWTest, SearchStatsConsistency) {
    // 验证带统计和不带统计的搜索结果一致
    size_t dim = 8;
    size_t max_elements = 50;
    size_t M = 4;
    size_t ef = 10;
    
    HierarchicalNSW<float, InnerProductSpace<float>> hnsw(
        InnerProductSpace<float>(dim), max_elements, M, ef, 42);
    
    // 构造测试数据
    std::vector<std::vector<float>> points;
    for (size_t i = 0; i < max_elements; ++i) {
        std::vector<float> point(dim);
        for (size_t j = 0; j < dim; ++j) {
            point[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        points.push_back(point);
        hnsw.addPoint(point.data(), static_cast<uint32_t>(i));
    }
    
    // 不带统计搜索
    auto result1 = hnsw.searchKnn(points[0].data(), 5);
    
    // 带统计搜索
    auto [result2, stats] = hnsw.searchKnnWithStats(points[0].data(), 5);
    
    // 结果数量应该一致
    EXPECT_EQ(result1.size(), result2.size());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
