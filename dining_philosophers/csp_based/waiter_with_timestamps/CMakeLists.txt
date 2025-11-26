cmake_minimum_required(VERSION 3.10)

set(PRJ csp_waiter_with_timestamps)

project(${PRJ})

add_executable(${PRJ} main.cpp)
target_link_libraries(${PRJ} sobjectizer::StaticLib)
target_link_libraries(${PRJ} sobjectizer::so5extra)
target_link_libraries(${PRJ} fmt::fmt-header-only)
target_link_libraries(${PRJ} csp_trace_maker)

install(
	TARGETS ${PRJ}
	RUNTIME DESTINATION bin
)

