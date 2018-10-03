/*
 * A test for simple_mtsafe env_infastructure with creation of new coops from
 * outside thread.
 */

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

using namespace std;

constexpr std::size_t total_threads = 3;

struct thread_completed final : public so_5::signal_t {};

class a_coop_member_t final : public so_5::agent_t
{
public :
	a_coop_member_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

	virtual void so_evt_start() override
	{
		so_deregister_agent_coop_normally();
	}
};

std::thread
make_thread( so_5::environment_t & env, so_5::mbox_t finish_mbox )
{
	return std::thread( [&env, finish_mbox] {
		auto ch = so_5::create_mchain( env );

		for( int i = 0; i < 1000; ++i )
		{
			env.introduce_coop( [&ch]( so_5::coop_t & coop ) {
				coop.make_agent< a_coop_member_t >();
				coop.add_dereg_notificator(
						so_5::make_coop_dereg_notificator( ch->as_mbox() ) );
			} );

			so_5::receive( ch, so_5::infinite_wait,
					[]( so_5::mhood_t<so_5::msg_coop_deregistered> ) {} );
		}

		so_5::send< thread_completed >( finish_mbox );
	} );
}

class a_controller_t final : public so_5::agent_t
{
public :
	a_controller_t( context_t ctx )
		: so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self()
			.event< thread_completed >( [this] {
				++m_completed_threads;
				if( m_completed_threads == total_threads )
					so_deregister_agent_coop_normally();
			} );
	}

private :
	std::size_t m_completed_threads{};
};

int
main()
{
	try
	{
		run_with_time_limit(
			[]() {
				asio::io_context io_svc;

				std::array< std::thread, total_threads > outside_threads;

				so_5::launch(
					[&]( so_5::environment_t & env ) {
						so_5::mbox_t finish_mbox;
						env.introduce_coop( [&]( so_5::coop_t & coop ) {
							finish_mbox = coop.make_agent< a_controller_t >()->
									so_direct_mbox();
						} );

						for( auto & t : outside_threads )
							t = make_thread( env, finish_mbox );
					},
					[&]( so_5::environment_params_t & params ) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;
						params.infrastructure_factory( factory(io_svc) );
					} );

				for( auto & t : outside_threads )
					t.join();
			},
			30 );
	}
	catch( const exception & ex )
	{
		cerr << "Error: " << ex.what() << endl;
		return 1;
	}

	return 0;
}

