/*
 * A test for simple_mtsafe env_infastructure with ping from outside thread.
 */

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using namespace std;

struct ping final : public so_5::signal_t {};
struct pong final : public so_5::signal_t {};

struct started final : public so_5::signal_t {};


class a_test_t final : public so_5::agent_t
{
public :
	a_test_t( context_t ctx, so_5::mchain_t pong_ch )
		: so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self()
			.event( [this, pong_ch]( mhood_t< ping > ) {
				so_5::send< pong >( pong_ch );
			} );
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
						auto pong_ch = so_5::create_mchain( env );
						auto ready_ch = so_5::create_mchain( env );

						so_5::mbox_t ping_mbox;
						env.introduce_coop( [&]( so_5::coop_t & coop ) {
							ping_mbox = coop.make_agent< a_test_t >( pong_ch )->
									so_direct_mbox();
						} );

						outside_thread = thread(
							[&env, ping_mbox, pong_ch, ready_ch] {
								so_5::send< ping >( ping_mbox );
								so_5::send< started >( ready_ch );

								so_5::receive( from(pong_ch).handle_n(200),
										[ping_mbox](so_5::mhood_t<pong>) {
											so_5::send< ping >( ping_mbox );
										} );

								env.stop();
							} );

						// Wait while outside thread started.
						so_5::receive(
								from(ready_ch).handle_n(1),
								[](so_5::mhood_t<started>) {} );
					},
					[&]( so_5::environment_params_t & params ) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;
						params.infrastructure_factory( factory(io_svc) );
#if 0
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
#endif
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

