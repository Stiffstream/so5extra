#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/msg_hierarchy/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace hierarchy_ns = so_5::extra::msg_hierarchy;

using namespace std::chrono_literals;

namespace test
{

struct root_t : public so_5::message_t
	{
		root_t() = default;
	};

class a_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< root_t > m_consumer;

	public:
		a_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< root_t > & demuxer )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			{}

		void
		so_evt_start() override
			{
				so_deregister_agent_coop_normally();
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "simple shutdown on empty environment" )
{
	bool completed{ false };

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [](so_5::coop_t & coop) {
								hierarchy_ns::demuxer_t< root_t > demuxer;
								coop.make_agent<a_receiver_t>( demuxer );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
}

