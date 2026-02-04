
#pragma once 
#include "arena_hnswlib.h"
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



template<typename T, size_t N>
class DataStoreAligned : public DataStoreInterface<T> {
    static_assert(N > 0 && (N & (N - 1)) == 0, 
        "Alignment N must be a power of two and greater than zero");
    static_assert(N == 32 || N == 64, 
        "Alignment N must be either 32 or 64");

    const size_t dataLen_;
    const size_t maxElements_;
    std::unique_ptr<T, decltype(&std::free)> data_; // Aligned memory managed by unique_ptr

public:
    DataStoreAligned(size_t dataLen, size_t maxElements)
        : dataLen_(dataLen), maxElements_(maxElements),
          data_(static_cast<T*>(std::aligned_alloc(N, dataLen_ * maxElements_ * sizeof(T))), &std::free) {
        if (!data_) {
            throw std::bad_alloc(); // Throw exception if memory allocation fails
        }
    }

    ~DataStoreAligned() = default; // unique_ptr handles memory cleanup

    bool setData(InternalId id, const T* data) override {
        if (!data || id >= maxElements_) {
            return false; // Check for null pointer and valid ID
        }
        std::memcpy(&data_.get()[id * dataLen_], data, dataLen_ * sizeof(T));
        return true;
    }

    T* getData(InternalId id) override {
        if (id >= maxElements_) {
            return nullptr; // Return nullptr for invalid ID
        }
        return &data_.get()[id * dataLen_];
    }

    const T* getData(InternalId id) const {
        if (id >= maxElements_) {
            return nullptr; // Return nullptr for invalid ID
        }
        return &data_.get()[id * dataLen_];
    }
};

} // namespace arena_hnswlib