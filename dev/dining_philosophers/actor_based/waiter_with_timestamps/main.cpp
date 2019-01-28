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
	waiter_t(
		context_t ctx,
		std::size_t forks_count,
		// Amount of time after that a philosopher should take a
		// priority acquiring forks.
		std::chrono::steady_clock::duration failures_threshold )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_failures_threshold{ failures_threshold }
		,	m_fork_states( forks_count, fork_state_t::free )
		,	m_failures( forks_count, failure_info_t{} )
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

	// Description of failures of a philosopher.
	class failure_info_t
	{
		// Count of failed attempts.
		// When it is empty the m_first_at has no sense.
		unsigned int m_counter{ 0u };
		// Time when the first failure happened.
		std::chrono::steady_clock::time_point m_first_at;

	public:
		failure_info_t() = default;

		auto actual() const noexcept { return 0u != m_counter; }

		void increment()
		{
			if( !actual() )
				m_first_at = std::chrono::steady_clock::now();
			++m_counter;
		}

		auto earliest() const noexcept { return m_first_at; }

		void clear() noexcept { m_counter = 0u; }

		// Return 'true' if 'a' has greater priority than 'b'.
		// Failure info 'a' has greater priority of it really describes
		// a failure and that failure happened before 'b'.
		static bool has_greater_priority(
			const failure_info_t & a,
			const failure_info_t & b ) noexcept
		{
			// Time of the first failure can be compared if both 'a' and 'b'
			// holds information about failures.
			if( a.actual() && b.actual() )
				return a.earliest() < b.earliest();
			else
				// 'a' or 'b' (or both) has no actual failure info.
				// Object 'a' will have greater priority only if 'a'
				// has actual failure info.
				return a.actual();
		}
	};

	// Amount of time after that a philosopher should take a
	// priority acquiring forks.
	const std::chrono::steady_clock::duration m_failures_threshold;

	// Mboxes for "forks".
	std::vector< so_5::mbox_t > m_fork_mboxes;

	// Current states for "forks".
	std::vector< fork_state_t > m_fork_states;

	// Information of philisophers' failuers.
	// Every item in that vector related to the corresponding philosopher.
	std::vector< failure_info_t > m_failures;

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
	void handle_take_left_fork(
		mhood_t<take_t> cmd,
		std::size_t left_fork_index )
	{
		const auto right_fork_index = (left_fork_index + 1) % m_fork_states.size();
		// Philopsoher can eat only if both fork are free now.
		bool can_eat =
				(fork_state_t::free == m_fork_states[ left_fork_index ]) &&
				(fork_state_t::free == m_fork_states[ right_fork_index ]);

		if( can_eat )
		{
			// May be the left neighbor has greater priority?
			const auto left_neighbor = (m_fork_states.size()
					+ cmd->m_philosopher_index - 1) % m_fork_states.size();

			can_eat = has_greater_priority(
					cmd->m_philosopher_index, left_neighbor );
		}

		if( can_eat )
		{
			// May be the left neighbor has greater priority?
			const auto right_neighbor =
					(cmd->m_philosopher_index + 1) % m_fork_states.size();

			can_eat = has_greater_priority(
					cmd->m_philosopher_index, right_neighbor );
		}

		if( can_eat )
		{
			// Both forks are free and there is no any neighbor with greater priority.
			// We can allow the requester to eat.
			// All previous information about failures no more relevant.
			m_failures[ cmd->m_philosopher_index ].clear();

			// Left fork will be taken to the requester right now.
			m_fork_states[ left_fork_index ] = fork_state_t::taken;
			// But the right fork will be marked as reserver until next 'take' request.
			m_fork_states[ right_fork_index ] = fork_state_t::reserved;

			so_5::send< taken_t >( cmd->m_who );
		}
		else
		{
			// Failures info should be update for the requester.
			m_failures[ cmd->m_philosopher_index ].increment();

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

	// Should this failure info be considered at all?
	// Content of 'info' should be considered only if 'info' contains
	// information about actual failure and appropriate amount of time passed
	// since the first failure.
	bool should_be_considered( const failure_info_t & info ) const noexcept
	{
		if( info.actual() )
			return info.earliest() + m_failures_threshold < std::chrono::steady_clock::now();

		return false;
	}

	// Returns 'true' if philosopher with 'requester_index' has greater priority
	// of serving than philosopher with 'neighbor_index'.
	bool has_greater_priority(
		std::size_t requester_index,
		std::size_t neighbor_index ) const noexcept
	{
		const auto & neighbor_failures = m_failures[ neighbor_index ];
		if( should_be_considered( neighbor_failures ) )
		{
			const auto & requester_failures = m_failures[ requester_index ];
			if( should_be_considered( requester_failures ) )
			{
				// Neighbor and requester have actual failure infos.
				// The result will depend on the content of that information.
				return failure_info_t::has_greater_priority(
						requester_failures, neighbor_failures );
			}
			else
				// Neighbor has actual failure info, but requester hasn't.
				// Neighbor has greater priority.
				return false;
		}
		else
			// Neighbor has no failure info.
			// Requester has greater priority.
			return true;
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

		auto * waiter = coop.make_agent< waiter_t >(
				count,
				std::chrono::milliseconds(50) );

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

