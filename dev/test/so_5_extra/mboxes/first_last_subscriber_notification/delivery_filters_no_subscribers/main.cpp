#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/first_last_subscriber_notification.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace mbox_ns = so_5::extra::mboxes::first_last_subscriber_notification;

struct msg_dummy final : public so_5::message_t {};

class child_agent final : public so_5::agent_t
{
	struct msg_finish final : public so_5::signal_t {};

	const so_5::mbox_t m_test_mbox;
public:
	child_agent(
		context_t ctx,
		so_5::mbox_t test_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_test_mbox{ std::move(test_mbox) }
	{
	}

	void
	so_define_agent()
	{
		so_set_delivery_filter( m_test_mbox,
				[]( const msg_dummy & ) { return true; } );

		so_subscribe_self()
			.event( &child_agent::evt_finish )
			;
	}

	void
	so_evt_start()
	{
		so_5::send< msg_finish >( *this );
	}

private:
	void
	evt_finish( mhood_t< msg_finish > )
	{
		so_deregister_agent_coop_normally();
	}
};

class main_agent final : public so_5::agent_t
{
	struct msg_finish final : public so_5::signal_t {};

	const so_5::mbox_t m_test_mbox;

	const int m_total_coops{ 5 };
	int m_coops_deregistered{ 0 };

public:
	main_agent( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_test_mbox{
				mbox_ns::make_mbox< msg_dummy >(
						so_environment(),
						so_direct_mbox(),
						so_5::mbox_type_t::multi_producer_multi_consumer )
			}
	{}

	void
	so_define_agent() override
	{
		so_subscribe_self()
			.event( &main_agent::evt_first_subscriber )
			.event( &main_agent::evt_last_subscriber )
			.event( &main_agent::evt_finish )
			.event( &main_agent::evt_coop_deregistered )
			;
	}

	void
	so_evt_start() override
	{
		for( int i = 0; i != m_total_coops; ++i )
		{
			so_5::introduce_child_coop( *this,
					[this]( so_5::coop_t & coop ) {
						coop.make_agent< child_agent >( m_test_mbox );
						coop.add_dereg_notificator(
								so_5::make_coop_dereg_notificator( so_direct_mbox() ) );
					} );
		}
	}

private:
	void
	evt_first_subscriber( mhood_t< mbox_ns::msg_first_subscriber > )
	{
		throw std::runtime_error{ "msg_first_subscriber received" };
	}

	void
	evt_last_subscriber( mhood_t< mbox_ns::msg_last_subscriber > )
	{
		throw std::runtime_error{ "msg_last_subscriber received" };
	}

	void
	evt_finish( mhood_t< msg_finish > )
	{
		so_deregister_agent_coop_normally();
	}

	void
	evt_coop_deregistered( mhood_t< so_5::msg_coop_deregistered > )
	{
		++m_coops_deregistered;
		if( m_coops_deregistered == m_total_coops )
			so_5::send< msg_finish >( *this );
	}
};

TEST_CASE( "simple case" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< main_agent >() );
					} /*,
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					}*/ );
		},
		5 );
}

