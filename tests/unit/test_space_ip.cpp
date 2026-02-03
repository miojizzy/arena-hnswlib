#include "gtest/gtest.h"
#include "arena-hnswlib/space_ip.h"

using namespace arena_hnswlib;

TEST(InnerProductTest, FloatInnerProduct) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    size_t dim = 3;
    float result = InnerProduct<float>(vec1, vec2, &dim);
    EXPECT_FLOAT_EQ(result, 32.0f);
}

TEST(InnerProductTest, DoubleInnerProduct) {
    double vec1[] = {1.0, 2.0, 3.0};
    double vec2[] = {4.0, 5.0, 6.0};
    size_t dim = 3;
    double result = InnerProduct<double>(vec1, vec2, &dim);
    EXPECT_DOUBLE_EQ(result, 32.0);
}

TEST(InnerProductSpaceTest, BasicFunctionality) {
    InnerProductSpace<float> space(3);
    EXPECT_EQ(space.getDataSize(), 3 * sizeof(float));
    EXPECT_NE(space.getDistFunc(), nullptr);
}