set(TEST _unit.so_5_extra.env.asio.simple_not_mtsafe.stress_timers)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} sobjectizer::SharedLib)

set(TEST_S _unit.so_5_extra.env.asio.simple_not_mtsafe.stress_timers_s)
add_executable(${TEST_S} main.cpp)
target_link_libraries(${TEST_S} sobjectizer::StaticLib)

