#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/inflight_limit.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace mbox_ns = so_5::extra::mboxes::inflight_limit;

class test_agent final : public so_5::agent_t
{
	struct msg_test final : public so_5::message_t {};
	struct msg_quit final : public so_5::signal_t {};

	const so_5::mbox_t m_limited_mbox;

	unsigned int m_messages_received{};

	[[nodiscard]]
	static so_5::mbox_t
	make_limited_mbox( const so_5::mbox_t & dest_mbox )
	{
		return mbox_ns::make_mbox< so_5::mutable_msg<msg_test> >( dest_mbox, 3u );
	}

public:
	test_agent( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_limited_mbox{ make_limited_mbox( so_direct_mbox() ) }
	{}

	void
	so_define_agent() override
	{
		so_subscribe_self()
			.event( [this]( mutable_mhood_t<msg_test> ) {
					++m_messages_received;
				} )
			.event( [this]( mhood_t<msg_quit> ) {
					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< so_5::mutable_msg<msg_test> >( m_limited_mbox );
		so_5::send< so_5::mutable_msg<msg_test> >( m_limited_mbox );
		so_5::send< so_5::mutable_msg<msg_test> >( m_limited_mbox );
		so_5::send< so_5::mutable_msg<msg_test> >( m_limited_mbox );

		so_5::send< msg_quit >( *this );
	}

	void
	so_evt_finish() override
	{
		if( 3u != m_messages_received )
			throw std::runtime_error{
				"unexpected number of received messages: " +
					std::to_string(m_messages_received)
			};
	}
};

TEST_CASE( "builder" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< test_agent >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

