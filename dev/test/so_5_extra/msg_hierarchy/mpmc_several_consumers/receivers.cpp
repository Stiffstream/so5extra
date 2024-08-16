#include "receivers.hpp"
#include "stopper.hpp"

namespace test
{

a_first_receiver_t::a_first_receiver_t(
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
a_first_receiver_t::so_define_agent()
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

void
a_first_receiver_t::on_data_message_two( mhood_t< data_message_two > /*cmd*/ )
	{
		m_trace += "two";
		so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
	}

void
a_first_receiver_t::on_data_message_one( mhood_t< data_message_one > /*cmd*/ )
	{
		m_trace += "one";
	}

void
a_first_receiver_t::on_base_message( mhood_t< base_message > /*cmd*/ )
	{
		m_trace += "base";
	}

a_second_receiver_t::a_second_receiver_t(
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
a_second_receiver_t::so_define_agent()
	{
		so_subscribe( m_consumer.receiving_mbox< data_message_one >() )
			.event( &a_second_receiver_t::on_data_message_one )
			;

		so_subscribe( m_consumer.receiving_mbox< base_message >() )
			.event( &a_second_receiver_t::on_base_message )
			;
	}

void
a_second_receiver_t::on_data_message_one( mhood_t< data_message_one > /*cmd*/ )
	{
		m_trace += "one";
		so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
	}

void
a_second_receiver_t::on_base_message( mhood_t< base_message > /*cmd*/ )
	{
		m_trace += "base";
	}

a_third_receiver_t::a_third_receiver_t(
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
a_third_receiver_t::so_define_agent()
	{
		so_subscribe( m_consumer.receiving_mbox< base_message >() )
			.event( &a_third_receiver_t::on_base_message )
			;
	}

void
a_third_receiver_t::on_base_message( mhood_t< base_message > /*cmd*/ )
	{
		m_trace += "base";
		so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
	}

a_forth_receiver_t::a_forth_receiver_t(
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
a_forth_receiver_t::so_define_agent()
	{
		so_subscribe( m_consumer.receiving_mbox< data_message_two >() )
			.event( &a_forth_receiver_t::on_data_message_two )
			;
	}

void
a_forth_receiver_t::on_data_message_two( mhood_t< data_message_two > /*cmd*/ )
	{
		m_trace += "two";
		so_5::send< a_stopper_t::msg_done >( m_stopper_mbox );
	}

} /* namespace test */

