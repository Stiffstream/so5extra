cmake_minimum_required(VERSION 3.8)

include(cmake/target.cmake)

project(so5extra VERSION ${SO5EXTRA_VERSION} LANGUAGES CXX)

add_library(so5extra INTERFACE)
add_library(sobjectizer::so5extra ALIAS so5extra)

IF (${CMAKE_VERSION} VERSION_LESS "3.8.0")
	set_target_properties(so5extra PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
	)
ELSE ()
	target_compile_features(so5extra INTERFACE cxx_std_17)
ENDIF ()

get_filename_component(CURRENT_FILE_DIR ${CMAKE_CURRENT_LIST_FILE} DIRECTORY)
get_filename_component(CURRENT_FILE_DIR ${CURRENT_FILE_DIR} DIRECTORY)
SET(SO5EXTRA_INCLUDE_PATH ${CURRENT_FILE_DIR})
UNSET(CURRENT_FILE_DIR)

# Propagate include directories for so_5_extra.
TARGET_INCLUDE_DIRECTORIES(so5extra
	INTERFACE
		$<BUILD_INTERFACE:${SO5EXTRA_INCLUDE_PATH}>
		$<INSTALL_INTERFACE:include>
	)

IF (SO5EXTRA_INSTALL)
	include(GNUInstallDirs)
	include(CMakePackageConfigHelpers)

	SET(SO5EXTRA_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/so5extra CACHE STRING
		"Installation directory for cmake files, relative to ${CMAKE_INSTALL_PREFIX}.")
	SET(SO5EXTRA_VERSION_CONFIG ${PROJECT_BINARY_DIR}/so5extra-config-version.cmake)
	SET(SO5EXTRA_PROJECT_CONFIG ${PROJECT_BINARY_DIR}/so5extra-config.cmake)

	SET(SO5EXTRA_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR}/so_5_extra/ CACHE STRING
		"Installation directory for include files, relative to ${CMAKE_INSTALL_PREFIX}.")

	# Generate the version, config and target files into the build directory.
	write_basic_package_version_file(
		${SO5EXTRA_VERSION_CONFIG}
		VERSION ${SO5EXTRA_VERSION}
		COMPATIBILITY ExactVersion )

	configure_package_config_file(
		"cmake/so5extra-config.cmake.in"
		${SO5EXTRA_PROJECT_CONFIG}
		INSTALL_DESTINATION ${SO5EXTRA_CMAKE_DIR}
		PATH_VARS SO5EXTRA_VERSION)

	# Install version, config and target files.
	install(
		FILES ${SO5EXTRA_PROJECT_CONFIG} ${SO5EXTRA_VERSION_CONFIG}
		DESTINATION ${SO5EXTRA_CMAKE_DIR})

	SET(SO5EXTRA_INSTALL_TARGETS so5extra)

	install(
		TARGETS ${SO5EXTRA_INSTALL_TARGETS}
		EXPORT SO5EXTRA_ALL_TARGETS
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		RUNTIME DESTINATION bin
	)

	install(
		EXPORT SO5EXTRA_ALL_TARGETS
		NAMESPACE sobjectizer::
		FILE so5extra-targets.cmake
		DESTINATION ${SO5EXTRA_CMAKE_DIR} )

	file(GLOB_RECURSE ALL_SO5EXTRA_HEADERS RELATIVE ${CMAKE_CURRENT_LIST_DIR} *.hpp)
	FOREACH (HEADER_FILE ${ALL_SO5EXTRA_HEADERS} )
		get_filename_component( DIR ${HEADER_FILE} PATH )
		install( FILES ${HEADER_FILE} DESTINATION ${SO5EXTRA_INC_DIR}/${DIR} )
	ENDFOREACH ()

ENDIF ()

