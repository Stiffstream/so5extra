#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

TEST_CASE( "simple shutdown on empty environment" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & /*env*/) {
					},
					[](so_5::environment_params_t & params) {
						params.add_layer(
								so_5::extra::shutdowner::make_layer<>(
										std::chrono::milliseconds(750) ) );
					} );
		},
		5 );
}

