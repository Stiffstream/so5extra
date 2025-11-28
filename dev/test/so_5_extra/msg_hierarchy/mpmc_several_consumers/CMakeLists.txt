set(TEST_SOURCES main.cpp receivers.cpp sender.cpp stopper.cpp)

set(TEST _unit.so_5_extra.msg_hierarchy.mpmc_several_consumers)
add_executable(${TEST} ${TEST_SOURCES})
target_link_libraries(${TEST} sobjectizer::SharedLib
	sobjectizer::so5extra doctest::doctest)
add_test(NAME ${TEST} COMMAND ${TEST})

set(TEST_S _unit.so_5_extra.msg_hierarchy.mpmc_several_consumers_s)
add_executable(${TEST_S} ${TEST_SOURCES})
target_link_libraries(${TEST_S} sobjectizer::StaticLib
	sobjectizer::so5extra doctest::doctest)
add_test(NAME ${TEST_S} COMMAND ${TEST})

