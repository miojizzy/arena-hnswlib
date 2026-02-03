#include "gtest/gtest.h"
#include "arena-hnswlib/data_store.h"

using namespace arena_hnswlib;

TEST(DataStoreTest, SetAndGetData) {
    DataStore<float> store(3, 10);

    float data[3] = {1.0f, 2.0f, 3.0f};
    EXPECT_TRUE(store.setData(0, data));

    float* retrieved = store.getData(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_FLOAT_EQ(retrieved[0], 1.0f);
    EXPECT_FLOAT_EQ(retrieved[1], 2.0f);
    EXPECT_FLOAT_EQ(retrieved[2], 3.0f);
}

TEST(DataStoreTest, InvalidSetData) {
    DataStore<float> store(3, 10);

    float data[3] = {1.0f, 2.0f, 3.0f};
    EXPECT_FALSE(store.setData(10, data)); // Invalid ID
    EXPECT_FALSE(store.setData(0, nullptr)); // Null data
}

TEST(DataStoreAlignedTest, SetAndGetData) {
    DataStoreAligned<float, 32> store(3, 10);

    float data[3] = {1.0f, 2.0f, 3.0f};
    EXPECT_TRUE(store.setData(0, data));

    float* retrieved = store.getData(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_FLOAT_EQ(retrieved[0], 1.0f);
    EXPECT_FLOAT_EQ(retrieved[1], 2.0f);
    EXPECT_FLOAT_EQ(retrieved[2], 3.0f);
}

TEST(DataStoreAlignedTest, InvalidSetData) {
    DataStoreAligned<float, 32> store(3, 10);

    float data[3] = {1.0f, 2.0f, 3.0f};
    EXPECT_FALSE(store.setData(10, data)); // Invalid ID
    EXPECT_FALSE(store.setData(0, nullptr)); // Null data
}