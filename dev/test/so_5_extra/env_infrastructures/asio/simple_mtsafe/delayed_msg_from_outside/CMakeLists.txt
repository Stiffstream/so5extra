set(TEST _unit.so_5_extra.env.asio.simple_mtsafe.delayed_msg_from_outside)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib
	sobjectizer::so5extra doctest::doctest asio)
add_test(NAME ${TEST} COMMAND ${TEST})

set(TEST_S _unit.so_5_extra.env.asio.simple_mtsafe.delayed_msg_from_outside_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib
	sobjectizer::so5extra doctest::doctest asio)
add_test(NAME ${TEST_S} COMMAND ${TEST})

