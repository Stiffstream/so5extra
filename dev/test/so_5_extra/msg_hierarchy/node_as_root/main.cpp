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

struct intermediate_node
	: public base_message
	, public hierarchy_ns::node_t< intermediate_node, base_message >
	{
		intermediate_node()
			: hierarchy_ns::node_t< intermediate_node, base_message >{ *this }
			{}
	};

struct data_message_one
	: public intermediate_node
	, public hierarchy_ns::node_t< data_message_one, intermediate_node >
	{
		data_message_one()
			: hierarchy_ns::node_t< data_message_one, intermediate_node >{ *this }
			{}
	};

struct data_message_two
	: public intermediate_node
	, public hierarchy_ns::node_t< data_message_two, intermediate_node >
	{
		data_message_two()
			: hierarchy_ns::node_t< data_message_two, intermediate_node >{ *this }
			{}
	};

class a_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< intermediate_node > m_consumer;

		const so_5::mbox_t m_sending_mbox;
	public:
		a_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< intermediate_node > & demuxer )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_sending_mbox{ demuxer.sending_mbox() }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< data_message_one >() )
					.event( &a_receiver_t::on_data_message_one )
					;

				so_subscribe( m_consumer.receiving_mbox< intermediate_node >() )
					.event( &a_receiver_t::on_base_message )
					;
			}

		void
		so_evt_start() override
			{
				so_5::send< data_message_one >( m_sending_mbox );
			}

	public:
		void
		on_data_message_one( mhood_t< data_message_one > /*cmd*/ )
			{
				so_5::send< data_message_two >( m_sending_mbox );
			}

		void
		on_base_message( mhood_t< intermediate_node > /*cmd*/ )
			{
				so_deregister_agent_coop_normally();
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "mpmc_node_as_root" )
{
	bool completed{ false };

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [](so_5::coop_t & coop) {
								hierarchy_ns::demuxer_t< intermediate_node > demuxer{
										coop.environment(),
										hierarchy_ns::multi_consumer
									};
								coop.make_agent<a_receiver_t>( demuxer );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
}

