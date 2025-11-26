cmake_minimum_required(VERSION 3.10)

set(PRJ actors_trace_maker)

project(${PRJ})

add_library(${PRJ} all.cpp)

target_link_libraries(${PRJ} fmt::fmt-header-only)
target_link_libraries(${PRJ} sobjectizer::StaticLib)

target_include_directories(${PRJ}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
)
