#pragma once

#include "arena_hnswlib.h"
#include "math_utils.h"

namespace arena_hnswlib {

// Compute the squared L2 (Euclidean) distance between two vectors
// Returns sum of squared differences without computing square root
// This is sufficient for ranking purposes and avoids expensive sqrt operation
template<typename dist_t>
inline static dist_t L2Squared(const dist_t* pVect1, const dist_t* pVect2, size_t dim) {
    dist_t res = 0;
    for (size_t i = 0; i < dim; i++) {
        const dist_t diff = pVect1[i] - pVect2[i];
        res += diff * diff;
    }
    return res;
}

#if defined(USE_AVX)
template<typename dist_t>
dist_t L2SqrSIMDAVX(const dist_t *pVect1, const dist_t *pVect2, const size_t dim);
#endif

#if defined(USE_AVX)
template<>
float L2SqrSIMDAVX<float>(const float *pVect1, const float *pVect2, const size_t dim) {
    float PORTABLE_ALIGN64 TmpRes[8];
    const float *pEnd1 = pVect1 + (dim >> 4 << 4);
    __m256 diff, v1, v2;
    __m256 sum = _mm256_set1_ps(0);
    while (pVect1 < pEnd1) {
        // Use unaligned load (loadu) to handle data that may not be 32-byte aligned
        // std::vector and other containers don't guarantee AVX alignment
        v1 = _mm256_loadu_ps(pVect1);
        pVect1 += 8;
        v2 = _mm256_loadu_ps(pVect2);
        pVect2 += 8;
        diff = _mm256_sub_ps(v1, v2);
        sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));

        v1 = _mm256_loadu_ps(pVect1);
        pVect1 += 8;
        v2 = _mm256_loadu_ps(pVect2);
        pVect2 += 8;
        diff = _mm256_sub_ps(v1, v2);
        sum = _mm256_add_ps(sum, _mm256_mul_ps(diff, diff));
    }
    _mm256_store_ps(TmpRes, sum);
    return TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3] + TmpRes[4] + TmpRes[5] + TmpRes[6] + TmpRes[7];
}

template<>
double L2SqrSIMDAVX<double>(const double *pVect1, const double *pVect2, const size_t dim) {
    double PORTABLE_ALIGN64 TmpRes[4];
    const double *pEnd1 = pVect1 + (dim >> 4 << 4);
    __m256d diff, v1, v2;
    __m256d sum = _mm256_set1_pd(0);
    while (pVect1 < pEnd1) {
        // Use unaligned load (loadu) to handle data that may not be 32-byte aligned
        v1 = _mm256_loadu_pd(pVect1);
        pVect1 += 4;
        v2 = _mm256_loadu_pd(pVect2);
        pVect2 += 4;
        diff = _mm256_sub_pd(v1, v2);
        sum = _mm256_add_pd(sum, _mm256_mul_pd(diff, diff));

        v1 = _mm256_loadu_pd(pVect1);
        pVect1 += 4;
        v2 = _mm256_loadu_pd(pVect2);
        pVect2 += 4;
        diff = _mm256_sub_pd(v1, v2);
        sum = _mm256_add_pd(sum, _mm256_mul_pd(diff, diff));
    }
    _mm256_store_pd(TmpRes, sum);
    return TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3];
}
#endif

#if defined(USE_AVX)
template<typename dist_t>
static dist_t
L2SqrSIMD16ExtResiduals(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    size_t dim16 = dim >> 4 << 4;
    dist_t res = L2SqrSIMDAVX<dist_t>(pVect1, pVect2, dim16);
    const dist_t *pVect1_tail = pVect1 + dim16;
    const dist_t *pVect2_tail = pVect2 + dim16;

    size_t qty_left = dim - dim16;
    dist_t res_tail = L2Squared(pVect1_tail, pVect2_tail, qty_left);
    return (res + res_tail);
}
#endif

// Squared L2 (Euclidean) distance function
// Returns sum of squared differences between two vectors
template<typename dist_t> 
inline static dist_t 
L2SquaredDistance(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    #if defined(USE_AVX)
    if (dim % 16 == 0) {
        return L2SqrSIMDAVX<dist_t>(pVect1, pVect2, dim);
    } else {
        return L2SqrSIMD16ExtResiduals<dist_t>(pVect1, pVect2, dim);
    }
    #else
    return L2Squared<dist_t>(pVect1, pVect2, dim);
    #endif
}


// L2 Space class using squared Euclidean distance metric
// Note: This uses squared L2 distance (without square root) for efficiency
// The ranking is preserved since sqrt is a monotonic function
template<typename dist_t>
class L2Space : public SpaceInterface<dist_t> {
    DISTFUNC<dist_t> fstdistfunc_;
    size_t data_size_;
    size_t dim_;

 public:
    explicit L2Space(size_t dim) {
        fstdistfunc_ = L2SquaredDistance<dist_t>;
        dim_ = dim;
        data_size_ = dim * sizeof(dist_t);
    }
    ~L2Space() override {}

    // Static inline distance function — callable by HierarchicalNSW<dist_t, L2Space<dist_t>>
    // without going through a function pointer, enabling compiler inlining and auto-vectorization.
    static inline dist_t distFunc(const dist_t* a, const dist_t* b, size_t dim) {
        return L2SquaredDistance<dist_t>(a, b, dim);
    }

    const DISTFUNC<dist_t>& getDistFunc() const override {
        return fstdistfunc_;
    }

    const size_t& getDim() const override {
        return dim_;
    }

    const size_t& getDataSize() const override {
        return data_size_;
    }
};

} // namespace arena_hnswlib
