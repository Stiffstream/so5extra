#pragma once

#include <so_5_extra/msg_hierarchy/pub.hpp>

namespace hierarchy_ns = so_5::extra::msg_hierarchy;

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

} /* namespace test */

