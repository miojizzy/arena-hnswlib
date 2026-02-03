#pragma once

#include "arena_hnswlib.h"

namespace arena_hnswlib {

// Changed to static for internal linkage

template<typename dist_t> 
static dist_t 
InnerProduct(const void *pVect1, const void *pVect2, const void *qty_ptr) {
    size_t qty = *static_cast<const size_t *>(qty_ptr);
    dist_t res = 0;
    for (size_t i = 0; i < qty; i++) {
        res += static_cast<const dist_t *>(pVect1)[i] * static_cast<const dist_t *>(pVect2)[i];
    }
    return res;
}

template<typename dist_t> 
static dist_t 
InnerProductDistance(const void *pVect1, const void *pVect2, const void *qty_ptr) {
    return 1.0f - InnerProduct<dist_t>(pVect1, pVect2, qty_ptr);
}


template<typename dist_t>
class InnerProductSpace : public SpaceInterface<dist_t> {
    DISTFUNC<dist_t> fstdistfunc_;
    size_t data_size_;
    size_t dim_;

public:
    InnerProductSpace(size_t dim) {
        fstdistfunc_ = InnerProductDistance<dist_t>;
        dim_ = dim;
        data_size_ = dim * sizeof(dist_t);
    }
    ~InnerProductSpace() override {}

    DISTFUNC<dist_t> getDistFunc() override {
        return fstdistfunc_;
    }

    size_t getDim() override {
        return dim_;
    }

    size_t getDataSize() override {
        return data_size_;
    }
};

} // namespace arena_hnswlib