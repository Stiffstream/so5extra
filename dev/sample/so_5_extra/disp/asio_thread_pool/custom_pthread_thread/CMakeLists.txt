set(SAMPLE sample.so_5_extra.disp.asio_thread_pool.custom_pthread_thread)
add_executable(${SAMPLE} main.cpp)
target_link_libraries(${SAMPLE} sobjectizer::SharedLib
	sobjectizer::so5extra asio)
install(TARGETS ${SAMPLE} DESTINATION bin)

set(SAMPLE_S sample.so_5_extra.disp.asio_thread_pool.custom_pthread_thread_s)
add_executable(${SAMPLE_S} main.cpp)
target_link_libraries(${SAMPLE_S} sobjectizer::StaticLib
	sobjectizer::so5extra asio)
install(TARGETS ${SAMPLE_S} DESTINATION bin)

