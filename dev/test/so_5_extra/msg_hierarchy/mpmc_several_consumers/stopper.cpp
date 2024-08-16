#include "stopper.hpp"

namespace test
{

a_stopper_t::a_stopper_t( context_t ctx, unsigned required_stops )
	: so_5::agent_t{ std::move(ctx) }
	, m_required_stops{ required_stops }
	{}

void
a_stopper_t::so_define_agent()
	{
		so_subscribe_self().event( &a_stopper_t::evt_done );
	}

void
a_stopper_t::evt_done( mhood_t<msg_done> )
	{
		if( ++m_received_stops >= m_required_stops )
			so_deregister_agent_coop_normally();
	}

} /* namespace test */

