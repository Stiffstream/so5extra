#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class a_test_t final : public so_5::agent_t
{
public :
	a_test_t( context_t ctx )
		:	so_5::agent_t( std::move(ctx) )
	{
		auto & s = so_5::extra::shutdowner::layer( so_environment() );
		so_subscribe( s.notify_mbox() ).event( &a_test_t::on_shutdown );
	}

private :
	void
	on_shutdown( mhood_t< so_5::extra::shutdowner::shutdown_initiated_t > )
	{
	}
};

int main()
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [](so_5::coop_t & coop) {
								coop.make_agent< a_test_t >();
							} );

						env.stop();
					},
					[](so_5::environment_params_t & params) {
						params.add_layer(
								so_5::extra::shutdowner::make_layer<>(
										std::chrono::milliseconds(500) ) );
					} );
		},
		5 );

	return 0;
}

