set(TEST _unit.asio.deadlock_on_exit)
add_executable(${TEST} main.cpp)
target_link_libraries(${TEST} asio)
add_test(NAME ${TEST} COMMAND ${TEST})
