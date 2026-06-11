add_executable(qle_tests test_main.cpp)
target_link_libraries(qle_tests PRIVATE qle_core)
add_test(NAME QLETests COMMAND qle_tests)
