add_executable(qle_tests test_main.cpp)
target_link_libraries(qle_tests PRIVATE qle_core)
add_test(NAME QLETests COMMAND qle_tests)

add_executable(test_json_crash test_json_crash.cpp)
target_link_libraries(test_json_crash PRIVATE qle_core)
add_test(NAME JsonCrash COMMAND test_json_crash)

add_executable(test_loop test_loop.cpp)
target_link_libraries(test_loop PRIVATE qle_core)
add_test(NAME Loop COMMAND test_loop)

add_executable(test_aggressive test_aggressive.cpp)
target_link_libraries(test_aggressive PRIVATE qle_core)
add_test(NAME Aggressive COMMAND test_aggressive)
