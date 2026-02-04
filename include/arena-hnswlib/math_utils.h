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

// Compute the squared L2 (Euclidean) distance between two vectors
// Returns sum of squared differences without computing square root
// This is sufficient for ranking purposes and avoids expensive sqrt operation
template<typename dist_t>
inline dist_t L2DistanceSquared(const dist_t* pVect1, const dist_t* pVect2, size_t dim) {
    dist_t res = 0;
    for (size_t i = 0; i < dim; i++) {
        dist_t diff = pVect1[i] - pVect2[i];
        res += diff * diff;
    }
    return res;
}


} // namespace arena_hnswlib
