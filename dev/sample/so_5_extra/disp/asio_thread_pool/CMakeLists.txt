add_subdirectory(hello_world)

if(NOT WIN32)
	add_subdirectory(custom_pthread_thread)
endif()
