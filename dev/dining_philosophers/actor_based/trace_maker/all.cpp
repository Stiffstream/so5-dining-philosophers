#include <dining_philosophers/actor_based/trace_maker/all.hpp>

#include <fmt/format.h>

#include <algorithm>

so_5::mbox_t trace_maker_t::make_mbox( so_5::environment_t & env )
{
	return env.create_mbox( "trace_maker" );
}

trace_maker_t::trace_maker_t(
	context_t ctx,
	const names_holder_t & names,
	std::chrono::steady_clock::duration step )
	:	so_5::agent_t{ std::move(ctx) }
	,	m_names{ names }
	,	m_step{ step }
	,	m_trace{ trace::make_trace_data( names.size() ) }
{}

void trace_maker_t::so_define_agent()
{
	so_subscribe( make_mbox(so_environment()) )
			.event( &trace_maker_t::on_state_change );
}

void trace_maker_t::so_evt_finish()
{
	trace::show_trace_data( m_names, m_trace, m_step );
}

void trace_maker_t::on_state_change( mhood_t<trace::state_changed_t> cmd )
{
	m_trace[ cmd->m_index ].emplace_back( cmd->m_when, cmd->m_state );
}

//
// state_watcher_t
//
state_watcher_t::state_watcher_t(
	so_5::mbox_t mbox,
	std::size_t index )
	:	m_mbox{ std::move(mbox) }
	,	m_index{ index }
{}

void state_watcher_t::changed( so_5::agent_t &, const so_5::state_t & state ) noexcept
{
	const auto detect_label = []( const std::string & name ) {
		if( "thinking.normal" == name ) return trace::st_normal_thinking;
		if( "thinking.hungry" == name ) return trace::st_hungry_thinking;
		if( "wait_left" == name ) return trace::st_wait_left;
		if( "wait_right" == name ) return trace::st_wait_right;
		if( "eating" == name ) return trace::st_eating;
		if( "done" == name ) return trace::st_done;
		return '?';
	};

	const char state_label = detect_label( state.query_name() ); 
	if( '?' == state_label )
		return;

	so_5::send< trace::state_changed_t >( m_mbox, m_index, state_label );
}

