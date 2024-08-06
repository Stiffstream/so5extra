/*!
 * @file
 * @brief Support for hierarchy of message types.
 *
 * @since v.1.6.2
 */

#pragma once

#include <so_5/environment.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

namespace so_5::extra::msg_hierarchy
{

namespace impl
{

/*!
 * @brief Type of numeric ID for consumer_id.
 */
using consumer_numeric_id_t = std::uint_least64_t;

//! Special value that means that a consumer_id is not valid.
inline constexpr consumer_numeric_id_t invalid_consumer_id{ 0 };

//
// demuxing_data_iface_t
//
//FIXME: document this!
template< typename Root >
class demuxing_data_iface_t : public ::so_5::atomic_refcounted_t
	{
	public:
		virtual ~demuxing_data_iface_t() noexcept = default;

		virtual void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept = 0;
	};

//
// demuxing_data_iface_shptr_t
//
//FIXME: document this!
template< typename Root >
using demuxing_data_iface_shptr_t =
		::so_5::intrusive_ptr_t< demuxing_data_iface_t< Root > >;

//
// demuxing_data_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class demuxing_data_t : public demuxing_data_iface_t< Root >
	{
		Lock_Type m_lock;

		consumer_numeric_id_t m_consumer_id_counter{};

	public:
		[[nodiscard]] consumer_numeric_id_t
		acquire_new_consumer_id()
			{
				std::lock_guard< Lock_Type > lock{ m_lock };

				return ++m_consumer_id_counter;
			}

		virtual void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept override
			{
				//FIXME: implement this!
				std::cout << "consumer_destroyed: " << id << std::endl;
			}
	};

//
// demuxing_data_shptr_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
using demuxing_data_shptr_t =
		::so_5::intrusive_ptr_t< demuxing_data_t< Root, Lock_Type > >;

} /* namespace impl */

//
// default_traits_t
//
//FIXME: document this!
struct default_traits_t
	{
	};

//
// consumer_t forward decl.
//
template< typename Root >
class consumer_t;

//
// demuxer_t definition
//
//FIXME: document this!
template<
	typename Root,
	typename Lock_Type = std::mutex,
	typename Traits = default_traits_t >
class demuxer_t
	{
		impl::demuxing_data_shptr_t< Root, Lock_Type > m_data;

//FIXME: there should be sending_mbox.

	public:
		friend void
		swap( demuxer_t & a, demuxer_t & b ) noexcept
			{
				using std::swap;

				swap( a.m_data, b.m_data );
			}

		demuxer_t()
			: m_data{ new impl::demuxing_data_t< Root, Lock_Type >{} }
			{}

		demuxer_t( const demuxer_t & ) = delete;
		demuxer_t &
		operator=( const demuxer_t & ) = delete;

		demuxer_t( demuxer_t && o ) noexcept
			: m_data{ std::exchange( o.m_data, impl::demuxing_data_shptr_t< Root >{} ) }
			{}
		demuxer_t &
		operator=( demuxer_t && o ) noexcept
			{
				demuxer_t tmp{ std::move(o) };
				swap( *this, tmp );
				return *this;
			}

		[[nodiscard]] consumer_t< Root >
		allocate_consumer();
	};

//
// consumer_t
//
//FIXME: document this!
template< typename Root >
class consumer_t
	{
		template< typename Root, typename Lock_Type, typename Traits >
		friend class demuxer_t;

		impl::demuxing_data_iface_shptr_t< Root > m_data;

		impl::consumer_numeric_id_t m_id;

		consumer_t(
			impl::demuxing_data_iface_shptr_t< Root > data,
			impl::consumer_numeric_id_t id )
			: m_data{ std::move(data) }
			, m_id{ id }
			{}

	public:
		friend void
		swap( consumer_t & a, consumer_t & b ) noexcept
			{
				using std::swap;

				swap( a.m_data, b.m_data );
				swap( a.m_id, b.m_id );
			}

		~consumer_t()
			{
				if( m_data )
					{
						m_data->consumer_destroyed( m_id );
					}
			}

		consumer_t( const consumer_t & ) = delete;
		consumer_t &
		operator=( const consumer_t & ) = delete;

		consumer_t( consumer_t && o ) noexcept
			: m_data{ std::exchange( o.m_data, impl::default_distribution_period< Root >{} ) }
			, m_id{ std::exchange( o.m_id, impl::invalid_consumer_id ) }
			{}

		consumer_t &
		operator=( consumer_t && o ) noexcept
			{
				consumer_t tmp{ std::move(o) };
				swap( *this, tmp );
				return *this;
			}
	};

//
// demuxer_t implementation
//

template< typename Root, typename Lock_Type, typename Traits >
[[nodiscard]] consumer_t< Root >
demuxer_t< Root, Lock_Type, Traits >::allocate_consumer()
	{
		return consumer_t< Root >{
				m_data,
				m_data->acquire_new_consumer_id()
			};
	}

} /* namespace so_5::extra::msg_hierarchy */

