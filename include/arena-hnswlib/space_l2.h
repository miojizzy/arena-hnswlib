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

// Squared L2 (Euclidean) distance function
// Returns sum of squared differences between two vectors
template<typename dist_t> 
inline static dist_t 
L2SquaredDistance(const void *pVect1, const void *pVect2, const void *qty_ptr) {
    const auto& l2_squared_func = L2Squared<dist_t>;
    return l2_squared_func(
        static_cast<const dist_t *>(pVect1),
        static_cast<const dist_t *>(pVect2),
        *static_cast<const size_t *>(qty_ptr)
    );
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
    L2Space(size_t dim) {
        fstdistfunc_ = L2SquaredDistance<dist_t>;
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
