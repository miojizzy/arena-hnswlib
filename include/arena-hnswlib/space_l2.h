#pragma once

#include "arena_hnswlib.h"
#include "math_utils.h"

namespace arena_hnswlib {

// Squared L2 (Euclidean) distance function
// Returns sum of squared differences between two vectors
template<typename dist_t> 
static dist_t 
L2Squared(const void *pVect1, const void *pVect2, const void *qty_ptr) {
    size_t qty = *static_cast<const size_t *>(qty_ptr);
    return L2DistanceSquared(
        static_cast<const dist_t *>(pVect1),
        static_cast<const dist_t *>(pVect2),
        qty
    );
}

// L2 Space class using squared Euclidean distance metric
// Note: This uses squared L2 distance (without square root) for efficiency
// The ranking is preserved since sqrt is a monotonic function
// score = x1
template<typename dist_t>
class L2Space : public SpaceInterface<dist_t> {
    DISTFUNC<dist_t> fstdistfunc_;
    size_t data_size_;
    size_t dim_;

public:
    L2Space(size_t dim) {
        fstdistfunc_ = L2Squared<dist_t>;
        dim_ = dim;
        data_size_ = dim * sizeof(dist_t);
    }
    ~L2Space() override {}

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
