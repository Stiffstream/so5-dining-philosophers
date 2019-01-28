#include <dining_philosophers/actor_based/common/philosopher.hpp>
#include <dining_philosophers/common/defaults.hpp>

#include <fmt/format.h>

// An actor for representing a waiter.
//
// This actor creates individual mboxes for "forks" and handles all messages
// sent to those mboxes.
//
class waiter_t final : public so_5::agent_t
{
public :
	waiter_t( context_t ctx, std::size_t forks_count )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_fork_states( forks_count, fork_state_t::free )
	{
		// Mboxes for every "fork" should be created.
		m_fork_mboxes.reserve( forks_count );
		for( std::size_t i{}; i != forks_count; ++i )
			m_fork_mboxes.push_back( so_environment().create_mbox() );
	}

	// Get mbox of fork with specified index.
	const so_5::mbox_t & fork_mbox( std::size_t index ) const noexcept
	{
		return m_fork_mboxes[ index ];
	}

	void so_define_agent() override
	{
		// We should make subscription for all mboxes of "forks".
		for( std::size_t i{}; i != m_fork_mboxes.size(); ++i )
		{
			// We need an index of fork. Because of that we use lambdas
			// as message handlers. Those lambdas capture indexes and
			// then pass indexes to actual message handlers.
			so_subscribe( fork_mbox( i ) )
				.event( [i, this]( mhood_t<take_t> cmd ) {
						on_take_fork( std::move(cmd), i );
					} )
				.event( [i, this]( mhood_t<put_t> cmd ) {
						on_put_fork( std::move(cmd), i );
					} );
		}
	}

private :
	// Every "fork" can be in one on these states.
	enum class fork_state_t
	{
		free,
		taken,
		reserved
	};

	// Mboxes for "forks".
	std::vector< so_5::mbox_t > m_fork_mboxes;

	// Current states for "forks".
	std::vector< fork_state_t > m_fork_states;

	// Queue for waiting philosophers. Every philisopher is identified by index.
	std::vector< std::size_t > m_wait_queue;

	// Actual handler for 'take' request.
	void on_take_fork( mhood_t<take_t> cmd, std::size_t fork_index )
	{
		// Use the fact that index of left fork is always equal to
		// index of the philosopher itself.
		if( fork_index == cmd->m_philosopher_index )
			handle_take_left_fork( std::move(cmd), fork_index );
		else
			handle_take_right_fork( std::move(cmd), fork_index );
	}

	// Actual handler for 'put' request.
	void on_put_fork( mhood_t<put_t>, std::size_t fork_index )
	{
		m_fork_states[ fork_index ] = fork_state_t::free;
	}

	// Actual implementation of 'take' request for left fork.
	void handle_take_left_fork( mhood_t<take_t> cmd, std::size_t left_fork_index )
	{
		const auto right_fork_index = (left_fork_index + 1) % m_fork_states.size();
		// Philopsoher can eat only if both fork are free now.
		bool can_eat =
				(fork_state_t::free == m_fork_states[ left_fork_index ]) &&
				(fork_state_t::free == m_fork_states[ right_fork_index ]);

		if( can_eat )
		{
			// Both forks are free. But we should check the presence of
			// neighbors in wait queue.

			const auto left_neighbor =
					(m_fork_states.size() + cmd->m_philosopher_index - 1) % m_fork_states.size();
			const auto right_neighbor =
					(cmd->m_philosopher_index + 1) % m_fork_states.size();

			for( auto it = m_wait_queue.begin(); it != m_wait_queue.end(); ++it )
			{
				if( cmd->m_philosopher_index == *it )
				{
					// We found ourselves before our neighbors.
					// Remove ourselves from waiting queue and approve eating.
					m_wait_queue.erase( it );
					break;
				}
				else if( left_neighbor == *it || right_neighbor == *it )
				{
					// There is some neighbor before us in the queue.
					can_eat = false;
					break;
				}
			}
		}

		if( can_eat )
		{
			// Both forks are free and there is no any neighbor before us in wait queue.
			// Left fork will be taken to the requester right now.
			m_fork_states[ left_fork_index ] = fork_state_t::taken;
			// But the right fork will be marked as reserver until next 'take' request.
			m_fork_states[ right_fork_index ] = fork_state_t::reserved;
			so_5::send< taken_t >( cmd->m_who );
		}
		else
		{
			// The requester should be placed to wait queue if he/she is not here yet.
			if( m_wait_queue.end() == std::find(
					m_wait_queue.begin(), m_wait_queue.end(),
					cmd->m_philosopher_index ) )
			{
				// There is no that philosopher in the wait queue.
				m_wait_queue.push_back( cmd->m_philosopher_index );
			}

			so_5::send< busy_t >( cmd->m_who );
		}
	}

	// Actual implementation of 'take' request for right fork.
	void handle_take_right_fork( mhood_t<take_t> cmd, std::size_t fork_index )
	{
		if( fork_state_t::reserved != m_fork_states[ fork_index ] )
			throw std::runtime_error(
					fmt::format( "unexpected state for right fork, state: {},"
							" fork_index: {}, philosopher_index: {}",
							static_cast<int>(m_fork_states[fork_index]),
							fork_index,
							cmd->m_philosopher_index ) );

		m_fork_states[ fork_index ] = fork_state_t::taken;
		so_5::send< taken_t >( cmd->m_who );
	}
};


void run_simulation( so_5::environment_t & env, const names_holder_t & names )
{
	env.introduce_coop( [&]( so_5::coop_t & coop ) {
		coop.make_agent_with_binder< trace_maker_t >(
				so_5::disp::one_thread::create_private_disp( env )->binder(),
				names,
				random_pause_generator_t::trace_step() );

		coop.make_agent_with_binder< completion_watcher_t >(
				so_5::disp::one_thread::create_private_disp( env )->binder(),
				names );

		const auto count = names.size();

		auto * waiter = coop.make_agent< waiter_t >( count );

		for( std::size_t i{}; i != count; ++i )
			coop.make_agent< philosopher_t >(
					i,
					waiter->fork_mbox( i ),
					waiter->fork_mbox( (i + 1) % count ),
					default_meals_count );
	});
}

int main()
{
	try
	{
		names_holder_t names{
			"Socrates", "Plato", "Aristotle", "Descartes", "Spinoza", "Kant",
			"Schopenhauer", "Nietzsche", "Wittgenstein", "Heidegger", "Sartre"	
		};

		so_5::launch( [&]( so_5::environment_t & env ) {
				run_simulation( env, names );
			} );
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

