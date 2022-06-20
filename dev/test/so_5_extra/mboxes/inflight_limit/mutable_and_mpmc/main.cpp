#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/inflight_limit.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace mbox_ns = so_5::extra::mboxes::inflight_limit;

struct msg_test final : public so_5::message_t {};

TEST_CASE( "builder" )
{
	run_with_time_limit( [] {
			bool exception_thrown = false;
			so_5::launch( [&](so_5::environment_t & env) {
						try
						{
							auto mbox = mbox_ns::make_mbox< so_5::mutable_msg<msg_test> >(
									env.create_mbox(),
									25u );
						}
						catch( const so_5::exception_t & x )
						{
							if( so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox ==
									x.error_code() )
							{
								exception_thrown = true;
							}
						}
					} );

			REQUIRE( exception_thrown );
		},
		5 );
}

