#include "sender.hpp"
#include "stopper.hpp"

namespace test
{

a_sender_t::a_sender_t(
	context_t ctx,
	hierarchy_ns::demuxer_t< base_message > & demuxer,
	so_5::mbox_t stopper_mbox )
	: so_5::agent_t{ std::move(ctx) }
	, m_stopper_mbox{ std::move(stopper_mbox) }
	, m_sending_mbox{ demuxer.sending_mbox() }
	{}

void
a_sender_t::so_evt_start()
	{
		so_5::send< data_message_two >( m_sending_mbox );
		so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
	}

} /* namespace test */

