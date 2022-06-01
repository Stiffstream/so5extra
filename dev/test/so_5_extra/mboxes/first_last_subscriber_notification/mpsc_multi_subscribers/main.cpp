#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/first_last_subscriber_notification.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace mbox_ns = so_5::extra::mboxes::first_last_subscriber_notification;

struct msg_dummy final : public so_5::signal_t {};

struct msg_make_first final : public so_5::signal_t {};
struct msg_make_second final : public so_5::signal_t {};

class first_agent final : public so_5::agent_t
{
	const so_5::mbox_t m_coordination_mbox;
	const so_5::mbox_t m_test_mbox;

public:
	first_agent(
		context_t ctx,
		so_5::mbox_t coordination_mbox,
		so_5::mbox_t test_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_coordination_mbox{ std::move(coordination_mbox) }
		,	m_test_mbox{ std::move(test_mbox) }
	{}

	void
	so_define_agent() override
	{
		so_subscribe( m_coordination_mbox )
			.event( &first_agent::evt_make_first )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< msg_make_first >( m_coordination_mbox );
	}

private:
	void
	evt_make_first( mhood_t< msg_make_first > )
	{
		so_subscribe( m_test_mbox )
			.event( []( mhood_t< msg_dummy > ) {} );

		so_5::send< msg_make_second >( m_coordination_mbox );
	}
};

class second_agent final : public so_5::agent_t
{
	const so_5::mbox_t m_coordination_mbox;
	const so_5::mbox_t m_test_mbox;

public:
	second_agent(
		context_t ctx,
		so_5::mbox_t coordination_mbox,
		so_5::mbox_t test_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_coordination_mbox{ std::move(coordination_mbox) }
		,	m_test_mbox{ std::move(test_mbox) }
	{}

	void
	so_define_agent() override
	{
		so_subscribe( m_coordination_mbox )
			.event( &second_agent::evt_make_second )
			;
	}

private:
	void
	evt_make_second( mhood_t< msg_make_second > )
	{
		bool exception_thrown{ false };

		try
		{
			so_subscribe( m_test_mbox )
				.event( []( mhood_t< msg_dummy > ) {} );
		}
		catch( const so_5::exception_t & )
		{
			exception_thrown = true;
		}

		if( !exception_thrown )
			throw std::runtime_error{ "expected exception wasn't thrown!" };

		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "simple case" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( []( so_5::coop_t & coop ) {
								const auto coordination_mbox =
										coop.environment().create_mbox();

								const auto dead_sink =
										coop.environment().create_mbox();
								const auto test_mbox =
										mbox_ns::make_mbox< msg_dummy >(
												coop.environment(),
												dead_sink,
												so_5::mbox_type_t::multi_producer_single_consumer );

								coop.make_agent< first_agent >(
										coordination_mbox,
										test_mbox );
								coop.make_agent< second_agent >(
										coordination_mbox,
										test_mbox );
							} );
					} /*,
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					}*/ );
		},
		5 );
}

