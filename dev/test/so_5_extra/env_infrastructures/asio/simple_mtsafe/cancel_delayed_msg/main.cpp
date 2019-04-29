#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using clock_type = std::chrono::steady_clock;

struct test_data_t
{
	clock_type::time_point m_started_at;
	clock_type::time_point m_finished_at;
};

const clock_type::duration hello_pause = std::chrono::milliseconds(100);
const clock_type::duration finish_pause = std::chrono::milliseconds(150);

class a_test_case_t final : public so_5::agent_t
{
	test_data_t & m_data;

	so_5::timer_id_t m_hello_timer;

	struct cancel final : public so_5::signal_t {};
	struct hello final : public so_5::signal_t {};
	struct finish final : public so_5::signal_t {};

public :
	a_test_case_t( context_t ctx, test_data_t & data )
		:	so_5::agent_t( std::move(ctx) )
		,	m_data( data )
	{
		so_subscribe_self()
			.event( &a_test_case_t::on_cancel )
			.event( &a_test_case_t::on_hello )
			.event( &a_test_case_t::on_finish );
	}

	virtual void
	so_evt_start() override
	{
		m_data.m_started_at = clock_type::now();

		m_hello_timer = so_5::send_periodic< hello >(
				*this, hello_pause, clock_type::duration::zero() );

		so_5::send_delayed< finish >( *this, finish_pause );

		so_5::send< cancel >( *this );
	}

private :
	void
	on_cancel( mhood_t<cancel> )
	{
		m_hello_timer.release();
	}

	void
	on_hello( mhood_t<hello> )
	{
		throw std::runtime_error( "hello signal must be canceled and should not "
				"be received" );
	}

	void
	on_finish( mhood_t<finish> )
	{
		m_data.m_finished_at = clock_type::now();
		so_deregister_agent_coop_normally();
	}
};

auto ms( clock_type::duration v ) {
	return std::chrono::duration_cast< std::chrono::milliseconds >(v).count();
}

TEST_CASE( "cancel delayed signal" )
{
	run_with_time_limit( [] {
			asio::io_context io_svc;

			test_data_t data;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t >( std::ref(data) ) );
					},
					[&io_svc](so_5::environment_params_t & params) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
					} );

			const auto expected_duration = ms( 9*(finish_pause/10) );

			REQUIRE( expected_duration < ms(data.m_finished_at - data.m_started_at) );
		},
		5 );
}

