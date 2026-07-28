set(SO_5_EXTRA_TEST_LAUNCHER ${CMAKE_COMMAND})
list(APPEND SO_5_EXTRA_TEST_LAUNCHER -E env)

set(TEST _unit.so_5_extra.shutdowner.time_elapsed)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib
	sobjectizer::so5extra doctest::doctest)
add_test(NAME ${TEST} COMMAND ${SO_5_EXTRA_TEST_LAUNCHER} ${TEST})
set_property(TEST ${TEST} PROPERTY WILL_FAIL TRUE)

set(TEST_S _unit.so_5_extra.shutdowner.time_elapsed_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib
	sobjectizer::so5extra doctest::doctest)
add_test(NAME ${TEST_S} COMMAND ${SO_5_EXTRA_TEST_LAUNCHER} ${TEST_S})
set_property(TEST ${TEST_S} PROPERTY WILL_FAIL TRUE)

