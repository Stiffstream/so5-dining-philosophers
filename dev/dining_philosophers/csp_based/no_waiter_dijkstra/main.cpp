#include <dining_philosophers/csp_based/trace_maker/all.hpp>

#include <dining_philosophers/common/fork_messages.hpp>
#include <dining_philosophers/common/random_generator.hpp>
#include <dining_philosophers/common/defaults.hpp>

#include <fmt/format.h>

#include <queue>

void fork_process(
	so_5::mchain_t fork_ch )
{
	// State of the fork.
	bool taken = false;

	// Queue of waiting philosophers.
	std::queue< so_5::mbox_t > wait_queue;

	// Receive and handle all messages until the channel will be closed.
	so_5::receive( so_5::from( fork_ch ).handle_all(),
			[&]( so_5::mhood_t<take_t> cmd ) {
				if( taken )
					// Fork already taken. The requester should be stored in queue.
					wait_queue.push( cmd->m_who );
				else
				{
					// Fork can be acquired by the requester.
					taken = true;
					so_5::send< taken_t >( cmd->m_who );
				}
			},
			[&]( so_5::mhood_t<put_t> ) {
				if( wait_queue.empty() )
					taken = false; // No more waiting philosophers. Fork is free again.
				else
				{
					// The first philosopher from queue should be notified.
					const auto who = wait_queue.front();
					wait_queue.pop();
					so_5::send< taken_t >( who );
				}
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

	random_pause_generator_t pause_generator;

	// This channel will be used for replies from forks.
	auto self_ch = so_5::create_mchain( control_ch->environment() );

	while( meals_eaten < meals_count )
	{
		tracer.thinking_started( philosopher_index, thinking_type_t::normal );

		// Simulate thinking by suspending the thread.
		std::this_thread::sleep_for(
				pause_generator.think_pause( thinking_type_t::normal ) );

		// Try to get the left fork.
		tracer.take_left_attempt( philosopher_index );
		so_5::send< take_t >( left_fork, self_ch->as_mbox(), philosopher_index );

		// Request sent, wait for a reply.
		so_5::receive( so_5::from( self_ch ).handle_n( 1u ),
			[&]( so_5::mhood_t<taken_t> ) {
				// Left fork is taken.
				// Try to get the right fork.
				tracer.take_right_attempt( philosopher_index );
				so_5::send< take_t >(
						right_fork, self_ch->as_mbox(), philosopher_index );

				// Request sent, wait for a reply.
				so_5::receive( so_5::from( self_ch ).handle_n( 1u ),
					[&]( so_5::mhood_t<taken_t> ) {
						// Both fork are taken. We can eat.
						tracer.eating_started( philosopher_index );

						// Simulate eating by suspending the thread.
						std::this_thread::sleep_for( pause_generator.eat_pause() );

						// One step closer to the end.
						++meals_eaten;

						// Right fork should be returned after eating.
						so_5::send< put_t >( right_fork );
					} );

				// Left fork should be returned too.
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

	// Create forks.
	std::vector< so_5::mchain_t > fork_chains;
	std::vector< std::thread > fork_threads( table_size );

	for( std::size_t i{}; i != table_size; ++i )
	{
		// Personal channel for fork.
		fork_chains.emplace_back( so_5::create_mchain(env) );
		// Run fork as a thread.
		fork_threads[ i ] = std::thread{ fork_process, fork_chains.back() };
	}

	// Chain for acks from philosophers.
	auto control_ch = so_5::create_mchain( env );

	// Create philosophers.
	const auto philosopher_maker =
			[&](auto index, auto left_fork_idx, auto right_fork_idx) {
				return std::thread{
						philosopher_process,
						std::ref(tracer),
						control_ch,
						index,
						fork_chains[ left_fork_idx ]->as_mbox(),
						fork_chains[ right_fork_idx ]->as_mbox(),
						default_meals_count };
			};
	std::vector< std::thread > philosopher_threads( table_size );
	for( std::size_t i{}; i != table_size - 1u; ++i )
	{
		// Run philosopher as a thread.
		philosopher_threads[ i ] = philosopher_maker( i, i, i+1u );
	}
	// The last philosopher should take forks in opposite direction.
	philosopher_threads[ table_size - 1u ] = philosopher_maker(
			table_size - 1u,
			table_size - 1u,
			0u );

	// Wait while all philosophers completed.
	so_5::receive( so_5::from( control_ch ).handle_n( table_size ),
			[&names]( so_5::mhood_t<philosopher_done_t> cmd ) {
				fmt::print( "{}: done\n", names[ cmd->m_philosopher_index ] );
			} );

	// Wait for completion of philosopher threads.
	join_all( philosopher_threads );

	// Close channels for all forks.
	for( auto & ch : fork_chains )
		so_5::close_drop_content( so_5::terminate_if_throws, ch );

	// Wait for completion of fork threads.
	join_all( fork_threads );

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

