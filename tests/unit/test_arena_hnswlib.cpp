#include "gtest/gtest.h"
#include "arena-hnswlib/arena_hnswlib.h"
#include <memory>

using namespace arena_hnswlib;

// Mock class for SpaceInterface

template<typename dist_t>
class MockSpace : public SpaceInterface<dist_t> {
    size_t dim_ = 128;
    size_t data_size_ = 128 * sizeof(dist_t);
    DISTFUNC<dist_t> dist_func_ = nullptr;
public:
    const size_t& getDim() const override { return dim_; }
    const size_t& getDataSize() const override { return data_size_; }
    const DISTFUNC<dist_t>& getDistFunc() const override { return dist_func_; }
};

// Mock class for AlgorithmInterface
template<typename dist_t>
class MockAlgorithm : public AlgorithmInterface<dist_t> {
public:
    MockAlgorithm() : AlgorithmInterface<dist_t>() {}

    void addPoint(const void *data_point, LabelType label_type) override {
        // Mock implementation
    }

    std::priority_queue<std::pair<dist_t, LabelType>>
    searchKnn(const void*, size_t) const override {
        return {};
    }
};

TEST(SpaceInterfaceTest, BasicFunctionality) {
    MockSpace<float> space;
    EXPECT_EQ(space.getDim(), 128);
    EXPECT_EQ(space.getDataSize(), 128 * sizeof(float));
    EXPECT_EQ(space.getDistFunc(), nullptr);
}

TEST(AlgorithmInterfaceTest, BasicFunctionality) {
    MockAlgorithm<float> algorithm;
    // Add more tests for addPoint and searchKnn as needed
}