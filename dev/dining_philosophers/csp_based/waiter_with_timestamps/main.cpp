#include <dining_philosophers/common/fork_messages.hpp>
#include <dining_philosophers/common/random_generator.hpp>
#include <dining_philosophers/common/defaults.hpp>

#include <dining_philosophers/csp_based/trace_maker/all.hpp>

#include <so_5_extra/mboxes/proxy.hpp>

#include <fmt/format.h>

namespace details {

// Message for delivering extended information about 'take' request.
struct extended_take_t final : public so_5::message_t
{
	const so_5::mbox_t m_who;
	const std::size_t m_philosopher_index;
	const std::size_t m_fork_index;

	extended_take_t(
		so_5::mbox_t who,
		std::size_t philosopher_index,
		std::size_t fork_index )
		:	m_who{ std::move(who) }
		,	m_philosopher_index{ philosopher_index }
		,	m_fork_index{ fork_index }
	{}
};

// Message for delivering extended information about 'put' request.
struct extended_put_t final : public so_5::message_t
{
	const std::size_t m_fork_index;

	extended_put_t(
		std::size_t fork_index )
		:	m_fork_index{ fork_index }
	{}
};

// Special mbox that translates 'take_t' to 'extended_take_t' and
// 'put_t' to 'extended_put_t'.
//
// Mbox should implement several methods inherited from
// so_5::abstract_message_box_t class. It's a very boring tasks.
// But there is a special proxy class in so_5_extra project that already
// do exactly that. Therefore wrapping_mbox_t is derived from that
// proxy instead of so_5::abstract_message_box_t.
//
class wrapping_mbox_t final : public so_5::extra::mboxes::proxy::simple_t
{
	using base_type_t = so_5::extra::mboxes::proxy::simple_t;

	// Where messages should be delivered.
	const so_5::mbox_t m_target;
	// Index of fork to be placed into extended_take_t and extended_put_t.
	const std::size_t m_fork_index;

	// IDs of types to be intercepted and translated.
	static std::type_index original_take_type;
	static std::type_index original_put_type;

public :
	wrapping_mbox_t(
		const so_5::mbox_t & target,
		std::size_t fork_index )
		:	base_type_t{ target }
		,	m_target{ target }
		,	m_fork_index{ fork_index }
	{}

	// This is the main method of so_5::abstract_message_box_t for message
	// delivery. We override it to intercept and translate some types of
	// messages.
	void do_deliver_message(
		const std::type_index & msg_type,
		const so_5::message_ref_t & message,
		unsigned int overlimit_reaction_deep ) const override
	{
		if( original_take_type == msg_type )
		{
			// Take access to the content of original message.
			const auto & original_msg = so_5::message_payload_type<::take_t>::
					payload_reference( *message );
			// Send new message instead of original one.
			so_5::send< extended_take_t >(
					m_target,
					original_msg.m_who,
					original_msg.m_philosopher_index,
					m_fork_index );
		}
		else if( original_put_type == msg_type )
		{
			// Send new message instead of original signal.
			so_5::send< extended_put_t >( m_target, m_fork_index );
		}
		else
			base_type_t::do_deliver_message(
					msg_type,
					message,
					overlimit_reaction_deep );
	}

	// Factory method just for simplicity of wrapping_mbox_t creation.
	static auto make(
		const so_5::mbox_t & target,
		std::size_t fork_index )
	{
		return so_5::mbox_t{ new wrapping_mbox_t{ target, fork_index } };
	}
};

std::type_index wrapping_mbox_t::original_take_type{ typeid(::take_t) };
std::type_index wrapping_mbox_t::original_put_type{ typeid(::put_t) };

//
// waiter_logic_t
//
class waiter_logic_t final
{
public :
	waiter_logic_t(
		const so_5::mbox_t & msg_target,
		std::size_t forks_count,
		// Amount of time after that a philosopher should take a
		// priority acquiring forks.
		std::chrono::steady_clock::duration failures_threshold )
		:	m_failures_threshold{ failures_threshold }
		,	m_fork_states( forks_count, fork_state_t::free )
		,	m_failures( forks_count, failure_info_t{} )
	{
		// Mboxes for every "fork" should be created.
		m_fork_mboxes.reserve( forks_count );
		for( std::size_t i{}; i != forks_count; ++i )
			m_fork_mboxes.push_back(
					details::wrapping_mbox_t::make( msg_target, i ) );
	}

	// Get mbox of fork with specified index.
	const so_5::mbox_t & fork_mbox( std::size_t index ) const noexcept
	{
		return m_fork_mboxes[ index ];
	}

	// Actual handler for 'take' request.
	void on_take_fork( so_5::mhood_t<extended_take_t> cmd )
	{
		// Use the fact that index of left fork is always equal to
		// index of the philosopher itself.
		if( cmd->m_fork_index == cmd->m_philosopher_index )
			handle_take_left_fork( std::move(cmd) );
		else
			handle_take_right_fork( std::move(cmd) );
	}

	// Actual handler for 'put' request.
	void on_put_fork( so_5::mhood_t<extended_put_t> cmd )
	{
		m_fork_states[ cmd->m_fork_index ] = fork_state_t::free;
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

	// Actual implementation of 'take' request for left fork.
	void handle_take_left_fork( so_5::mhood_t<extended_take_t> cmd )
	{
		const auto left_fork_index = cmd->m_fork_index;
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
	void handle_take_right_fork( so_5::mhood_t<extended_take_t> cmd )
	{
		const auto fork_index = cmd->m_fork_index;

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

} /* namespace details */

void waiter_process(
	so_5::mchain_t waiter_ch,
	details::waiter_logic_t & logic )
{
	// Receive and handle all messages until the channel will be closed.
	so_5::receive( so_5::from( waiter_ch ),
			[&]( so_5::mhood_t<details::extended_take_t> cmd ) {
				logic.on_take_fork( std::move(cmd) );
			},
			[&]( so_5::mhood_t<details::extended_put_t> cmd ) {
				logic.on_put_fork( std::move(cmd) );
			} );
}

void philosopher_process(
	trace_maker_t & tracer,
	so_5::mchain_t control_ch,
	std::size_t philosopher_index,
	so_5::mbox_t left_fork,
	so_5::mbox_t right_fork,
	int meals_count )
{
	int meals_eaten{ 0 };

	// This flag is necessary for tracing of philosopher actions.
	thinking_type_t thinking_type{ thinking_type_t::normal };

	random_pause_generator_t pause_generator;

	// This channel will be used for replies from forks.
	auto self_ch = so_5::create_mchain( control_ch->environment() );

	while( meals_eaten < meals_count )
	{
		tracer.thinking_started( philosopher_index, thinking_type );

		// Simulate thinking by suspending the thread.
		std::this_thread::sleep_for( pause_generator.think_pause( thinking_type ) );

		// For the case if we can't take forks.
		thinking_type = thinking_type_t::hungry;

		// Try to get the left fork.
		tracer.take_left_attempt( philosopher_index );
		so_5::send< take_t >( left_fork, self_ch->as_mbox(), philosopher_index );

		// Request sent, wait for a reply.
		so_5::receive( self_ch, so_5::infinite_wait,
				[]( so_5::mhood_t<busy_t> ) { /* nothing to do */ },
				[&]( so_5::mhood_t<taken_t> ) {
					// Left fork is taken.
					// Try to get the right fork.
					tracer.take_right_attempt( philosopher_index );
					so_5::send< take_t >(
							right_fork, self_ch->as_mbox(), philosopher_index );

					// Request sent, wait for a reply.
					so_5::receive( self_ch, so_5::infinite_wait,
							[]( so_5::mhood_t<busy_t> ) { /* nothing to do */ },
							[&]( so_5::mhood_t<taken_t> ) {
								// Both fork are taken. We can eat.
								tracer.eating_started( philosopher_index );

								// Simulate eating by suspending the thread.
								std::this_thread::sleep_for( pause_generator.eat_pause() );

								// One step closer to the end.
								++meals_eaten;

								// Right fork should be returned after eating.
								so_5::send< put_t >( right_fork );

								// Next thinking will be normal, not 'hungry_thinking'.
								thinking_type = thinking_type_t::normal;
							} );

					// Left fork should be returned.
					so_5::send< put_t >( left_fork );
				} );

	}

	// Notify about the completion of the work.
	tracer.philosopher_done( philosopher_index );
	so_5::send< philosopher_done_t >( control_ch, philosopher_index );
}

void run_simulation(
	so_5::environment_t & env,
	const names_holder_t & names ) noexcept
{
	const auto table_size = names.size();
	const auto join_all = []( std::vector<std::thread> & threads ) {
		for( auto & t : threads )
			t.join();
	};

	trace_maker_t tracer{
			env,
			names,
			random_pause_generator_t::trace_step() };

	// Create and run waiter.
	auto waiter_ch = so_5::create_mchain( env );
	details::waiter_logic_t waiter_logic{
			waiter_ch->as_mbox(),
			table_size,
			std::chrono::milliseconds(100)
	};
	std::thread waiter_thread{
			waiter_process,
			waiter_ch,
			std::ref(waiter_logic)
	};

	// Chain for acks from philosophers.
	auto control_ch = so_5::create_mchain( env );

	// Create philosophers.
	std::vector< std::thread > philosopher_threads( table_size );
	for( std::size_t i{}; i != table_size; ++i )
	{
		// Run philosopher as a thread.
		philosopher_threads[ i ] = std::thread{
				philosopher_process,
				std::ref(tracer),
				control_ch,
				i,
				waiter_logic.fork_mbox( i ),
				waiter_logic.fork_mbox( (i + 1) % table_size ),
				default_meals_count };
	}

	// Wait while all philosophers completed.
	so_5::receive( so_5::from( control_ch ).handle_n( table_size ),
			[&names]( so_5::mhood_t<philosopher_done_t> cmd ) {
				fmt::print( "{}: done\n", names[ cmd->m_philosopher_index ] );
			} );

	// Wait for completion of philosopher threads.
	join_all( philosopher_threads );

	// Close waiter channel.
	so_5::close_drop_content( waiter_ch );

	// Wait for completion of waiter thread.
	waiter_thread.join();

	// Show the result.
	tracer.done();

	// Stop the SObjectizer.
	env.stop();
}

int main()
{
	try
	{
		const names_holder_t names{
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

