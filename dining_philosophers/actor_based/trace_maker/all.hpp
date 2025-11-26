#pragma once

#include <dining_philosophers/common/types.hpp>
#include <dining_philosophers/common/trace.hpp>

#include <so_5/all.hpp>

//
// trace_maker_t
//
class trace_maker_t final : public so_5::agent_t
{
public :
	static so_5::mbox_t make_mbox( so_5::environment_t & env );

	trace_maker_t(
		context_t ctx,
		const names_holder_t & names,
		std::chrono::steady_clock::duration step );

	void so_define_agent() override;

	void so_evt_finish() override;

private :
	const names_holder_t & m_names;

	const std::chrono::steady_clock::duration m_step;

	trace::trace_data_t m_trace;

	void on_state_change( mhood_t<trace::state_changed_t> cmd );
};

//
// state_watcher_t
//
class state_watcher_t final : public so_5::agent_state_listener_t
{
	const so_5::mbox_t m_mbox;
	const std::size_t m_index;

	state_watcher_t( so_5::mbox_t mbox, std::size_t index );

public :
	static auto make( so_5::environment_t & env, std::size_t index )
	{
		return so_5::agent_state_listener_unique_ptr_t{
				new state_watcher_t{ trace_maker_t::make_mbox(env), index }
		};
	}

	void changed( so_5::agent_t &, const so_5::state_t & state ) noexcept override;
};

