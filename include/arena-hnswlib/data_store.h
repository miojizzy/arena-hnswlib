
#pragma once 
#include "arena_hnswlib.h"
#include "aligned_vector.h"
#include <cstddef>
#include <memory>
#include <cstring>

namespace arena_hnswlib {


template<typename T>
class DataStoreInterface {
 public:
    virtual ~DataStoreInterface() {};

    virtual bool setData(InternalId id, const T *data) = 0;

    virtual T* getData(InternalId id) = 0;
};


template<typename T>
class DataStore : public DataStoreInterface<T> {
    const size_t dataLen_;
    const size_t maxElements_;
    std::unique_ptr<T[]> data_; // Use smart pointer for automatic memory management

 public:
    DataStore(size_t dataLen, size_t maxElements) 
        : dataLen_(dataLen), maxElements_(maxElements), 
        data_(std::make_unique<T[]>(dataLen_ * maxElements_)) {
        if (!data_) {
            throw std::bad_alloc(); // Throw exception if memory allocation fails
        }
    }

    ~DataStore() = default; // Smart pointer handles memory cleanup

    bool setData(InternalId id, const T *data) override {
        if (!data || id >= maxElements_) {
            return false; // Check for null pointer and valid id
        }
        std::memcpy(&data_[id * dataLen_], data, dataLen_ * sizeof(T));
        return true;
    }

    T* getData(InternalId id) override {
        if (id >= maxElements_) {
            return nullptr;
        }
        return &data_[id * dataLen_];
    }

    const T* getData(InternalId id) const {
        if (id >= maxElements_) {
            return nullptr;
        }
        return &data_[id * dataLen_];
    }
};



template<typename T, size_t Alignment = 64>
class DataStoreAligned : public DataStoreInterface<T> {
    static_assert(Alignment > 0 && (Alignment & (Alignment - 1)) == 0, 
        "Alignment must be a power of two and greater than zero");
    static_assert(Alignment == 32 || Alignment == 64, 
        "Alignment must be either 32 or 64");

    AlignedVector<T, Alignment> data_;
    const size_t dataLen_;
    const size_t maxElements_;

 public:
    DataStoreAligned(size_t dataLen, size_t maxElements)
        : data_(dataLen * maxElements), dataLen_(dataLen), maxElements_(maxElements) {}

    ~DataStoreAligned() = default;

    bool setData(InternalId id, const T* data) override {
        if (!data || id >= maxElements_) {
            return false;
        }
        std::memcpy(data_.data() + id * dataLen_, data, dataLen_ * sizeof(T));
        return true;
    }

    T* getData(InternalId id) override {
        if (id >= maxElements_) {
            return nullptr;
        }
        return data_.data() + id * dataLen_;
    }

    const T* getData(InternalId id) const {
        if (id >= maxElements_) {
            return nullptr;
        }
        return data_.data() + id * dataLen_;
    }

    // Get a view of the element at the given id
    AlignedVectorView<T, Alignment> getView(InternalId id) {
        if (id >= maxElements_) {
            return AlignedVectorView<T, Alignment>(nullptr, 0);
        }
        return AlignedVectorView<T, Alignment>(data_.data() + id * dataLen_, dataLen_);
    }

    const AlignedVectorView<T, Alignment> getView(InternalId id) const {
        if (id >= maxElements_) {
            return AlignedVectorView<T, Alignment>(nullptr, 0);
        }
        return AlignedVectorView<T, Alignment>(data_.data() + id * dataLen_, dataLen_);
    }

    // Access to underlying aligned vector
    AlignedVector<T, Alignment>& getVector() { return data_; }
    const AlignedVector<T, Alignment>& getVector() const { return data_; }
};

} // namespace arena_hnswlib
