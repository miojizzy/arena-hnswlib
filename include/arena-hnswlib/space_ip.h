#pragma once

#include "arena_hnswlib.h"

namespace arena_hnswlib {

template<typename dist_t> 
inline static dist_t 
InnerProduct(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    dist_t res = 0;
    for (size_t i = 0; i < dim; i++) {
        res += pVect1[i] * pVect2[i];
    }
    return res;
}

template<typename dist_t> 
inline static dist_t 
InnerProductDistance(const dist_t *pVect1, const dist_t *pVect2, const size_t dim) {
    return 1.0f - InnerProduct<dist_t>(pVect1, pVect2, dim);
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
