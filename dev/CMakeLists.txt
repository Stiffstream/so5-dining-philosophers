cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

set(SOBJECTIZER_BUILD_SHARED OFF)
set(SOBJECTIZER_BUILD_STATIC ON)

add_subdirectory(so_5)
add_subdirectory(so_5_extra)
add_subdirectory(fmt)

add_subdirectory(dining_philosophers)
