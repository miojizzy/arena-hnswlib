#pragma once

#include "arena_hnswlib.h"

namespace arena_hnswlib {

// Compute inner product between two vectors (scalar baseline)
template<typename dist_t> 
inline static dist_t 
InnerProduct(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    dist_t res = 0;
    for (size_t i = 0; i < dim; i++) {
        res += pVect1[i] * pVect2[i];
    }
    return res;
}

#if defined(USE_AVX)
template<typename dist_t>
dist_t IPSqrSIMDAVX(const dist_t *pVect1, const dist_t *pVect2, const size_t dim);
#endif

#if defined(USE_AVX)
template<>
float IPSqrSIMDAVX<float>(const float *pVect1, const float *pVect2, const size_t dim) {
    float PORTABLE_ALIGN64 TmpRes[8];
    const float *pEnd1 = pVect1 + (dim >> 4 << 4);
    __m256 v1, v2;
    __m256 sum = _mm256_set1_ps(0);
    while (pVect1 < pEnd1) {
        v1 = _mm256_loadu_ps(pVect1);
        pVect1 += 8;
        v2 = _mm256_loadu_ps(pVect2);
        pVect2 += 8;
        sum = _mm256_add_ps(sum, _mm256_mul_ps(v1, v2));

        v1 = _mm256_loadu_ps(pVect1);
        pVect1 += 8;
        v2 = _mm256_loadu_ps(pVect2);
        pVect2 += 8;
        sum = _mm256_add_ps(sum, _mm256_mul_ps(v1, v2));
    }
    _mm256_store_ps(TmpRes, sum);
    return TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3] + TmpRes[4] + TmpRes[5] + TmpRes[6] + TmpRes[7];
}

template<>
double IPSqrSIMDAVX<double>(const double *pVect1, const double *pVect2, const size_t dim) {
    double PORTABLE_ALIGN64 TmpRes[4];
    const double *pEnd1 = pVect1 + (dim >> 4 << 4);
    __m256d v1, v2;
    __m256d sum = _mm256_set1_pd(0);
    while (pVect1 < pEnd1) {
        v1 = _mm256_loadu_pd(pVect1);
        pVect1 += 4;
        v2 = _mm256_loadu_pd(pVect2);
        pVect2 += 4;
        sum = _mm256_add_pd(sum, _mm256_mul_pd(v1, v2));

        v1 = _mm256_loadu_pd(pVect1);
        pVect1 += 4;
        v2 = _mm256_loadu_pd(pVect2);
        pVect2 += 4;
        sum = _mm256_add_pd(sum, _mm256_mul_pd(v1, v2));
    }
    _mm256_store_pd(TmpRes, sum);
    return TmpRes[0] + TmpRes[1] + TmpRes[2] + TmpRes[3];
}
#endif

#if defined(USE_AVX)
template<typename dist_t>
static dist_t
IPSqrSIMD16ExtResiduals(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    size_t dim16 = dim >> 4 << 4;
    dist_t res = IPSqrSIMDAVX<dist_t>(pVect1, pVect2, dim16);
    const dist_t *pVect1_tail = pVect1 + dim16;
    const dist_t *pVect2_tail = pVect2 + dim16;
    size_t qty_left = dim - dim16;
    dist_t res_tail = InnerProduct<dist_t>(pVect1_tail, pVect2_tail, qty_left);
    return (res + res_tail);
}
#endif

template<typename dist_t>
inline static dist_t
InnerProductSIMD(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
#if defined(USE_AVX)
    if (dim % 16 == 0) {
        return IPSqrSIMDAVX<dist_t>(pVect1, pVect2, dim);
    } else {
        return IPSqrSIMD16ExtResiduals<dist_t>(pVect1, pVect2, dim);
    }
#else
    return InnerProduct<dist_t>(pVect1, pVect2, dim);
#endif
}

template<typename dist_t> 
inline static dist_t 
InnerProductDistance(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    return 1.0f - InnerProductSIMD<dist_t>(pVect1, pVect2, dim);
}

template<typename dist_t>
class InnerProductSpace : public SpaceInterface<dist_t> {
    DISTFUNC<dist_t> fstdistfunc_;
    size_t data_size_;
    size_t dim_;

 public:
    explicit InnerProductSpace(size_t dim) {
        fstdistfunc_ = InnerProductDistance<dist_t>;
        dim_ = dim;
        data_size_ = dim * sizeof(dist_t);
    }
    ~InnerProductSpace() override {}

    // Static inline distance function — callable by HierarchicalNSW<dist_t, InnerProductSpace<dist_t>>
    // without going through a function pointer, enabling compiler inlining.
    static inline dist_t distFunc(const dist_t* a, const dist_t* b, size_t dim) {
        return InnerProductDistance<dist_t>(a, b, dim);
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
