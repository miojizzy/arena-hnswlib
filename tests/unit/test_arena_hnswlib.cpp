#include "gtest/gtest.h"
#include "arena-hnswlib/arena_hnswlib.h"
#include <memory>

using namespace arena_hnswlib;

// Mock class for SpaceInterface
template<typename dist_t>
class MockSpace : public SpaceInterface<dist_t> {
public:
    size_t getDim() override { return 128; }
    size_t getDataSize() override { return 128 * sizeof(dist_t); }
    DISTFUNC<dist_t> getDistFunc() override { return nullptr; }
};

// Mock class for AlgorithmInterface
template<typename dist_t>
class MockAlgorithm : public AlgorithmInterface<dist_t> {
public:
    MockAlgorithm(SpacePtr<dist_t> s) : AlgorithmInterface<dist_t>(std::move(s)) {}

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
    auto space = std::make_unique<MockSpace<float>>();
    MockAlgorithm<float> algorithm(std::move(space));
    // Add more tests for addPoint and searchKnn as needed
}