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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}