// Copyright 2026 Arena HNSW Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>

namespace arena_hnswlib {

// Tag type for external memory constructor
struct ExternalMemoryTag {};
inline constexpr ExternalMemoryTag external_memory{};

// AlignedVector - A fixed-size array with aligned memory support
//
// This container provides a simple fixed-size array with guaranteed memory
// alignment. It supports two memory modes:
// 1. Owned mode: allocates and owns aligned memory
// 2. External mode: operates on externally provided memory (non-owning)
//
// Template parameters:
//   T - Element type (must be trivially copyable)
//   Alignment - Memory alignment in bytes (must be power of 2, default 32)
template<typename T, size_t Alignment = 32>
class AlignedVector {
    static_assert(std::is_trivially_copyable<T>::value,
        "AlignedVector requires trivially copyable types");
    static_assert(Alignment > 0 && (Alignment & (Alignment - 1)) == 0,
        "Alignment must be a power of two");
    static_assert(Alignment >= alignof(T),
        "Alignment must be at least as strict as T's alignment");

 public:
    using size_type = size_t;
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;

 private:
    pointer data_ = nullptr;
    size_type size_ = 0;        // Element count

 private:
    static size_type calc_aligned_capacity(size_type count) {
        const size_type bytes = count * sizeof(T);
        const size_type aligned_bytes = ((bytes + Alignment - 1) / Alignment) * Alignment;
        return aligned_bytes / sizeof(T);
    }

    // Allocate aligned memory
    static pointer allocate_aligned(size_type count) {
        if (count == 0) return nullptr;
        void* ptr = std::aligned_alloc(Alignment, count * sizeof(T));
        if (!ptr) throw std::bad_alloc();
        return static_cast<pointer>(ptr);
    }

 public:
    explicit AlignedVector(size_type count)
        : size_(count), data_(allocate_aligned(calc_aligned_capacity(count))) {}

    // Move constructor
    AlignedVector(AlignedVector&& other) noexcept
        : size_(other.size_), data_(other.data_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // Move assignment
    AlignedVector& operator=(AlignedVector&& other) noexcept {
        if (this != &other) {
            if (data_) std::free(data_);
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // Copy is deleted
    AlignedVector(const AlignedVector&) = delete;
    AlignedVector& operator=(const AlignedVector&) = delete;

    // Destructor
    ~AlignedVector() {
        if (data_) std::free(data_);
    }

    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    const size_type size() const noexcept{ return size_; }
    const size_type capacity() const noexcept { return calc_aligned_capacity(size_); }
    const size_type alignment() const noexcept { return Alignment; }
};

template<typename T, size_t Alignment = 32>
class AlignedVectorView {
    static_assert(std::is_trivially_copyable<T>::value,
        "AlignedVectorView requires trivially copyable types");
    static_assert(Alignment > 0 && (Alignment & (Alignment - 1)) == 0,
        "Alignment must be a power of two");
    static_assert(Alignment >= alignof(T),
        "Alignment must be at least as strict as T's alignment");

 public:
    using size_type = size_t;
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;

 private:
    pointer data_ = nullptr;
    size_type size_ = 0;        // Element count

 private:
    static size_type calc_aligned_capacity(size_type count) {
        const size_type bytes = count * sizeof(T);
        const size_type aligned_bytes = ((bytes + Alignment - 1) / Alignment) * Alignment;
        return aligned_bytes / sizeof(T);
    }

 public:
    AlignedVectorView(pointer ptr, size_type count) : data_(ptr), size_(count) {}

    // Default copy and assignment (View is reference semantics)
    AlignedVectorView(const AlignedVectorView&) = default;
    AlignedVectorView& operator=(const AlignedVectorView&) = default;

    // Destructor (no-op, does not own memory)
    ~AlignedVectorView() = default;

    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    size_type size() const noexcept { return size_; }
    size_type capacity() const noexcept { return calc_aligned_capacity(size_); }
    static constexpr size_type alignment() noexcept { return Alignment; }
};

}  // namespace arena_hnswlib
