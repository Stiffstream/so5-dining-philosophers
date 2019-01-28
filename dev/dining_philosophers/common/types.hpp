#pragma once

#include <string>
#include <vector>

//
// names_holder_t
//
using names_holder_t = std::vector< std::string >;

//
// thinking_type_t
//
enum class thinking_type_t
{
	normal,
	hungry
};

//
// philosopher_done_t
//
struct philosopher_done_t
{
	std::size_t m_philosopher_index;
};

