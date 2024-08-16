#pragma once

#include <so_5/all.hpp>

namespace test
{

class a_stopper_t final : public so_5::agent_t
	{
		const unsigned m_required_stops;
		unsigned m_received_stops{};

	public:
		struct msg_done final : public so_5::signal_t {};

		a_stopper_t( context_t ctx, unsigned required_stops );

		void
		so_define_agent() override;

	private:
		void
		evt_done( mhood_t<msg_done> );
	};

} /* namespace test */

