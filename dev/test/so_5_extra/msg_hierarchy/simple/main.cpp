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

struct data_message_one
	: public base_message
	, public hierarchy_ns::node_t< data_message_one, base_message >
	{
		data_message_one()
			: hierarchy_ns::node_t< data_message_one, base_message >( *this )
			{}
	};

struct data_message_two
	: public base_message
	, public hierarchy_ns::node_t< data_message_two, base_message >
	{
		data_message_two()
			: hierarchy_ns::node_t< data_message_two, base_message >( *this )
			{}
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
				so_subscribe( m_consumer.receiving_mbox< data_message_one >() )
					.event( &a_receiver_t::on_data_message_one )
					;

				so_subscribe( m_consumer.receiving_mbox< base_message >() )
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
		on_base_message( mhood_t< base_message > /*cmd*/ )
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
								hierarchy_ns::demuxer_t< base_message > demuxer{
										coop.environment(),
										so_5::mbox_type_t::multi_producer_multi_consumer
									};
								coop.make_agent<a_receiver_t>( demuxer );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
}

