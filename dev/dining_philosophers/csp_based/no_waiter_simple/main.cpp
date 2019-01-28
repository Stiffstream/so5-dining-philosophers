#include <dining_philosophers/csp_based/common/philosopher.hpp>
#include <dining_philosophers/csp_based/trace_maker/all.hpp>

#include <dining_philosophers/common/defaults.hpp>

#include <fmt/format.h>

void fork_process(
	so_5::mchain_t fork_ch )
{
	// State of the fork.
	bool taken = false;

	// Receive and handle all messages until the channel will be closed.
	so_5::receive( so_5::from( fork_ch ),
			[&]( so_5::mhood_t<take_t> cmd ) {
				if( taken )
					so_5::send< busy_t >( cmd->m_who );
				else
				{
					taken = true;
					so_5::send< taken_t >( cmd->m_who );
				}
			},
			[&]( so_5::mhood_t<put_t> ) {
				if( taken )
					taken = false;
			} );
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
	std::vector< std::thread > philosopher_threads( table_size );
	for( std::size_t i{}; i != table_size; ++i )
	{
		// Run philosopher as a thread.
		philosopher_threads[ i ] = std::thread{
				philosopher_process,
				std::ref(tracer),
				control_ch,
				i,
				fork_chains[ i ]->as_mbox(),
				fork_chains[ (i + 1) % table_size ]->as_mbox(),
				default_meals_count };
	}

	// Wait while all philosophers completed.
	so_5::receive( so_5::from( control_ch ).handle_n( table_size ),
			[&names]( so_5::mhood_t<philosopher_done_t> cmd ) {
				fmt::print( "{}: done\n", names[ cmd->m_philosopher_index ] );
			} );

	// Wait for completion of philosopher threads.
	join_all( philosopher_threads );

	// Close channels for all forks.
	for( auto & ch : fork_chains )
		so_5::close_drop_content( ch );

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

