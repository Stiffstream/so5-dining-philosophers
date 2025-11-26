cmake_minimum_required(VERSION 3.10)

# For MSVC_RUNTIME_LIBRARY.
cmake_policy(SET CMP0091 NEW)

# For case if there is no CMAKE_TOOLCHAIN_FILE, but VCPKG_HOME exists.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (DEFINED ENV{VCPKG_HOME})
        set(_vcpkg_env_var_name VCPKG_HOME)
    endif()

    if(_vcpkg_env_var_name)
        message(STATUS "*** vcpkg environment variable to use: ${_vcpkg_env_var_name}")
        set(_vcpkg_env_var_value $ENV{${_vcpkg_env_var_name}})
        message(STATUS "*** vcpkg to use: ${_vcpkg_env_var_value}")
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_env_var_value}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
        message(STATUS "*** CMAKE_TOOLCHAIN_FILE is set to: ${CMAKE_TOOLCHAIN_FILE}")
        unset(_vcpkg_env_var_name)
    else()
        message(FATAL_ERROR "No environment variable VCPKG_HOME found!")
    endif()
endif()

set(VCPKG_FEATURE_FLAGS versions)

project(so5_dining_philosophers LANGUAGES CXX)

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

find_package(sobjectizer CONFIG REQUIRED)
find_package(so5extra CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_subdirectory(dining_philosophers)

