set(TEST _unit.so_5_extra.mboxes.composite.mbox_type)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib
	sobjectizer::so5extra doctest::doctest)
add_test(NAME ${TEST} COMMAND ${TEST})

set(TEST_S _unit.so_5_extra.mboxes.composite.mbox_type_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib
	sobjectizer::so5extra doctest::doctest)
add_test(NAME ${TEST_S} COMMAND ${TEST})

