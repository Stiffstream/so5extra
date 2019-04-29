#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

class service_t final : public so_5::agent_t
	{
	public :
		struct get_answer final : public so_5::signal_t {};

		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self().event( &service_t::on_request );
			}

	private :
		void
		on_request( sync_ns::request_mhood_t<get_answer, int> cmd )
			{
				cmd->make_reply( 42 );
			}
	};

TEST_CASE( "simple shutdown on empty environment" )
{
	int result{};

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						auto svc = env.introduce_coop( [](so_5::coop_t & coop) {
								return coop.make_agent<service_t>()->so_direct_mbox();
							} );

						result = sync_ns::request_reply<service_t::get_answer, int>(
								svc, 5s );

						env.stop();
					} );
		},
		5 );

	REQUIRE( result == 42 );
}

