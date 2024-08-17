/*
 * This test should work the same way as MPMC variant.
 */
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
	: public data_message_one
	, public hierarchy_ns::node_t< data_message_two, data_message_one >
	{
		data_message_two()
			: hierarchy_ns::node_t< data_message_two, data_message_one >( *this )
			{}
	};

class a_stopper_t final : public so_5::agent_t
	{
		const unsigned m_required_stops;
		unsigned m_received_stops{};

	public:
		struct msg_done final : public so_5::signal_t {};

		a_stopper_t( context_t ctx, unsigned required_stops )
			: so_5::agent_t{ std::move(ctx) }
			, m_required_stops{ required_stops }
			{}

		void
		so_define_agent() override
			{
				so_subscribe_self().event( &a_stopper_t::evt_done );
			}

	private:
		void
		evt_done( mhood_t<msg_done> )
			{
				if( ++m_received_stops >= m_required_stops )
					so_deregister_agent_coop_normally();
			}
	};

class a_sender_t final : public so_5::agent_t
	{
		const so_5::mbox_t m_stopper_mbox;
		const so_5::mbox_t m_sending_mbox;

	public:
		a_sender_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			so_5::mbox_t stopper_mbox )
			: so_5::agent_t{ std::move(ctx) }
			, m_stopper_mbox{ std::move(stopper_mbox) }
			, m_sending_mbox{ demuxer.sending_mbox() }
			{}

		void
		so_evt_start() override
			{
				so_5::send< data_message_two >( m_sending_mbox );
				so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
			}
	};

class a_first_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;
		const so_5::mbox_t m_stopper_mbox;

	public:
		a_first_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace,
			so_5::mbox_t stopper_mbox )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			, m_stopper_mbox{ std::move(stopper_mbox) }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< data_message_two >() )
					.event( &a_first_receiver_t::on_data_message_two )
					;

				so_subscribe( m_consumer.receiving_mbox< data_message_one >() )
					.event( &a_first_receiver_t::on_data_message_one )
					;

				so_subscribe( m_consumer.receiving_mbox< base_message >() )
					.event( &a_first_receiver_t::on_base_message )
					;
			}

	public:
		void
		on_data_message_two( mhood_t< data_message_two > /*cmd*/ )
			{
				m_trace += "two";
				so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
			}

		void
		on_data_message_one( mhood_t< data_message_one > /*cmd*/ )
			{
				m_trace += "one";
			}

		void
		on_base_message( mhood_t< base_message > /*cmd*/ )
			{
				m_trace += "base";
			}
	};

class a_second_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;
		const so_5::mbox_t m_stopper_mbox;

	public:
		a_second_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace,
			so_5::mbox_t stopper_mbox )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			, m_stopper_mbox{ std::move(stopper_mbox) }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< data_message_one >() )
					.event( &a_second_receiver_t::on_data_message_one )
					;

				so_subscribe( m_consumer.receiving_mbox< base_message >() )
					.event( &a_second_receiver_t::on_base_message )
					;
			}

	public:
		void
		on_data_message_one( mhood_t< data_message_one > /*cmd*/ )
			{
				m_trace += "one";
				so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
			}

		void
		on_base_message( mhood_t< base_message > /*cmd*/ )
			{
				m_trace += "base";
			}
	};

class a_third_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;
		const so_5::mbox_t m_stopper_mbox;

	public:
		a_third_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace,
			so_5::mbox_t stopper_mbox )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			, m_stopper_mbox{ std::move(stopper_mbox) }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< base_message >() )
					.event( &a_third_receiver_t::on_base_message )
					;
			}

	public:
		void
		on_base_message( mhood_t< base_message > /*cmd*/ )
			{
				m_trace += "base";
				so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
			}
	};

class a_forth_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;
		const so_5::mbox_t m_stopper_mbox;

	public:
		a_forth_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace,
			so_5::mbox_t stopper_mbox )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			, m_stopper_mbox{ std::move(stopper_mbox) }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< data_message_two >() )
					.event( &a_forth_receiver_t::on_data_message_two )
					;
			}

	public:
		void
		on_data_message_two( mhood_t< data_message_two > /*cmd*/ )
			{
				m_trace += "two";
				so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "mpsc_several_consumers_1" )
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
										hierarchy_ns::single_consumer
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

