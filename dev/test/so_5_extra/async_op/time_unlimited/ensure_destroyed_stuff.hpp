#pragma once

#include <atomic>

class test_op_data_t
	:	public so_5::extra::async_op::time_unlimited::details::op_data_t
	{
		friend class ::so_5::intrusive_ptr_t< test_op_data_t >;

		using base_type_t = so_5::extra::async_op::time_unlimited::details::op_data_t;

		static std::atomic<int> m_live_items;

	public :
		test_op_data_t(
			::so_5::outliving_reference_t< ::so_5::agent_t > owner )
			:	base_type_t( owner )
			{
				++m_live_items;
			}
		~test_op_data_t()
			{
				--m_live_items;
			}

		static int live_items() noexcept {
			return m_live_items.load( std::memory_order_acquire );
		}
	};

std::atomic<int> test_op_data_t::m_live_items{ 0 };

