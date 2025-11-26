set(TEST _unit.so_5_extra.msg_hierarchy.mpsc_root_only)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib)
add_test(NAME ${TEST} COMMAND ${TEST})

set(TEST_S _unit.so_5_extra.msg_hierarchy.mpsc_root_only_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib)
add_test(NAME ${TEST_S} COMMAND ${TEST})

