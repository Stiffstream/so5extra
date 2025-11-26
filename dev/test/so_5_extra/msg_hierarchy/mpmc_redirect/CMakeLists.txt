set(TEST _unit.so_5_extra.msg_hierarchy.mpmc_redirect)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib)
add_test(NAME ${TEST} COMMAND ${TEST})

set(TEST_S _unit.so_5_extra.msg_hierarchy.mpmc_redirect_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib)
add_test(NAME ${TEST_S} COMMAND ${TEST})

