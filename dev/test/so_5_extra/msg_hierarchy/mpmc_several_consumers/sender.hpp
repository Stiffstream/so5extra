#pragma once

#include "messages.hpp"

#include <so_5/all.hpp>

namespace test
{

class a_sender_t final : public so_5::agent_t
	{
		const so_5::mbox_t m_stopper_mbox;
		const so_5::mbox_t m_sending_mbox;

	public:
		a_sender_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			so_5::mbox_t stopper_mbox );

		void
		so_evt_start() override;
	};

} /* namespace test */

