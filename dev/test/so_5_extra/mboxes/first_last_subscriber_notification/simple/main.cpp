#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/first_last_subscriber_notification.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace mbox_ns = so_5::extra::mboxes::first_last_subscriber_notification;

class main_agent final : public so_5::agent_t
{
	struct dummy final : public so_5::signal_t {};

	const so_5::mbox_t m_test_mbox;

public:
	main_agent( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_test_mbox{
				mbox_ns::make_multi_consumer_mbox< dummy >(
						so_environment(),
						so_direct_mbox() )
			}
	{}

	void
	so_define_agent() override
	{
		// We can't make subscription to m_test_mbox here because
		// the first msg_first_subscriber will be lost (agent isn't
		// bound to an event queue yet).
		// Subscription to `dummy` from m_test_mbox will be made in so_evt_start.
		so_subscribe_self()
			.event( &main_agent::evt_first_subscriber )
			.event( &main_agent::evt_last_subscriber )
			;
	}

	void
	so_evt_start() override
	{
		so_subscribe( m_test_mbox )
			.event( &main_agent::evt_dummy )
			;
	}

private:
	void
	evt_dummy( mhood_t<dummy> )
	{
		so_drop_subscription( m_test_mbox, &main_agent::evt_dummy );
	}

	void
	evt_first_subscriber( mhood_t< mbox_ns::msg_first_subscriber > )
	{
		so_5::send< dummy >( m_test_mbox );
	}

	void
	evt_last_subscriber( mhood_t< mbox_ns::msg_last_subscriber > )
	{
		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "simple case" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< main_agent >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

