#pragma once

#include <dining_philosophers/common/fork_messages.hpp>
#include <dining_philosophers/common/random_generator.hpp>
#include <dining_philosophers/actor_based/trace_maker/all.hpp>
#include <dining_philosophers/actor_based/common/completion_watcher.hpp>

class philosopher_t final
	: public so_5::agent_t
	, private random_pause_generator_t
{
	struct stop_thinking_t : public so_5::signal_t {};
	struct stop_eating_t : public so_5::signal_t {};

public :
	philosopher_t(
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
		so_add_destroyable_listener(
				state_watcher_t::make( so_environment(), index ) );
	}

	void so_define_agent() override
	{
		st_thinking
			.event( [this](mhood_t<stop_thinking_t>) {
				this >>= st_wait_left;
				so_5::send< take_t >( m_left_fork, so_direct_mbox(), m_index );
			} );

		st_wait_left
			.event( [this](mhood_t<taken_t>) {
				this >>= st_wait_right;
				so_5::send< take_t >( m_right_fork, so_direct_mbox(), m_index );
			} )
			.event( [this](mhood_t<busy_t>) {
				think( st_hungry_thinking );
			} );

		st_wait_right
			.event( [this](mhood_t<taken_t>) {
				this >>= st_eating;
			} )
			.event( [this](mhood_t<busy_t>) {
				so_5::send< put_t >( m_left_fork );
				think( st_hungry_thinking );
			} );

		st_eating
			.on_enter( [this] {
					so_5::send_delayed< stop_eating_t >( *this, eat_pause() );
				} )
			.event( [this](mhood_t<stop_eating_t>) {
				so_5::send< put_t >( m_right_fork );
				so_5::send< put_t >( m_left_fork );

				++m_meals_eaten;
				if( m_meals_count == m_meals_eaten )
					this >>= st_done;
				else
					think( st_normal_thinking );
			} );

		st_done
			.on_enter( [this] {
				completion_watcher_t::done( so_environment(), m_index );
			} );
	}

	void so_evt_start() override
	{
		think( st_normal_thinking );
	}

private :
	state_t st_thinking{ this, "thinking" };
	state_t st_normal_thinking{ initial_substate_of{ st_thinking }, "normal" };
	state_t st_hungry_thinking{ substate_of{ st_thinking }, "hungry" };

	state_t st_wait_left{ this, "wait_left" };
	state_t st_wait_right{ this, "wait_right" };
	state_t st_eating{ this, "eating" };

	state_t st_done{ this, "done" };

	const std::size_t m_index;

	const so_5::mbox_t m_left_fork;
	const so_5::mbox_t m_right_fork;

	const int m_meals_count;
	int m_meals_eaten{};

	void think( const state_t & target_st )
	{
		this >>= target_st;
		so_5::send_delayed< stop_thinking_t >(
				*this,
				think_pause( target_st == st_normal_thinking
						? thinking_type_t::normal : thinking_type_t::hungry ) );
	}
};

