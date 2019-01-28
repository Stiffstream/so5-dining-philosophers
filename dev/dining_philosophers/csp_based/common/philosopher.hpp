#pragma once

#include <dining_philosophers/csp_based/trace_maker/all.hpp>

#include <dining_philosophers/common/fork_messages.hpp>
#include <dining_philosophers/common/random_generator.hpp>

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
			[&]( so_5::mhood_t<taken_t> ) {
				// Left fork is taken.
				// Try to get the right fork.
				tracer.take_right_attempt( philosopher_index );
				so_5::send< take_t >(
						right_fork, self_ch->as_mbox(), philosopher_index );

				// Request sent, wait for a reply.
				so_5::receive( self_ch, so_5::infinite_wait,
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

