set(TEST _unit.so_5_extra.env.asio.simple_not_mtsafe.completion_on_stop)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib
	sobjectizer::so5extra doctest::doctest asio)
add_test(NAME ${TEST} COMMAND ${TEST})

set(TEST_S _unit.so_5_extra.env.asio.simple_not_mtsafe.completion_on_stop_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib
	sobjectizer::so5extra doctest::doctest asio)
add_test(NAME ${TEST_S} COMMAND ${TEST})

