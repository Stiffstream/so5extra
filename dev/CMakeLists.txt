cmake_minimum_required(VERSION 3.14)

project(so5extra_master LANGUAGES CXX)

set(SO5EXTRA_MASTER_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(SO5EXTRA_MASTER_PROJECT ON)
endif()

option(BUILD_EXAMPLES "Enable building examples [default: ON]" ON)
option(BUILD_TESTS    "Enable building tests    [default: ON]" ON)

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

include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR}/asio/include)

add_subdirectory(so_5)
add_subdirectory(so_5_extra)

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(test/so_5_extra)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(sample/so_5_extra)
endif()

