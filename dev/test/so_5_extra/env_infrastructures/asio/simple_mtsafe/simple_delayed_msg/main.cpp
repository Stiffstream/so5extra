#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

using clock_type = std::chrono::steady_clock;

struct test_data_t
{
	clock_type::time_point m_sent_at;
	clock_type::time_point m_received_at;
};

const clock_type::duration msg_pause = std::chrono::milliseconds(250);

class a_test_case_t final : public so_5::agent_t
{
	test_data_t & m_data;

	struct hello final : public so_5::signal_t {};

public :
	a_test_case_t( context_t ctx, test_data_t & data )
		:	so_5::agent_t( std::move(ctx) )
		,	m_data( data )
	{
		so_subscribe_self().event( &a_test_case_t::on_hello );
	}

	virtual void
	so_evt_start() override
	{
		m_data.m_sent_at = clock_type::now();
		so_5::send_delayed< hello >( *this, msg_pause );
	}

private :
	void
	on_hello( mhood_t<hello> )
	{
		m_data.m_received_at = clock_type::now();
		so_deregister_agent_coop_normally();
	}
};

auto ms( clock_type::duration v ) {
	return std::chrono::duration_cast< std::chrono::milliseconds >(v).count();
}

TEST_CASE( "receive simple delayed signal" )
{
	run_with_time_limit( [] {
			asio::io_context io_svc;

			test_data_t data;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent< a_test_case_t >( std::ref(data) ) );
					},
					[&io_svc](so_5::environment_params_t & params) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
					} );

			const auto expected_duration = ms( 9*(msg_pause/10) );

			REQUIRE( expected_duration < ms(data.m_received_at - data.m_sent_at) );
		},
		5 );
}

