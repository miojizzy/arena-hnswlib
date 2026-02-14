#include "gtest/gtest.h"
#include "arena-hnswlib/aligned_vector.h"

using namespace arena_hnswlib;

// ==================== AlignedVector Tests ====================

// Test constructor with size
TEST(AlignedVectorTest, SizeConstructor) {
    AlignedVector<float> vec(10);
    EXPECT_EQ(vec.size(), 10);
    EXPECT_GE(vec.capacity(), 10);
    EXPECT_NE(vec.data(), nullptr);
}

// Test memory alignment (default 32 bytes)
TEST(AlignedVectorTest, MemoryAlignment32) {
    AlignedVector<float, 32> vec(100);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(vec.data()) % 32, 0);
}

// Test memory alignment (64 bytes)
TEST(AlignedVectorTest, MemoryAlignment64) {
    AlignedVector<float, 64> vec(100);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(vec.data()) % 64, 0);
}

// Test aligned capacity calculation
TEST(AlignedVectorTest, AlignedCapacity) {
    // 32-byte alignment, float (4 bytes)
    // 10 floats = 40 bytes -> aligned to 64 bytes = 16 floats capacity
    AlignedVector<float, 32> vec1(10);
    EXPECT_GE(vec1.capacity() * sizeof(float), 10 * sizeof(float));
    EXPECT_EQ(vec1.capacity() * sizeof(float) % 32, 0);
    
    // 64-byte alignment
    AlignedVector<float, 64> vec2(10);
    EXPECT_GE(vec2.capacity() * sizeof(float), 10 * sizeof(float));
    EXPECT_EQ(vec2.capacity() * sizeof(float) % 64, 0);
}

// Test data access
TEST(AlignedVectorTest, DataAccess) {
    AlignedVector<float> vec(10);
    float* ptr = vec.data();
    ASSERT_NE(ptr, nullptr);
    
    // Write through pointer
    for (size_t i = 0; i < vec.size(); ++i) {
        ptr[i] = static_cast<float>(i);
    }
    
    // Verify
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_FLOAT_EQ(ptr[i], static_cast<float>(i));
    }
}

// Test move constructor
TEST(AlignedVectorTest, MoveConstructor) {
    AlignedVector<int> vec1(10);
    auto* old_data = vec1.data();
    
    AlignedVector<int> vec2(std::move(vec1));
    
    EXPECT_EQ(vec2.size(), 10);
    EXPECT_EQ(vec2.data(), old_data);
    EXPECT_EQ(vec1.size(), 0);
    EXPECT_EQ(vec1.data(), nullptr);
}

// Test move assignment
TEST(AlignedVectorTest, MoveAssignment) {
    AlignedVector<int> vec1(10);
    AlignedVector<int> vec2(5);
    
    auto* old_data = vec1.data();
    vec2 = std::move(vec1);
    
    EXPECT_EQ(vec2.size(), 10);
    EXPECT_EQ(vec2.data(), old_data);
    EXPECT_EQ(vec1.size(), 0);
    EXPECT_EQ(vec1.data(), nullptr);
}

// Test alignment method
TEST(AlignedVectorTest, AlignmentMethod) {
    AlignedVector<float, 32> vec1(10);
    EXPECT_EQ(vec1.alignment(), 32);
    
    AlignedVector<float, 64> vec2(10);
    EXPECT_EQ(vec2.alignment(), 64);
}

// Test different types
TEST(AlignedVectorTest, DifferentTypes) {
    // int32_t
    AlignedVector<int32_t> vec_int(10);
    EXPECT_EQ(vec_int.size(), 10);
    
    // double
    AlignedVector<double> vec_double(10);
    EXPECT_EQ(vec_double.size(), 10);
    
    // uint8_t
    AlignedVector<uint8_t> vec_byte(10);
    EXPECT_EQ(vec_byte.size(), 10);
}

// ==================== AlignedVectorView Tests ====================

// Test basic constructor
TEST(AlignedVectorViewTest, BasicConstructor) {
    float data[10];
    AlignedVectorView<float> view(data, 10);
    
    EXPECT_EQ(view.size(), 10);
    EXPECT_EQ(view.data(), data);
}

// Test copy constructor
TEST(AlignedVectorViewTest, CopyConstructor) {
    float data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    AlignedVectorView<float> view1(data, 10);
    
    AlignedVectorView<float> view2(view1);
    EXPECT_EQ(view2.size(), 10);
    EXPECT_EQ(view2.data(), data);
}

// Test copy assignment
TEST(AlignedVectorViewTest, CopyAssignment) {
    float data1[10];
    float data2[5];
    
    AlignedVectorView<float> view1(data1, 10);
    AlignedVectorView<float> view2(data2, 5);
    
    view2 = view1;
    EXPECT_EQ(view2.size(), 10);
    EXPECT_EQ(view2.data(), data1);
}

// Test data access through view
TEST(AlignedVectorViewTest, DataAccess) {
    float data[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    AlignedVectorView<float> view(data, 5);
    
    float* ptr = view.data();
    for (size_t i = 0; i < view.size(); ++i) {
        EXPECT_FLOAT_EQ(ptr[i], data[i]);
    }
    
    // Modify through view
    ptr[0] = 100.0f;
    EXPECT_FLOAT_EQ(data[0], 100.0f);
}

// Test alignment
TEST(AlignedVectorViewTest, Alignment) {
    alignas(32) float data[10];
    AlignedVectorView<float, 32> view(data, 10);
    EXPECT_EQ(view.alignment(), 32);
}

// Test capacity calculation
TEST(AlignedVectorViewTest, Capacity) {
    float data[10];
    AlignedVectorView<float, 32> view(data, 10);
    EXPECT_GE(view.capacity(), 10);
    EXPECT_EQ(view.capacity() * sizeof(float) % 32, 0);
}
