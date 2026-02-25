# Add unit tests for new files

add_executable(test_arena_hnswlib test_arena_hnswlib.cpp)
target_link_libraries(test_arena_hnswlib arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_arena_hnswlib COMMAND test_arena_hnswlib)


add_executable(test_space_ip test_space_ip.cpp)
target_link_libraries(test_space_ip arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_space_ip COMMAND test_space_ip)


add_executable(test_space_l2 test_space_l2.cpp)
target_link_libraries(test_space_l2 arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_space_l2 COMMAND test_space_l2)


add_executable(test_bruteforce test_bruteforce.cpp)
target_link_libraries(test_bruteforce arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_bruteforce COMMAND test_bruteforce)


add_executable(test_data_store test_data_store.cpp)
target_link_libraries(test_data_store arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_data_store COMMAND test_data_store)


# HNSWALG 单元测试
add_executable(test_hnswalg test_hnswalg.cpp)
target_link_libraries(test_hnswalg arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_hnswalg COMMAND test_hnswalg)

# math_utils 单元测试
add_executable(test_math_utils test_math_utils.cpp)
target_link_libraries(test_math_utils arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_math_utils COMMAND test_math_utils)

# aligned_vector 单元测试
add_executable(test_aligned_vector test_aligned_vector.cpp)
target_link_libraries(test_aligned_vector arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_aligned_vector COMMAND test_aligned_vector)

# 多核并发测试
add_executable(test_concurrent test_concurrent.cpp)
target_link_libraries(test_concurrent arena-hnswlib gtest gtest_main pthread)
add_test(NAME test_concurrent COMMAND test_concurrent)