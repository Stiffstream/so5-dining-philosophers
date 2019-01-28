#pragma once

#include <dining_philosophers/common/types.hpp>

#include <random>
#include <chrono>

class random_pause_generator_t
{
public :
	random_pause_generator_t()
	{
		m_random_engine.seed( reinterpret_cast<std::size_t>(this) % 1000 );
	}

	auto think_pause( thinking_type_t type )
	{
		return std::chrono::milliseconds(
				random( 10, thinking_type_t::normal == type ? 60 : 30 ) );
	}

	auto eat_pause()
	{
		return std::chrono::milliseconds( random( 20, 80 ) );
	}

	static constexpr auto trace_step() {
		return std::chrono::milliseconds( 5 );
	}

private :
	// Engine for random values generation.
	std::mt19937 m_random_engine;

	int
	random( int l, int h )
	{
		return std::uniform_int_distribution< int >( l, h )( m_random_engine );
	}
};

