#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class empty_actor final : public so_5::agent_t
{
public :
	using so_5::agent_t::agent_t;
};

TEST_CASE( "simple shutdown on empty environment" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [](so_5::coop_t & coop) {
								coop.make_agent<empty_actor>();
							} );

						env.stop();
					},
					[](so_5::environment_params_t & params) {
						params.add_layer(
								so_5::extra::shutdowner::make_layer<>(
										std::chrono::milliseconds(750) ) );
					} );
		},
		5 );
}

