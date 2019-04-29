/*
 * A test for simple_mtsafe env_infastructure with one simple agent
 * and delayed message which is sent after call to env.stop().
 */

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using namespace std;

class a_test_t final : public so_5::agent_t
{
public :
	struct perform_stop final : public so_5::signal_t {};
	struct tick final : public so_5::signal_t {};

	a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self()
			.event( [this]( mhood_t< tick > ) { std::abort(); } )
			.event( [this]( mhood_t< perform_stop > ) {
				so_environment().stop();
				so_5::send_delayed< tick >( *this, std::chrono::seconds(10) );
			} );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send< perform_stop >( *this );
	}
};

int
main()
{
	try
	{
		run_with_time_limit(
			[]() {
				asio::io_context io_svc;

				so_5::launch(
					[&]( so_5::environment_t & env ) {
						env.introduce_coop( [&]( so_5::coop_t & coop ) {
							coop.make_agent< a_test_t >();
						} );
					},
					[&]( so_5::environment_params_t & params ) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;
						params.infrastructure_factory( factory(io_svc) );
					} );
			},
			5 );
	}
	catch( const exception & ex )
	{
		cerr << "Error: " << ex.what() << endl;
		return 1;
	}

	return 0;
}

