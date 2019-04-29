#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class a_test_t final : public so_5::agent_t
{
	struct initiate_shutdown final : public so_5::signal_t {};

public :
	a_test_t( context_t ctx )
		:	so_5::agent_t( std::move(ctx) )
	{
		auto & s = so_5::extra::shutdowner::layer( so_environment() );
		so_subscribe( s.notify_mbox() ).event( &a_test_t::on_shutdown );

		so_subscribe_self().event( &a_test_t::on_initiate_shutdown );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send_delayed< initiate_shutdown >( *this,
				std::chrono::milliseconds(125) );
	}

private :
	void
	on_shutdown( mhood_t< so_5::extra::shutdowner::shutdown_initiated_t > )
	{
		so_deregister_agent_coop_normally();
	}

	void
	on_initiate_shutdown( mhood_t<initiate_shutdown> )
	{
		so_environment().stop();
	}
};

TEST_CASE( "shutdown with a single subscriber" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [](so_5::coop_t & coop) {
								coop.make_agent< a_test_t >();
							} );
					},
					[](so_5::environment_params_t & params) {
						params.add_layer(
								so_5::extra::shutdowner::make_layer< so_5::null_mutex_t >(
										std::chrono::milliseconds(750) ) );
						params.infrastructure_factory(
								so_5::env_infrastructures::simple_not_mtsafe::factory() );
					} );
		},
		5 );
}

