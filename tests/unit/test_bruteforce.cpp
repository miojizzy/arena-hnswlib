#include "gtest/gtest.h"
#include "arena-hnswlib/bruteforce.h"
#include "arena-hnswlib/space_ip.h"

using namespace arena_hnswlib;

TEST(BruteForceSearchTest, AddPoint) {
    auto space = std::make_unique<InnerProductSpace<float>>(3);
    BruteForceSearch<float> bfs(std::move(space), 10);

    float point[3] = {1.0f, 2.0f, 3.0f};
    bfs.addPoint(point, 0);

    // Add assertions to verify the point was added correctly
}

TEST(BruteForceSearchTest, AddPointExceedsMaxElements) {
    auto space = std::make_unique<InnerProductSpace<float>>(3);
    BruteForceSearch<float> bfs(std::move(space), 1);

    float point1[3] = {1.0f, 2.0f, 3.0f};
    float point2[3] = {4.0f, 5.0f, 6.0f};

    bfs.addPoint(point1, 0);
    EXPECT_THROW(bfs.addPoint(point2, 1), std::runtime_error);
}