#include <dining_philosophers/csp_based/trace_maker/all.hpp>

#include <dining_philosophers/common/trace.hpp>

#include <fmt/format.h>

#include <algorithm>

//
// trace_maker_t
//
trace_maker_t::trace_maker_t(
	so_5::environment_t & env,
	const names_holder_t & names,
	std::chrono::steady_clock::duration step )
	:	m_names{ names }
	,	m_step{ step }
	,	m_ch{ so_5::create_mchain( env ) }
{
	m_trace_thread = std::thread{ trace_maker_t::thread_func, this };
}

trace_maker_t::~trace_maker_t()
{
	done();
}

void trace_maker_t::thinking_started(
	std::size_t philosopher_index,
	thinking_type_t thinking_type )
{
	so_5::send< trace::state_changed_t >( m_ch, philosopher_index,
			(thinking_type_t::normal == thinking_type ?
			 		trace::st_normal_thinking : trace::st_hungry_thinking ) );
}

void trace_maker_t::take_left_attempt( std::size_t philosopher_index )
{
	so_5::send< trace::state_changed_t >( m_ch, philosopher_index, trace::st_wait_left );
}

void trace_maker_t::take_right_attempt( std::size_t philosopher_index )
{
	so_5::send< trace::state_changed_t >( m_ch, philosopher_index, trace::st_wait_right );
}

void trace_maker_t::eating_started( std::size_t philosopher_index )
{
	so_5::send< trace::state_changed_t >( m_ch, philosopher_index, trace::st_eating );
}

void trace_maker_t::philosopher_done( std::size_t philosopher_index )
{
	so_5::send< trace::state_changed_t >( m_ch, philosopher_index, trace::st_done );
}

void trace_maker_t::done()
{
	if( m_trace_thread.joinable() )
	{
		so_5::close_retain_content( m_ch );
		m_trace_thread.join();
	}
}

void trace_maker_t::thread_func( trace_maker_t * self )
{
	auto trace_data = trace::make_trace_data( self->m_names.size() );

	so_5::receive( so_5::from( self->m_ch ),
			[&trace_data]( so_5::mhood_t< trace::state_changed_t > cmd ) {
				trace_data[ cmd->m_index ].emplace_back( cmd->m_when, cmd->m_state );
			} );

	trace::show_trace_data( self->m_names, trace_data, self->m_step );
}

