#pragma once

#include "arena_hnswlib.h"
#include <cstdint>

namespace arena_hnswlib {


template<typename dist_t>
class BruteForceSearch : public AlgorithmInterface<dist_t> {
public:
    const size_t maxElements_;
    const size_t dim_;
    const size_t data_size_;

    void* data_;

    BruteForceSearch(SpacePtr<dist_t> s, size_t maxElements) 
        : AlgorithmInterface<dist_t>(std::move(s)), maxElements_(maxElements),
          dim_(this->space_->getDim()), data_size_(this->space_->getDataSize()) {
    }


    void addPoint(const void *data_point, LabelType label_type) override {
        if (label_type >= maxElements_) {
            throw std::runtime_error("LabelType exceeds maxElements");
        }
        std::memcpy(static_cast<char*>(data_) + label_type * data_size_, data_point, data_size_);
    }

};



} // namespace arena_hnswlib