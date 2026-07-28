add_subdirectory(ping_pong)
add_subdirectory(resolve_n)

if(NOT WIN32)
	add_subdirectory(custom_pthread)
endif()
