#pragma once

#include <dining_philosophers/common/types.hpp>

#include <so_5/all.hpp>

#include <fmt/format.h>

//
// completion_watcher_t
//
class completion_watcher_t final : public so_5::agent_t
{
	const names_holder_t & m_names;
	std::size_t m_completed{};

	static auto make_mbox( so_5::environment_t & env )
	{
		return env.create_mbox( "completion_watcher" );
	}

public :
	completion_watcher_t( context_t ctx, const names_holder_t & names )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_names{ names }
	{
		so_subscribe( make_mbox( so_environment() ) )
				.event( [this]( mhood_t<philosopher_done_t> cmd ) {
					fmt::print( "{}: done\n", m_names[ cmd->m_philosopher_index ] );

					++m_completed;
					if( m_completed == m_names.size() )
						so_environment().stop();
				} );
	}

	static void done(
		so_5::environment_t & env,
		std::size_t philosopher_index )
	{
		so_5::send< philosopher_done_t >( make_mbox(env), philosopher_index );
	}
};

