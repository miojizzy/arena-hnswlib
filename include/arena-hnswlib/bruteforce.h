#pragma once

#include "arena_hnswlib.h"
#include "data_store.h"
#include <cstdint>

namespace arena_hnswlib {


template<typename dist_t>
class BruteForceSearch : public AlgorithmInterface<dist_t> {
public:
    const size_t maxElements_;
    const size_t dim_;
    const size_t data_size_;
    const DISTFUNC<dist_t> dist_func_;


    size_t current_count_ = 0;
    DataStoreAligned<dist_t, 64> point_store_;
    DataStore<LabelType> label_store_;

    BruteForceSearch(SpacePtr<dist_t> s, size_t maxElements) 
        : AlgorithmInterface<dist_t>(std::move(s)), maxElements_(maxElements),
          dim_(this->space_->getDim()), data_size_(this->space_->getDataSize()),
          dist_func_(this->space_->getDistFunc()),
          point_store_(dim_, maxElements_), label_store_(1, maxElements_) {
    }

    void addPoint(const void *data_point, LabelType label_type) override {
        if (current_count_ >= maxElements_) {
            throw std::runtime_error("LabelType exceeds maxElements");
        }
        point_store_.setData(current_count_, static_cast<const dist_t*>(data_point));
        label_store_.setData(current_count_, &label_type);
        current_count_++;   
    }

    std::priority_queue<std::pair<dist_t, LabelType>>
    searchKnn(const void* query_data, size_t k) const override {
        std::priority_queue<std::pair<dist_t, LabelType>> result;
        for (size_t i = 0; i < current_count_; i++) {
            const dist_t* data_point = point_store_.getData(i);
            dist_t dist = dist_func_(query_data, data_point, &dim_);
            LabelType label = *(label_store_.getData(i));
            if (result.size() < k) {
                result.emplace(dist, label);
            } else if (dist < result.top().first) {
                result.pop();
                result.emplace(dist, label);
            }
        }
        return result;
    }

};



} // namespace arena_hnswlib