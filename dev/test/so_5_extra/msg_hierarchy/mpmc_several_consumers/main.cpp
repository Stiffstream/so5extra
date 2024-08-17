#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "sender.hpp"
#include "stopper.hpp"
#include "receivers.hpp"

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using namespace std::chrono_literals;

using namespace test;

TEST_CASE( "mpmc_several_consumers" )
{
	bool completed{ false };
	std::string trace_first;
	std::string trace_second;
	std::string trace_third;
	std::string trace_forth;

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [&](so_5::coop_t & coop) {
								const auto stopper_mbox = coop.make_agent< a_stopper_t >( 5 )->
										so_direct_mbox();

								hierarchy_ns::demuxer_t< base_message > demuxer{
										coop.environment(),
										hierarchy_ns::multi_consumer
									};

								coop.make_agent<a_sender_t>( demuxer, stopper_mbox );
								coop.make_agent<a_first_receiver_t>(
										demuxer,
										trace_first,
										stopper_mbox );
								coop.make_agent<a_second_receiver_t>(
										demuxer,
										trace_second,
										stopper_mbox );
								coop.make_agent<a_third_receiver_t>(
										demuxer,
										trace_third,
										stopper_mbox );
								coop.make_agent<a_forth_receiver_t>(
										demuxer,
										trace_forth,
										stopper_mbox );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
	REQUIRE( "two" == trace_first );
	REQUIRE( "one" == trace_second );
	REQUIRE( "base" == trace_third );
	REQUIRE( "two" == trace_forth );
}

