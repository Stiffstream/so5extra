cmake_minimum_required(VERSION 3.14)

project(so5extra_master LANGUAGES CXX)

set(SO5EXTRA_MASTER_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(SO5EXTRA_MASTER_PROJECT ON)
endif()

option(SO5EXTRA_BUILD_EXAMPLES "Enable building examples [default: ON]" ON)
option(SO5EXTRA_BUILD_TESTS    "Enable building tests    [default: ON]" ON)

option(SO5EXTRA_INSTALL "Generate install target for so5extra"
	${SO5EXTRA_MASTER_PROJECT})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_CXX_STANDARD)
	message(STATUS "C++17 will be used by default")
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CXX_EXTENSIONS NO)
else()
	message(STATUS "C++ standard is set to C++${CMAKE_CXX_STANDARD}")
endif()

if(SO5EXTRA_MASTER_PROJECT)
	include(FetchContent)

	set(FETCHCONTENT_QUIET OFF)

	set(SOBJECTIZER_BUILD_SHARED ON CACHE INTERNAL "")
	set(SOBJECTIZER_BUILD_STATIC ON CACHE INTERNAL "")
	set(SOBJECTIZER_INSTALL ON CACHE INTERNAL "")
	set(BUILD_ALL OFF CACHE INTERNAL "")
	set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
	set(BUILD_TESTS OFF CACHE INTERNAL "")

	FetchContent_Declare(sobjectizer
		GIT_REPOSITORY https://github.com/stiffstream/sobjectizer
		GIT_TAG e633be9bfd02c9088a531a39f55105702409285b
		SOURCE_SUBDIR dev
		OVERRIDE_FIND_PACKAGE
		LOG_DOWNLOAD TRUE
		LOG_CONFIGURE TRUE
		LOG_BUILD TRUE
		LOG_INSTALL TRUE
	)

	set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")

	FetchContent_Declare(doctest
		GIT_REPOSITORY https://github.com/doctest/doctest
		GIT_TAG v2.5.3
		OVERRIDE_FIND_PACKAGE
	)

	FetchContent_Declare(asio
		GIT_REPOSITORY https://github.com/chriskohlhoff/asio
		GIT_TAG asio-1-38-2
		OVERRIDE_FIND_PACKAGE
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
	)

	FetchContent_MakeAvailable(sobjectizer doctest asio)

	# Because Asio doesn't provide CMake file it's necessary to
	# define asio target by youself.
	add_library(asio INTERFACE)

	# For some reason there is no need to add /asio/include for Windows.
	# Maybe it's because symbolic links are used on Linux?
	if(WIN32)
		target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio)
	else()
		target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
	endif()

	find_package(Threads)
	target_link_libraries(asio INTERFACE Threads::Threads)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(
		-Wall
		-Wextra
		-Wpedantic
		-Wno-c++98-compat
		-Wno-c++98-compat-pedantic
		-Wno-padded
		-Wno-missing-noreturn
		-Wno-documentation-unknown-command
		-Wno-documentation-deprecated-sync
		-Wno-documentation
		-Wno-weak-vtables
		-Wno-missing-prototypes
		-Wno-missing-variable-declarations
		-Wno-exit-time-destructors
		-Wno-global-constructors
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(
		-Wextra
		-Wpedantic
		-Wall
		-Wno-unused-function
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(
		-W3
	)
	add_compile_definitions(
		_WIN32_WINNT=0x0A00
	)
endif()

# FIXME: it seems that those lines have to be removed.
#include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR})
#include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR}/asio/include)
#add_subdirectory(so_5)

add_subdirectory(so_5_extra)

if(SO5EXTRA_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test/so_5_extra)
endif()

if(SO5EXTRA_BUILD_EXAMPLES)
	add_subdirectory(sample/so_5_extra)
endif()

