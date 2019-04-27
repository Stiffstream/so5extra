#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

using ask_reply_t = sync_ns::request_reply_t<int, int>;

class service_t final : public so_5::agent_t
	{
	public :
		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self().event( &service_t::on_request );
			}

	private :
		void
		on_request( typename ask_reply_t::request_mhood_t /*cmd*/ )
			{
				// We don't send the reply!
			}
	};

TEST_CASE( "close of reply_ch" )
{
	int result{};

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						auto svc = env.introduce_coop( [&](so_5::coop_t & coop) {
								return coop.make_agent<service_t>()->so_direct_mbox();
							} );

						auto ch = create_mchain(env);

						ask_reply_t::initiate_with_custom_reply_to(
								svc,
								ch,
								sync_ns::close_reply_chain,
								2 );

						so_5::receive( so_5::from(ch).handle_n(100),
								[&result]( typename ask_reply_t::reply_mhood_t cmd )
								{
									result += *cmd;
								} );

						env.stop();
					} );
		},
		5 );

	REQUIRE( result == 0 );
}

