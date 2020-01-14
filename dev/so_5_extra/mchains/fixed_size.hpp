/*!
 * \file
 * \brief Implementation of fixed-size mchain.
 *
 * \since
 * v.1.4.0
 */

#pragma once

#include <so_5/impl/mchain_details.hpp>
#include <so_5/impl/make_mchain.hpp>
#include <so_5/impl/internal_env_iface.hpp>

#include <array>

namespace so_5
{

namespace extra
{

namespace mchains
{

namespace fixed_size
{

namespace details
{

//
// demand_queue_t
//
/*!
 * \brief Implementation of demands queue for fixed-size message chain with
 * "static" storage.
 *
 * \since
 * v.1.4.0
 */
template< std::size_t Size >
class demand_queue_t
	{
	public :
		// NOTE: constructor of this format is necessary
		// because the standard implementation of mchain from SO-5
		// requires it.
		demand_queue_t( so_5::mchain_props::capacity_t /*capacity*/ ) {}

		//! Is queue full?
		[[nodiscard]] bool
		is_full() const { return Size == m_size; }

		//! Is queue empty?
		[[nodiscard]] bool
		is_empty() const { return 0u == m_size; }

		//! Access to front item of the queue.
		[[nodiscard]] so_5::mchain_props::demand_t &
		front()
			{
				so_5::mchain_props::details::ensure_queue_not_empty( *this );
				return m_storage[ m_head ];
			}

		//! Remove the front item from queue.
		void
		pop_front()
			{
				so_5::mchain_props::details::ensure_queue_not_empty( *this );
				m_storage[ m_head ] = so_5::mchain_props::demand_t{};
				m_head = (m_head + 1u) % Size;
				--m_size;
			}

		//! Add a new item to the end of the queue.
		void
		push_back( so_5::mchain_props::demand_t && demand )
			{
				so_5::mchain_props::details::ensure_queue_not_full( *this );
				auto index = (m_head + m_size) % Size;
				m_storage[ index ] = std::move(demand);
				++m_size;
			}

		//! Size of the queue.
		std::size_t
		size() const { return m_size; }

	private :
		//! Queue's storage.
		std::array< so_5::mchain_props::demand_t, Size > m_storage;

		//! Index of the queue head.
		std::size_t m_head{ 0u };
		//! The current size of the queue.
		std::size_t m_size{ 0u };
	};

} /* namespace details */

//
// create_mchain
//
//FIXME: document this!
template< std::size_t Size >
[[nodiscard]] mchain_t
create_mchain(
	environment_t & env,
	so_5::mchain_props::overflow_reaction_t overflow_reaction )
	{
		so_5::impl::internal_env_iface_t env_iface{ env };

		return so_5::impl::make_mchain< details::demand_queue_t<Size> >(
				outliving_mutable( env_iface.msg_tracing_stuff() ),
				make_limited_without_waiting_mchain_params(
						// NOTE: this value won't be used.
						Size,
						// NOTE: this value won't be used.
						so_5::mchain_props::memory_usage_t::preallocated,
						overflow_reaction ),
				env,
				env_iface.allocate_mbox_id() );
	}

//
// create_mchain
//
//FIXME: document this!
template< std::size_t Size >
[[nodiscard]] mchain_t
create_mchain(
	environment_t & env,
	so_5::mchain_props::duration_t wait_timeout,
	so_5::mchain_props::overflow_reaction_t overflow_reaction )
	{
		so_5::impl::internal_env_iface_t env_iface{ env };

		return so_5::impl::make_mchain< details::demand_queue_t<Size> >(
				outliving_mutable( env_iface.msg_tracing_stuff() ),
				make_limited_with_waiting_mchain_params(
						// NOTE: this value won't be used.
						Size,
						// NOTE: this value won't be used.
						so_5::mchain_props::memory_usage_t::preallocated,
						overflow_reaction,
						wait_timeout ),
				env,
				env_iface.allocate_mbox_id() );
	}

//
// create_mchain
//
//FIXME: document this!
template< std::size_t Size >
[[nodiscard]] mchain_t
create_mchain(
	environment_t & env,
	const so_5::mchain_params_t & params )
	{
		so_5::impl::internal_env_iface_t env_iface{ env };

		return so_5::impl::make_mchain< details::demand_queue_t<Size> >(
				outliving_mutable( env_iface.msg_tracing_stuff() ),
				// NOTE: some of params's value won't be used.
				params,
				env,
				env_iface.allocate_mbox_id() );
	}

} /* namespace fixed_size */

} /* namespace mchains */

} /* namespace extra */

} /* namespace so_5 */

