/*!
 * @file
 * @brief Support for hierarchy of message types.
 *
 * @since v.1.6.2
 */

#pragma once

#include <so_5/version.hpp>

#if SO_5_VERSION < SO_5_VERSION_MAKE(5u, 8u, 0u)
#error "SObjectizer-5.8.0 of newest is required"
#endif

#include <so_5/unique_subscribers_mbox.hpp>
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
class demuxing_data_iface_t : public ::so_5::atomic_refcounted_t
	{
	public:
		virtual ~demuxing_data_iface_t() noexcept = default;

		[[nodiscard]] virtual ::so_5::environment_t &
		environment() const noexcept = 0;

		virtual void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept = 0;

		[[nodiscard]] virtual ::so_5::mbox_t
		acquire_receiving_mbox_for(
			consumer_numeric_id_t id,
			const std::type_index & msg_type ) = 0;

		virtual void
		do_deliver_message(
			//! Can the delivery blocks the current thread?
			message_delivery_mode_t delivery_mode,
			//! Type of the message to deliver.
			const std::type_index & msg_type,
			//! A message instance to be delivered.
			const message_ref_t & message,
			//! Current deep of overlimit reaction recursion.
			unsigned int redirection_deep ) = 0;
	};

//
// demuxing_data_iface_shptr_t
//
//FIXME: document this!
using demuxing_data_iface_shptr_t =
		::so_5::intrusive_ptr_t< demuxing_data_iface_t >;

//
// basic_demuxing_data_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class basic_demuxing_data_t : public demuxing_data_iface_t
	{
	protected:
		::so_5::environment_t & m_env;

		Lock_Type m_lock;

		const ::so_5::mbox_type_t m_mbox_type;

		consumer_numeric_id_t m_consumer_id_counter{};

	public:
		basic_demuxing_data_t(
			::so_5::outliving_reference_t< ::so_5::environment_t > env,
			::so_5::mbox_type_t mbox_type )
			: m_env{ env.get() }
			, m_mbox_type{ mbox_type }
			{}

		[[nodiscard]] ::so_5::mbox_type_t
		mbox_type() const noexcept
			{
				return m_mbox_type;
			}

		[[nodiscard]] consumer_numeric_id_t
		acquire_new_consumer_id()
			{
				std::lock_guard< Lock_Type > lock{ m_lock };

				return ++m_consumer_id_counter;
			}

		[[nodiscard]] ::so_5::environment_t &
		environment() const noexcept override
			{
				return m_env;
			}

		//FIXME: this implementation is not needed in this class.
		//It's here for debugging purposes only.
		void
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
		::so_5::intrusive_ptr_t< basic_demuxing_data_t< Root, Lock_Type > >;

//
// mpmc_demuxing_data_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class multi_consumer_demuxing_data_t : public basic_demuxing_data_t< Root, Lock_Type >
	{
		using base_type_t = basic_demuxing_data_t< Root, Lock_Type >;

		using one_consumer_mboxes_map_t = std::map< std::type_index, ::so_5::mbox_t >;

		using consumers_map_t = std::map<
				consumer_numeric_id_t,
				one_consumer_mboxes_map_t
			>;

		consumers_map_t m_consumers_with_mboxes;

	public:
		multi_consumer_demuxing_data_t(
			::so_5::outliving_reference_t< ::so_5::environment_t > env )
			: base_type_t{ env, ::so_5::mbox_type_t::multi_producer_multi_consumer }
			{}

		[[nodiscard]] so_5::mbox_t
		acquire_receiving_mbox_for(
			consumer_numeric_id_t id,
			const std::type_index & msg_type ) override
			{
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				auto [it_consumer, _] = m_consumers_with_mboxes.emplace(
						id, one_consumer_mboxes_map_t{} );
				auto & consumer_map = it_consumer->second;

				auto it_msg = consumer_map.find( msg_type );
				if( it_msg == consumer_map.end() )
					{
						it_msg = consumer_map.emplace(
								msg_type,
								this->m_env.create_mbox() ).first;
					}

				return it_msg->second;
			}

		void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept override
			{
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				m_consumers_with_mboxes.erase( id );
			}

		void
		do_deliver_message(
			message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int redirection_deep ) override
			{
				//FIXME: the message has to be checked for immutability!

				//FIXME: should a reader-writer lock be used here?
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				//FIXME: actual searching algorithm has to be used here!
				for( const auto & [id, consumer_map] : m_consumers_with_mboxes )
					{
						if( const auto it = consumer_map.find( msg_type );
								it != consumer_map.end() )
							{
								it->second->do_deliver_message(
										delivery_mode,
										msg_type,
										message,
										redirection_deep );
							}
					}
			}
	};

//
// mpsc_demuxing_data_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class single_consumer_demuxing_data_t : public basic_demuxing_data_t< Root, Lock_Type >
	{
		using base_type_t = basic_demuxing_data_t< Root, Lock_Type >;

	public:
		single_consumer_demuxing_data_t(
			::so_5::outliving_reference_t< ::so_5::environment_t > env )
			: base_type_t{ env, ::so_5::mbox_type_t::multi_producer_single_consumer }
			{}

		[[nodiscard]] so_5::mbox_t
		acquire_receiving_mbox_for(
			consumer_numeric_id_t id,
			const std::type_index & msg_type )
			{
				throw std::runtime_error{ "Not implemented!" };
			}

		void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept override
			{
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				//FIXME: implement this!
			}

		void
		do_deliver_message(
			message_delivery_mode_t /*delivery_mode*/,
			const std::type_index & /*msg_type*/,
			const message_ref_t & /*message*/,
			unsigned int /*redirection_deep*/ ) override
			{
				//FIXME: implement this!
			}
	};

//
// multi_consumer_sending_mbox_t
//
//FIXME: document this!
template< typename Root >
class multi_consumer_sending_mbox_t final
	: public ::so_5::abstract_message_box_t
	{
		demuxing_data_iface_shptr_t m_data;

		const ::so_5::mbox_id_t m_id;

	public:
		multi_consumer_sending_mbox_t(
			demuxing_data_iface_shptr_t data,
			::so_5::mbox_id_t id )
			: m_data{ std::move(data) }
			, m_id{ id }
			{}

		//FIXME: has to be removed after debugging!
		~multi_consumer_sending_mbox_t() override
			{
				std::cout << "~multi_consumer_sending_mbox_t" << std::endl;
			}

		::so_5::mbox_id_t
		id() const override
			{
				return m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & /*type_index*/,
			::so_5::abstract_message_sink_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION( ::so_5::rc_not_implemented,
						"subscribe_event_handler is not supported for this type of mbox" );
			}

		void
		unsubscribe_event_handler(
			const std::type_index & /*type_index*/,
			abstract_message_sink_t & /*subscriber*/ ) noexcept override
			{
				// Nothing to do.
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=MSG_HIERARCHY_SENDING_MPMC:root="
						<< typeid(Root).name()
						<< ":id=" << m_id << ">";
				return s.str();
			}

		::so_5::mbox_type_t
		type() const override
			{
				return ::so_5::mbox_type_t::multi_producer_multi_consumer;
			}

		void
		do_deliver_message(
			message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int redirection_deep ) override
			{
				m_data->do_deliver_message(
						delivery_mode,
						msg_type,
						message,
						redirection_deep );
			}

		void
		set_delivery_filter(
			const std::type_index & /*msg_type*/,
			const delivery_filter_t & /*filter*/,
			abstract_message_sink_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION( ::so_5::rc_not_implemented,
						"set_delivery_filter is not supported for this type of mbox" );
			}

		virtual void
		drop_delivery_filter(
			const std::type_index & msg_type,
			abstract_message_sink_t & subscriber ) noexcept override
			{
				// Nothing to do.
			}

		[[nodiscard]]
		so_5::environment_t &
		environment() const noexcept override
			{
				return m_data->environment();
			}
	};

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

		::so_5::mbox_t m_sending_mbox;

		[[nodiscard]] static impl::demuxing_data_shptr_t< Root, Lock_Type >
		make_required_demuxing_data_object(
			::so_5::outliving_reference_t< ::so_5::environment_t > env,
			::so_5::mbox_type_t mbox_type )
			{
				using result_t = impl::demuxing_data_shptr_t< Root, Lock_Type >;

				result_t result;
				switch( mbox_type )
					{
					case ::so_5::mbox_type_t::multi_producer_multi_consumer:
						{
							using data_t = impl::multi_consumer_demuxing_data_t<
									Root, Lock_Type >;

							result = result_t{ new data_t{ env } };
						}
					break;

					case ::so_5::mbox_type_t::multi_producer_single_consumer:
						{
							using data_t = impl::single_consumer_demuxing_data_t<
									Root, Lock_Type >;

							result = result_t{ new data_t{ env } };
						}
					break;
					}
				return result;
			}

		[[nodiscard]] static ::so_5::mbox_t
		make_required_sending_mbox(
			impl::demuxing_data_iface_shptr_t data,
			::so_5::environment_t & env,
			::so_5::mbox_type_t mbox_type )
			{
				const auto mbox_id = ::so_5::impl::internal_env_iface_t{ env }
						.allocate_mbox_id();

				so_5::mbox_t result;
				switch( mbox_type )
					{
					case ::so_5::mbox_type_t::multi_producer_multi_consumer:
						result = so_5::mbox_t{
								new impl::multi_consumer_sending_mbox_t< Root >(
										std::move(data),
										mbox_id )
							};
					break;

					case ::so_5::mbox_type_t::multi_producer_single_consumer:
						//FIXME: implement this!
					break;
					}

				return result;
			}

	public:
		friend void
		swap( demuxer_t & a, demuxer_t & b ) noexcept
			{
				using std::swap;

				swap( a.m_data, b.m_data );
				swap( a.m_sending_mbox, b.m_sending_mbox );
			}

		demuxer_t(
			::so_5::environment_t & env,
			::so_5::mbox_type_t mbox_type )
			: m_data{
					make_required_demuxing_data_object(
						::so_5::outliving_mutable( env ),
						mbox_type )
				}
			, m_sending_mbox{
					make_required_sending_mbox( m_data, env, mbox_type )
				}
			{}

		demuxer_t( const demuxer_t & ) = delete;
		demuxer_t &
		operator=( const demuxer_t & ) = delete;

		demuxer_t( demuxer_t && o ) noexcept
			: m_data{ std::exchange( o.m_data, impl::demuxing_data_shptr_t< Root >{} ) }
			, m_sending_mbox{ std::exchange( o.m_sending_mbox, ::so_5::mbox_t{} ) }
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

		[[nodiscard]] ::so_5::mbox_t
		sending_mbox() const noexcept
			{
				return m_sending_mbox;
			}
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

		impl::demuxing_data_iface_shptr_t m_data;

		impl::consumer_numeric_id_t m_id;

		consumer_t(
			impl::demuxing_data_iface_shptr_t data,
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

		template< typename Msg_Type >
		[[nodiscard]] so_5::mbox_t
		receiving_mbox()
			{
				//FIXME: Msg_Type has to be Root type or should be
				//derived from Root type.

				//FIXME: mutability of the message has to be checked.

				return m_data->acquire_receiving_mbox_for(
						m_id,
						::so_5::message_payload_type< Msg_Type >::subscription_type_index() );
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

