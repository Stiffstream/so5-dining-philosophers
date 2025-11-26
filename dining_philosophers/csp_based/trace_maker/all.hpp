#pragma once

#include <dining_philosophers/common/types.hpp>

#include <so_5/all.hpp>

//
// trace_maker_t
//
class trace_maker_t final
{
public :
	trace_maker_t(
		so_5::environment_t & env,
		const names_holder_t & names,
		std::chrono::steady_clock::duration step );
	~trace_maker_t();

	trace_maker_t( const trace_maker_t & ) = delete;
	trace_maker_t( trace_maker_t && ) = delete;

	void thinking_started(
		std::size_t philosopher_index,
		thinking_type_t thinking_type );

	void take_left_attempt( std::size_t philosopher_index );
	void take_right_attempt( std::size_t philosopher_index );

	void eating_started( std::size_t philosopher_index );

	void philosopher_done( std::size_t philosopher_index );

	void done();

private :
	const names_holder_t & m_names;
	const std::chrono::steady_clock::duration m_step;

	so_5::mchain_t m_ch;

	std::thread m_trace_thread;

	static void thread_func( trace_maker_t * self );
};

