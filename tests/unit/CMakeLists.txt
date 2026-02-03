# Add unit tests for new files
add_executable(test_arena_hnswlib test_arena_hnswlib.cpp)
target_link_libraries(test_arena_hnswlib gtest gtest_main pthread)
add_test(NAME test_arena_hnswlib COMMAND test_arena_hnswlib)

add_executable(test_space_ip test_space_ip.cpp)
target_link_libraries(test_space_ip gtest gtest_main pthread)
add_test(NAME test_space_ip COMMAND test_space_ip)

add_executable(test_bruteforce test_bruteforce.cpp)
target_link_libraries(test_bruteforce gtest gtest_main pthread)
add_test(NAME test_bruteforce COMMAND test_bruteforce)

add_executable(test_data_store test_data_store.cpp)
target_link_libraries(test_data_store gtest gtest_main pthread)
add_test(NAME test_data_store COMMAND test_data_store)