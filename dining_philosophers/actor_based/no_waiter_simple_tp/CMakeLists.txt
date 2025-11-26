cmake_minimum_required(VERSION 3.10)

set(PRJ actors_no_waiter_simple_tp)

project(${PRJ})

add_executable(${PRJ} main.cpp)
target_link_libraries(${PRJ} sobjectizer::StaticLib)
target_link_libraries(${PRJ} fmt::fmt-header-only)
target_link_libraries(${PRJ} actors_trace_maker)

install(
	TARGETS ${PRJ}
	RUNTIME DESTINATION bin
)

