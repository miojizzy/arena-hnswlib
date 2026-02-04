#include "gtest/gtest.h"
#include "arena-hnswlib/math_utils.h"

TEST(MathUtilsTest, AddTest) {
    EXPECT_EQ(arena_hnswlib::add(2, 3), 5);
    EXPECT_EQ(arena_hnswlib::add(-1, 1), 0);
    EXPECT_EQ(arena_hnswlib::add(0, 0), 0);
}

TEST(MathUtilsTest, MultiplyTest) {
    EXPECT_EQ(arena_hnswlib::multiply(2, 3), 6);
    EXPECT_EQ(arena_hnswlib::multiply(-1, 1), -1);
    EXPECT_EQ(arena_hnswlib::multiply(0, 5), 0);
}

// L2 Distance Squared Tests
TEST(MathUtilsTest, L2DistanceSquaredBasic) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    size_t dim = 3;
    // (4-1)^2 + (5-2)^2 + (6-3)^2 = 9 + 9 + 9 = 27
    float result = arena_hnswlib::L2DistanceSquared(vec1, vec2, dim);
    EXPECT_FLOAT_EQ(result, 27.0f);
}

TEST(MathUtilsTest, L2DistanceSquaredIdentical) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {1.0f, 2.0f, 3.0f};
    size_t dim = 3;
    float result = arena_hnswlib::L2DistanceSquared(vec1, vec2, dim);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(MathUtilsTest, L2DistanceSquaredZeroVectors) {
    float vec1[] = {0.0f, 0.0f, 0.0f};
    float vec2[] = {0.0f, 0.0f, 0.0f};
    size_t dim = 3;
    float result = arena_hnswlib::L2DistanceSquared(vec1, vec2, dim);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(MathUtilsTest, L2DistanceSquaredNegativeValues) {
    float vec1[] = {-1.0f, -2.0f, -3.0f};
    float vec2[] = {1.0f, 2.0f, 3.0f};
    size_t dim = 3;
    // (1-(-1))^2 + (2-(-2))^2 + (3-(-3))^2 = 4 + 16 + 36 = 56
    float result = arena_hnswlib::L2DistanceSquared(vec1, vec2, dim);
    EXPECT_FLOAT_EQ(result, 56.0f);
}

TEST(MathUtilsTest, L2DistanceSquaredDouble) {
    double vec1[] = {1.0, 2.0, 3.0};
    double vec2[] = {4.0, 5.0, 6.0};
    size_t dim = 3;
    double result = arena_hnswlib::L2DistanceSquared(vec1, vec2, dim);
    EXPECT_DOUBLE_EQ(result, 27.0);
}

TEST(MathUtilsTest, L2DistanceSquaredSingleDimension) {
    float vec1[] = {5.0f};
    float vec2[] = {2.0f};
    size_t dim = 1;
    float result = arena_hnswlib::L2DistanceSquared(vec1, vec2, dim);
    EXPECT_FLOAT_EQ(result, 9.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}