#pragma once

#include <dining_philosophers/common/types.hpp>

#include <so_5/all.hpp>

#include <fmt/format.h>

#include <vector>
#include <string>
#include <chrono>
#include <algorithm>

namespace trace {

constexpr char st_normal_thinking = 't';
constexpr char st_hungry_thinking = '.';
constexpr char st_eating = 'E';
constexpr char st_wait_left = 'L';
constexpr char st_wait_right = 'R';
constexpr char st_done = 'q';

struct state_changed_t final : public so_5::message_t
{
	std::chrono::steady_clock::time_point m_when;
	const std::size_t m_index;
	const char m_state;

	state_changed_t( std::size_t index, char state )
		:	m_when{ std::chrono::steady_clock::now() }
		,	m_index{ index }
		,	m_state{ state }
	{}
};

struct history_item_t 
{
	std::chrono::steady_clock::time_point m_when;
	char m_state;

	history_item_t(
		std::chrono::steady_clock::time_point when,
		char state )
		:	m_when{ when }
		,	m_state{ state }
	{}
};

using history_t = std::vector< history_item_t >;
using trace_data_t = std::vector< history_t >;

inline trace_data_t make_trace_data( std::size_t philosopher_count )
{
	return { philosopher_count, history_t{} };
}

inline void show_trace_data(
	const names_holder_t & names,
	const trace_data_t & trace,
	std::chrono::steady_clock::duration step )
{
	const auto has_empty_history = [&] {
		return (trace.empty() ||
				trace.end() != std::find_if( trace.begin(), trace.end(),
						[]( const auto & h ) { return h.size() < 2; } ));
	};

	const auto min_max_times = [&] {
		auto left = trace.front().front().m_when;
		auto right = left;

		for( const auto & h : trace )
		{
			const auto minmax = std::minmax_element(
					h.begin(), h.end(),
					[]( const auto & a, const auto & b ) {
						return a.m_when < b.m_when;
					} );
			left = std::min( left, minmax.first->m_when );
			right = std::max( right, minmax.second->m_when );
		}

		return std::make_pair( left, right );
	};

	if( has_empty_history() )
		// Can't show trace because in normal circumstances there won't be
		// trace with empty history.
		return;

	const auto minmax = min_max_times();

	const auto calc_position = [minmax, step](const auto when) {
		return static_cast<std::size_t>( (when - minmax.first) / step );
	};

	constexpr char empty_char = ' ';
	auto output_length = calc_position( minmax.second );
	if( !output_length )
		output_length = 1u;

	std::string output_line( output_length, empty_char );

	for( std::size_t index{}; index != names.size(); ++index )
	{
		std::fill( output_line.begin(), output_line.end(), empty_char );

		const auto & history = trace[ index ];
		// Last item can be ignored because it is 'quit' indicator.
		const std::size_t j_max = history.size() - 1;

		for( std::size_t j = 0; j != j_max; ++j )
		{
			auto left_pos = calc_position( history[ j ].m_when );
			const auto right_pos = calc_position( history[ j+1 ].m_when );

			const auto st = history[ j ].m_state;
			if( st_hungry_thinking == st )
				// Indication of hungry_thinking shouldn't overwrite the
				// previous item because it can be an important wait_left
				// or wait_right indicators (as a signle symbol).
				left_pos += 1;

			std::fill(
					output_line.begin() + left_pos,
					output_line.begin() + right_pos + 1,
					history[ j ].m_state );
		}

		fmt::print( "[{:>3}]{:>15}: {}\n", index, names[ index ], output_line );
	}
}

} /* namespace trace */

