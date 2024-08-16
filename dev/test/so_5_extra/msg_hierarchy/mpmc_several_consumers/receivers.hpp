#pragma once

#include "messages.hpp"

#include <so_5/all.hpp>

namespace hierarchy_ns = so_5::extra::msg_hierarchy;

namespace test
{

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
			so_5::mbox_t stopper_mbox );

		void
		so_define_agent() override;

	public:
		void
		on_data_message_two( mhood_t< data_message_two > /*cmd*/ );

		void
		on_data_message_one( mhood_t< data_message_one > /*cmd*/ );

		void
		on_base_message( mhood_t< base_message > /*cmd*/ );
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
			so_5::mbox_t stopper_mbox );

		void
		so_define_agent() override;

	public:
		void
		on_data_message_one( mhood_t< data_message_one > /*cmd*/ );

		void
		on_base_message( mhood_t< base_message > /*cmd*/ );
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
			so_5::mbox_t stopper_mbox );

		void
		so_define_agent() override;

	public:
		void
		on_base_message( mhood_t< base_message > /*cmd*/ );
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
			so_5::mbox_t stopper_mbox );

		void
		so_define_agent() override;

	public:
		void
		on_data_message_two( mhood_t< data_message_two > /*cmd*/ );
	};

} /* namespace test */

