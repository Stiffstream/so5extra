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

class a_first_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;
		const so_5::mbox_t m_sending_mbox;

	public:
		a_first_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			, m_sending_mbox{ demuxer.sending_mbox() }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< data_message_two > >() )
					.event( &a_first_receiver_t::on_data_message_two )
					;

				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< data_message_one > >() )
					.event( &a_first_receiver_t::on_data_message_one )
					;

				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< base_message > >() )
					.event( &a_first_receiver_t::on_base_message )
					;
			}

		void
		so_evt_start() override
			{
				try
					{
						so_5::send< so_5::mutable_msg< data_message_two > >( m_sending_mbox );

						m_trace = "MESSAGE SENT";
					}
				catch( const so_5::exception_t & x )
					{
						if( hierarchy_ns::errors::rc_more_than_one_subscriber_for_mutable_msg
								== x.error_code() )
							m_trace = "OK";
						else
							m_trace = "FAIL: " + std::to_string( x.error_code() );
					}

				so_deregister_agent_coop_normally();
			}

	public:
		void
		on_data_message_two( mutable_mhood_t< data_message_two > /*cmd*/ )
			{
				m_trace += "two";
			}

		void
		on_data_message_one( mutable_mhood_t< data_message_one > /*cmd*/ )
			{
				m_trace += "one";
			}

		void
		on_base_message( mutable_mhood_t< base_message > /*cmd*/ )
			{
				m_trace += "base";
			}
	};

class a_second_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;

	public:
		a_second_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< data_message_one > >() )
					.event( &a_second_receiver_t::on_data_message_one )
					;

				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< base_message > >() )
					.event( &a_second_receiver_t::on_base_message )
					;
			}

	public:
		void
		on_data_message_one( mutable_mhood_t< data_message_one > /*cmd*/ )
			{
				m_trace += "one";
			}

		void
		on_base_message( mutable_mhood_t< base_message > /*cmd*/ )
			{
				m_trace += "base";
			}
	};

class a_third_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;

	public:
		a_third_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< base_message > >() )
					.event( &a_third_receiver_t::on_base_message )
					;
			}

	public:
		void
		on_base_message( mutable_mhood_t< base_message > /*cmd*/ )
			{
				m_trace += "base";
			}
	};

class a_forth_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;
		std::string & m_trace;

	public:
		a_forth_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< so_5::mutable_msg< data_message_two > >() )
					.event( &a_forth_receiver_t::on_data_message_two )
					;
			}

	public:
		void
		on_data_message_two( mutable_mhood_t< data_message_two > /*cmd*/ )
			{
				m_trace += "two";
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
								hierarchy_ns::demuxer_t< base_message > demuxer{
										coop.environment(),
										hierarchy_ns::single_consumer
									};

								coop.make_agent<a_first_receiver_t>( demuxer, trace_first );
								coop.make_agent<a_second_receiver_t>( demuxer, trace_second );
								coop.make_agent<a_third_receiver_t>( demuxer, trace_third );
								coop.make_agent<a_forth_receiver_t>( demuxer, trace_forth );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
	REQUIRE( "OK" == trace_first );
	REQUIRE( trace_second.empty() );
	REQUIRE( trace_third.empty() );
	REQUIRE( trace_forth.empty() );
}

