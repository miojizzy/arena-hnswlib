#include "gtest/gtest.h"
#include "arena-hnswlib/space_ip.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <random>

using namespace arena_hnswlib;

// ---------------------------------------------------------------------------
// Scalar InnerProduct
// ---------------------------------------------------------------------------

TEST(InnerProductTest, FloatBasic) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    EXPECT_FLOAT_EQ(InnerProduct<float>(vec1, vec2, 3), 32.0f);
}

TEST(InnerProductTest, DoubleBasic) {
    double vec1[] = {1.0, 2.0, 3.0};
    double vec2[] = {4.0, 5.0, 6.0};
    EXPECT_DOUBLE_EQ(InnerProduct<double>(vec1, vec2, 3), 32.0);
}

TEST(InnerProductTest, OrthogonalVectors) {
    // Orthogonal unit vectors: IP should be 0
    float vec1[] = {1.0f, 0.0f, 0.0f};
    float vec2[] = {0.0f, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(InnerProduct<float>(vec1, vec2, 3), 0.0f);
}

TEST(InnerProductTest, ParallelUnitVectors) {
    // Identical unit vectors: IP should be 1
    float vec1[] = {1.0f, 0.0f, 0.0f};
    EXPECT_FLOAT_EQ(InnerProduct<float>(vec1, vec1, 3), 1.0f);
}

TEST(InnerProductTest, NegativeValues) {
    float vec1[] = {-1.0f, -2.0f, 3.0f};
    float vec2[] = {1.0f, 2.0f, 3.0f};
    // -1 + -4 + 9 = 4
    EXPECT_FLOAT_EQ(InnerProduct<float>(vec1, vec2, 3), 4.0f);
}

TEST(InnerProductTest, ZeroVector) {
    float vec1[] = {0.0f, 0.0f, 0.0f};
    float vec2[] = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(InnerProduct<float>(vec1, vec2, 3), 0.0f);
}

TEST(InnerProductTest, Commutativity) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    EXPECT_FLOAT_EQ(InnerProduct<float>(vec1, vec2, 3),
                    InnerProduct<float>(vec2, vec1, 3));
}

// ---------------------------------------------------------------------------
// InnerProductDistance
// ---------------------------------------------------------------------------

TEST(InnerProductDistanceTest, FloatBasic) {
    float vec1[] = {1.0f, 0.0f, 0.0f};
    float vec2[] = {1.0f, 0.0f, 0.0f};
    // IP = 1 → distance = 1 - 1 = 0
    EXPECT_NEAR(InnerProductDistance<float>(vec1, vec2, 3), 0.0f, 1e-6f);
}

TEST(InnerProductDistanceTest, OrthogonalDistance) {
    float vec1[] = {1.0f, 0.0f, 0.0f};
    float vec2[] = {0.0f, 1.0f, 0.0f};
    // IP = 0 → distance = 1 - 0 = 1
    EXPECT_NEAR(InnerProductDistance<float>(vec1, vec2, 3), 1.0f, 1e-6f);
}

TEST(InnerProductDistanceTest, IsSymmetric) {
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    EXPECT_FLOAT_EQ(InnerProductDistance<float>(vec1, vec2, 3),
                    InnerProductDistance<float>(vec2, vec1, 3));
}

// ---------------------------------------------------------------------------
// SIMD consistency: scalar vs InnerProductSIMD
// ---------------------------------------------------------------------------

// Generates a reproducible random float vector of given length
static std::vector<float> makeFloatVec(size_t dim, unsigned seed = 0) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(dim);
    for (auto &x : v) x = dist(rng);
    return v;
}

static std::vector<double> makeDoubleVec(size_t dim, unsigned seed = 0) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    std::vector<double> v(dim);
    for (auto &x : v) x = dist(rng);
    return v;
}

// dim divisible by 16 → goes through IPSqrSIMDAVX directly
TEST(InnerProductSIMDTest, FloatDimAligned16) {
    const size_t dim = 128;  // 128 % 16 == 0
    auto a = makeFloatVec(dim, 1);
    auto b = makeFloatVec(dim, 2);
    float scalar = InnerProduct<float>(a.data(), b.data(), dim);
    float simd   = InnerProductSIMD<float>(a.data(), b.data(), dim);
    EXPECT_NEAR(simd, scalar, std::abs(scalar) * 1e-5f + 1e-7f);
}

// dim not divisible by 16 → goes through IPSqrSIMD16ExtResiduals
TEST(InnerProductSIMDTest, FloatDimUnaligned) {
    const size_t dim = 130;  // 130 % 16 == 2
    auto a = makeFloatVec(dim, 3);
    auto b = makeFloatVec(dim, 4);
    float scalar = InnerProduct<float>(a.data(), b.data(), dim);
    float simd   = InnerProductSIMD<float>(a.data(), b.data(), dim);
    EXPECT_NEAR(simd, scalar, std::abs(scalar) * 1e-5f + 1e-7f);
}

// dim less than 16 → entirely scalar residual path
TEST(InnerProductSIMDTest, FloatDimSmall) {
    const size_t dim = 7;
    auto a = makeFloatVec(dim, 5);
    auto b = makeFloatVec(dim, 6);
    float scalar = InnerProduct<float>(a.data(), b.data(), dim);
    float simd   = InnerProductSIMD<float>(a.data(), b.data(), dim);
    EXPECT_NEAR(simd, scalar, std::abs(scalar) * 1e-5f + 1e-7f);
}

TEST(InnerProductSIMDTest, DoubleDimAligned16) {
    // For double, AVX processes 4 doubles per register, 2 unrolled = 8 per loop.
    // The alignment threshold is still >> 4 << 4 in doubles, i.e. multiples of 16 doubles.
    const size_t dim = 32;  // 32 % 16 == 0
    auto a = makeDoubleVec(dim, 7);
    auto b = makeDoubleVec(dim, 8);
    double scalar = InnerProduct<double>(a.data(), b.data(), dim);
    double simd   = InnerProductSIMD<double>(a.data(), b.data(), dim);
    EXPECT_NEAR(simd, scalar, std::abs(scalar) * 1e-12 + 1e-15);
}

TEST(InnerProductSIMDTest, DoubleDimUnaligned) {
    const size_t dim = 35;  // 35 % 16 == 3
    auto a = makeDoubleVec(dim, 9);
    auto b = makeDoubleVec(dim, 10);
    double scalar = InnerProduct<double>(a.data(), b.data(), dim);
    double simd   = InnerProductSIMD<double>(a.data(), b.data(), dim);
    EXPECT_NEAR(simd, scalar, std::abs(scalar) * 1e-12 + 1e-15);
}

// Large dimension stress test
TEST(InnerProductSIMDTest, FloatLargeDim) {
    const size_t dim = 1536;  // common embedding dimension, 1536 % 16 == 0
    auto a = makeFloatVec(dim, 11);
    auto b = makeFloatVec(dim, 12);
    float scalar = InnerProduct<float>(a.data(), b.data(), dim);
    float simd   = InnerProductSIMD<float>(a.data(), b.data(), dim);
    EXPECT_NEAR(simd, scalar, std::abs(scalar) * 1e-4f + 1e-6f);
}

// ---------------------------------------------------------------------------
// InnerProductSpace
// ---------------------------------------------------------------------------

TEST(InnerProductSpaceTest, DimAndDataSize) {
    InnerProductSpace<float> space(128);
    EXPECT_EQ(space.getDim(), 128u);
    EXPECT_EQ(space.getDataSize(), 128 * sizeof(float));
}

TEST(InnerProductSpaceTest, DistFuncNotNull) {
    InnerProductSpace<float> space(3);
    EXPECT_NE(space.getDistFunc(), nullptr);
}

TEST(InnerProductSpaceTest, DistFuncCorrectness) {
    InnerProductSpace<float> space(3);
    float vec1[] = {1.0f, 0.0f, 0.0f};
    float vec2[] = {1.0f, 0.0f, 0.0f};
    // Identical unit vectors → IP=1 → distance=0
    EXPECT_NEAR(space.getDistFunc()(vec1, vec2, 3), 0.0f, 1e-6f);
}

TEST(InnerProductSpaceTest, StaticDistFuncConsistency) {
    const size_t dim = 128;
    auto a = makeFloatVec(dim, 20);
    auto b = makeFloatVec(dim, 21);
    InnerProductSpace<float> space(dim);

    float via_ptr    = space.getDistFunc()(a.data(), b.data(), dim);
    float via_static = InnerProductSpace<float>::distFunc(a.data(), b.data(), dim);
    EXPECT_FLOAT_EQ(via_ptr, via_static);
}

TEST(InnerProductSpaceTest, SymmetricDistance) {
    InnerProductSpace<float> space(3);
    float vec1[] = {1.0f, 2.0f, 3.0f};
    float vec2[] = {4.0f, 5.0f, 6.0f};
    auto fn = space.getDistFunc();
    EXPECT_FLOAT_EQ(fn(vec1, vec2, 3), fn(vec2, vec1, 3));
}