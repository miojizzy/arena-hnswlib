#pragma once

namespace arena_hnswlib {

// A simple function to add two numbers
inline int add(int a, int b) {
    return a + b;
}

// A simple function to multiply two numbers
inline int multiply(int a, int b) {
    return a * b;
}

#include <immintrin.h>

template<typename T> struct SimdTraits;

template<>
struct SimdTraits<float> {
    using reg128 = __m128;
    using reg256 = __m256;
    using reg512 = __m512;
    static constexpr size_t pack128 = 4;
    static constexpr size_t pack256 = 8;
    static constexpr size_t pack512 = 16;
};

template<>
struct SimdTraits<double> {
    using reg128 = __m128d;
    using reg256 = __m256d;
    using reg512 = __m512d;
    static constexpr size_t pack128 = 2;
    static constexpr size_t pack256 = 4;
    static constexpr size_t pack512 = 8;
};


} // namespace arena_hnswlib
