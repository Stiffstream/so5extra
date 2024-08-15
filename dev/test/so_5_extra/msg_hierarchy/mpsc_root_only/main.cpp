#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/msg_hierarchy/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace hierarchy_ns = so_5::extra::msg_hierarchy;

using namespace std::chrono_literals;

namespace test
{

struct base_message : public hierarchy_ns::root_t<base_message>
	{
		base_message() = default;
	};

class a_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;

		const so_5::mbox_t m_sending_mbox;
	public:
		a_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_sending_mbox{ demuxer.sending_mbox() }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< base_message > >() )
					.event( [this]( mhood_t< so_5::mutable_msg< base_message > > ) {
							so_deregister_agent_coop_normally();
						} )
					;
			}

		void
		so_evt_start() override
			{
				so_5::send< so_5::mutable_msg< base_message > >( m_sending_mbox );
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "mpsc_root_only" )
{
	bool completed{ false };

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [](so_5::coop_t & coop) {
								hierarchy_ns::demuxer_t< base_message > demuxer{
										coop.environment(),
										so_5::mbox_type_t::multi_producer_single_consumer
									};
								coop.make_agent<a_receiver_t>( demuxer );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
}

