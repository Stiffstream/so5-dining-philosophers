#include <dining_philosophers/common/fork_messages.hpp>
#include <dining_philosophers/common/random_generator.hpp>
#include <dining_philosophers/common/defaults.hpp>
#include <dining_philosophers/actor_based/trace_maker/all.hpp>
#include <dining_philosophers/actor_based/common/completion_watcher.hpp>

#include <queue>

// An actor for representing a fork.
// Fork can be in two states: 'free' and 'taken'.
// A queue of philosophers is maintained in taken state.
class fork_t final : public so_5::agent_t
{
public :
	fork_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) } {}

	void so_define_agent() override
	{
		// The starting state is 'free' state.
		this >>= st_free;

		// In 'free' state just one message is handled.
		st_free
			.event( [this]( mhood_t<take_t> cmd ) {
					this >>= st_taken;
					so_5::send< taken_t >( cmd->m_who );
				} );

		// In 'taken' state we should handle two messages.
		st_taken
			.event( [this]( mhood_t<take_t> cmd ) {
					// The requester should wait for some time.
					m_queue.push( cmd->m_who );
				} )
			.event( [this]( mhood_t<put_t> ) {
					if( m_queue.empty() )
						// No one needs the fork. Can switch to 'free' state.
						this >>= st_free;
					else
					{
						// The first philosopher from wait queue should be notified.
						const auto who = m_queue.front();
						m_queue.pop();
						so_5::send< taken_t >( who );
					}
				} );
	}

private :
	// Definition of agent's states.
	const state_t st_free{ this, "free" };
	const state_t st_taken{ this, "taken" };

	// Wait queue for philosophers.
	std::queue< so_5::mbox_t > m_queue;
};

// An actor for representing a philosopher.
// If this philosopher takes a fork it doesn't return it until he/she
// takes the second fork and eats his/her meal.
class greedy_philosopher_t final
	: public so_5::agent_t
	, private random_pause_generator_t
{
	// Signals to be used for limiting thinking/eating time.
	struct stop_thinking_t : public so_5::signal_t {};
	struct stop_eating_t : public so_5::signal_t {};

public :
	greedy_philosopher_t(
		context_t ctx,
		std::size_t index,
		so_5::mbox_t left_fork,
		so_5::mbox_t right_fork,
		int meals_count )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_index{ index }
		,	m_left_fork{ std::move( left_fork ) }
		,	m_right_fork{ std::move( right_fork ) }
		,	m_meals_count{ meals_count }
	{
		// This is necessary for tracing of state changes.
		so_add_destroyable_listener(
				state_watcher_t::make( so_environment(), index ) );
	}

	void so_define_agent() override
	{
		// In 'thinking' state we wait only for delayed stop_thinking signal.
		st_thinking
			.event( [=]( mhood_t<stop_thinking_t> ) {
					// Try to get the left fork.
					this >>= st_wait_left;
					so_5::send< take_t >( m_left_fork, so_direct_mbox(), m_index );
				} );

		// When we wait for the left fork we react only to 'taken' reply.
		st_wait_left
			.event( [=]( mhood_t<taken_t> ) {
					// Now we have the left fork.
					// Try to get the right fork.
					this >>= st_wait_right;
					so_5::send< take_t >( m_right_fork, so_direct_mbox(), m_index );
				} );

		// When we wait for the right fork we react only to 'taken' reply.
		st_wait_right
			.event( [=]( mhood_t<taken_t> ) {
					// We have both forks. Can eat our meal.
					this >>= st_eating;
				} );

		// When we in 'eating' state we react only to 'stop_eating' signal.
		st_eating
			// 'stop_eating' signal should be initiated when we enter 'eating' state.
			.on_enter( [=] {
					so_5::send_delayed< stop_eating_t >( *this, eat_pause() );
				} )
			.event( [=]( mhood_t<stop_eating_t> ) {
				// Both forks should be returned back.
				so_5::send< put_t >( m_right_fork );
				so_5::send< put_t >( m_left_fork );

				// One step closer to the end.
				++m_meals_eaten;
				if( m_meals_count == m_meals_eaten )
					this >>= st_done; // No more meals to eat, we are done.
				else
					think();
			} );

		st_done
			.on_enter( [=] {
				// Notify about completion.
				completion_watcher_t::done( so_environment(), m_index );
			} );
	}

	void so_evt_start() override
	{
		// Agent should start in 'thinking' state.
		think();
	}

private :
	// States of the agent.
	state_t st_thinking{ this, "thinking.normal" };
	state_t st_wait_left{ this, "wait_left" };
	state_t st_wait_right{ this, "wait_right" };
	state_t st_eating{ this, "eating" };

	state_t st_done{ this, "done" };

	// Philosopher's index.
	const std::size_t m_index;

	// Mboxes of left and right forks.
	const so_5::mbox_t m_left_fork;
	const so_5::mbox_t m_right_fork;

	const int m_meals_count;
	int m_meals_eaten{};

	// Switch agent to 'thinking' state and limit thinking time by delayed message.
	void think()
	{
		this >>= st_thinking;
		so_5::send_delayed< stop_thinking_t >(
				*this,
				think_pause( thinking_type_t::normal ) );
	}
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

		// Create forks.
		std::vector< so_5::agent_t * > forks( count, nullptr );
		for( std::size_t i{}; i != count; ++i )
			forks[ i ] = coop.make_agent< fork_t >();

		// Create philosophers.
		for( std::size_t i{}; i != count - 1u; ++i )
			coop.make_agent< greedy_philosopher_t >(
					i,
					forks[ i ]->so_direct_mbox(),
					forks[ i + 1 ]->so_direct_mbox(),
					default_meals_count );
		// The last philosopher should take forks in opposite direction.
		coop.make_agent< greedy_philosopher_t >(
				count - 1u,
				forks[ count - 1u ]->so_direct_mbox(),
				forks[ 0 ]->so_direct_mbox(),
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

