#include <dining_philosophers/actor_based/common/philosopher.hpp>
#include <dining_philosophers/common/defaults.hpp>

class fork_t final : public so_5::agent_t
{
public :
	fork_t( context_t ctx ) : so_5::agent_t( ctx )
   {
		this >>= st_free;

		st_free.event( [this]( mhood_t<take_t> cmd )
				{
					this >>= st_taken;
					so_5::send< taken_t >( cmd->m_who );
				} );

		st_taken.event( []( mhood_t<take_t> cmd )
				{
					so_5::send< busy_t >( cmd->m_who );
				} )
			.just_switch_to< put_t >( st_free );
	}

private :
	const state_t st_free{ this };
	const state_t st_taken{ this };
};

void run_simulation( so_5::environment_t & env, const names_holder_t & names )
{
	env.introduce_coop( [&]( so_5::coop_t & coop ) {
		coop.make_agent_with_binder< trace_maker_t >(
				so_5::disp::one_thread::make_dispatcher( env ).binder(),
				names,
				random_pause_generator_t::trace_step() );

		coop.make_agent_with_binder< completion_watcher_t >(
				so_5::disp::one_thread::make_dispatcher( env ).binder(),
				names );

		const auto count = names.size();

		// Params for tuning thread_pool behavior.
		so_5::disp::thread_pool::bind_params_t bind_params;
		bind_params.fifo( so_5::disp::thread_pool::fifo_t::individual );

		std::vector< so_5::agent_t * > forks( count, nullptr );
		// Create a thread_pool dispatcher for fork agents.
		auto fork_disp = so_5::disp::thread_pool::make_dispatcher(
					env,
					3u // Size of the pool
				);
		for( std::size_t i{}; i != count; ++i )
			// Every fork actor will be bound to fork_disp dispatcher.
			forks[ i ] = coop.make_agent_with_binder< fork_t >(
					fork_disp.binder( bind_params ) );

		// Create a thread_pool dispatcher for philosopher agents.
		auto philosopher_disp = so_5::disp::thread_pool::make_dispatcher(
					env,
					6u // Size of the pool
				);
		for( std::size_t i{}; i != count; ++i )
			coop.make_agent_with_binder< philosopher_t >(
					philosopher_disp.binder( bind_params ),
					i,
					forks[ i ]->so_direct_mbox(),
					forks[ (i + 1) % count ]->so_direct_mbox(),
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

