/*
 * A test for simple_mtsafe env_infastructure with calling to stop from
 * different threads.
 */

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using namespace std;

class a_test_t final : public so_5::agent_t
{
public :
	struct tick final : public so_5::signal_t {};

	a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self()
			.event( [this]( mhood_t< tick > ) {
				so_5::send_delayed< tick >( *this, std::chrono::milliseconds(100) );
			} );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send< tick >( *this );
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

				thread outside_thread;

				so_5::launch(
					[&]( so_5::environment_t & env ) {
						env.introduce_coop( [&]( so_5::coop_t & coop ) {
							coop.make_agent< a_test_t >();
						} );

						outside_thread = thread( [&env] {
							this_thread::sleep_for( chrono::milliseconds( 350 ) );
							env.stop();
						} );
					},
					[&]( so_5::environment_params_t & params ) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;
						params.infrastructure_factory( factory(io_svc) );
					} );

				outside_thread.join();
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

