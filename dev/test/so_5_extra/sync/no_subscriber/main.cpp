#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

class service_t final : public so_5::agent_t
	{
	public :
		using so_5::agent_t::agent_t;
	};

TEST_CASE( "simple shutdown on empty environment" )
{
	enum result_t { undefined, no_reply, reply_received };

	result_t result = undefined;
	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						auto svc = env.introduce_coop( [](so_5::coop_t & coop) {
								return coop.make_agent<service_t>()->so_direct_mbox();
							} );

						try
						{
							// Set timeout to 10s. It won't elapsed because
							// run_with_time_limit() uses 5s limit.
							(void)sync_ns::request_reply_t<int, int>::
									request_value( svc, 10s, 2 );
							result = reply_received;
						}
						catch( const so_5::exception_t & x )
						{
							ensure( sync_ns::errors::rc_no_reply == x.error_code(),
									"sync_ns::errors::no_reply expected, got: " +
									std::to_string( x.error_code() ) );
							result = no_reply;
						}

						env.stop();
					} );
		},
		5 );

	REQUIRE( result == no_reply );
}

