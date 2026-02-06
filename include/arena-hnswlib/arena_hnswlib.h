#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <type_traits>

#ifdef __AVX__
    #define USE_AVX
    #include <x86intrin.h>
#endif

#if defined(__GNUC__)
    #define PORTABLE_ALIGN32 __attribute__((aligned(32)))
    #define PORTABLE_ALIGN64 __attribute__((aligned(64)))
#else
    #define PORTABLE_ALIGN32 __declspec(align(32))
    #define PORTABLE_ALIGN64 __declspec(align(64))
#endif

namespace arena_hnswlib {

using LabelType = uint32_t;
using InternalId = uint32_t;
using LevelId = uint32_t;

template<typename dist_t>
using DISTFUNC = dist_t(*)(const dist_t*, const dist_t*, const size_t);

template<typename dist_t>
class SpaceInterface {
    static_assert(std::is_same<dist_t, float>::value || std::is_same<dist_t, double>::value,
                "dist_t must be either float or double");
public:
    virtual ~SpaceInterface() {}

    virtual const size_t& getDim() const = 0;

    virtual const size_t& getDataSize() const = 0;

    virtual const DISTFUNC<dist_t>& getDistFunc() const = 0;

};

template<typename dist_t>
using SpacePtr = std::unique_ptr<SpaceInterface<dist_t>>;


template<typename dist_t>
class AlgorithmInterface {
public:
    AlgorithmInterface(SpacePtr<dist_t> s) : space_(std::move(s)) {}

    virtual void addPoint(const void *data_point, LabelType label_type) = 0;

    virtual std::priority_queue<std::pair<dist_t, LabelType>>
        searchKnn(const void*, size_t) const = 0;

    virtual ~AlgorithmInterface() {}

protected:
    SpacePtr<dist_t> space_;
};




} // namespace arena_hnswlib
