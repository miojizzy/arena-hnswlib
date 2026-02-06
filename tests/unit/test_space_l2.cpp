#include "gtest/gtest.h"
#include "arena-hnswlib/space_l2.h"
#include <cmath>

using namespace arena_hnswlib;

TEST(L2SquaredTest, FloatL2Squared) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    size_t dim = 3;
    // (4-1)^2 + (5-2)^2 + (6-3)^2 = 9 + 9 + 9 = 27
    float result = L2Squared<float>(vec1, vec2, &dim);
    EXPECT_FLOAT_EQ(result, 27.0f);
}

TEST(L2SquaredTest, DoubleL2Squared) {
    double vec1[] = {1.0, 2.0, 3.0};
    double vec2[] = {4.0, 5.0, 6.0};
    size_t dim = 3;
    double result = L2Squared<double>(vec1, vec2, &dim);
    EXPECT_DOUBLE_EQ(result, 27.0);
}

TEST(L2SquaredTest, IdenticalVectors) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {1.0f, 2.0f, 3.0f};
    size_t dim = 3;
    float result = L2Squared<float>(vec1, vec2, &dim);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(L2SquaredTest, NegativeValues) {
    float vec1[] = {-1.0f, -2.0f, -3.0f};
    float vec2[] = {1.0f, 2.0f, 3.0f};
    size_t dim = 3;
    // (1-(-1))^2 + (2-(-2))^2 + (3-(-3))^2 = 4 + 16 + 36 = 56
    float result = L2Squared<float>(vec1, vec2, &dim);
    EXPECT_FLOAT_EQ(result, 56.0f);
}

TEST(L2SpaceTest, BasicFunctionality) {
    L2Space<float> space(3);
    EXPECT_EQ(space.getDim(), 3);
    EXPECT_EQ(space.getDataSize(), 3 * sizeof(float));
    EXPECT_NE(space.getDistFunc(), nullptr);
}

TEST(L2SpaceTest, DistanceFunctionWorks) {
    L2Space<float> space(3);
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    size_t dim = 3;
    
    auto distFunc = space.getDistFunc();
    float result = distFunc(vec1, vec2, &dim);
    EXPECT_FLOAT_EQ(result, 27.0f);
}

TEST(L2SpaceTest, DistanceIsSymmetric) {
    L2Space<float> space(3);
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    size_t dim = 3;
    
    auto distFunc = space.getDistFunc();
    float dist1 = distFunc(vec1, vec2, &dim);
    float dist2 = distFunc(vec2, vec1, &dim);
    EXPECT_FLOAT_EQ(dist1, dist2);
}

TEST(L2SpaceTest, DistanceIsNonNegative) {
    L2Space<float> space(3);
    float vec1[] = {-10.0f, 20.0f, -30.0f};
    float vec2[] = {5.0f, -15.0f, 25.0f};
    size_t dim = 3;
    
    auto distFunc = space.getDistFunc();
    float result = distFunc(vec1, vec2, &dim);
    EXPECT_GE(result, 0.0f);
}

TEST(L2SpaceTest, TriangleInequality) {
    L2Space<float> space(3);
    float vec1[] = {0.0f, 0.0f, 0.0f};
    float vec2[] = {1.0f, 1.0f, 1.0f};
    float vec3[] = {2.0f, 2.0f, 2.0f};
    size_t dim = 3;
    
    auto distFunc = space.getDistFunc();
    float d12 = distFunc(vec1, vec2, &dim);
    float d23 = distFunc(vec2, vec3, &dim);
    float d13 = distFunc(vec1, vec3, &dim);
    
    // For squared distance, the triangle inequality becomes:
    // sqrt(d13) <= sqrt(d12) + sqrt(d23)
    // We can verify this by checking: d13 <= (sqrt(d12) + sqrt(d23))^2
    EXPECT_LE(std::sqrt(d13), std::sqrt(d12) + std::sqrt(d23));
}
