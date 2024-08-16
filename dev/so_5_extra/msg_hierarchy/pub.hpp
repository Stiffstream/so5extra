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

#include <so_5_extra/error_ranges.hpp>

#include <so_5/unique_subscribers_mbox.hpp>
#include <so_5/environment.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <shared_mutex>
#include <type_traits>

namespace so_5::extra::msg_hierarchy
{

namespace errors
{

/*!
 * @brief An attempt to get parent upcaster but it isn't exists.
 *
 * @since v.1.6.2
 */
const int rc_no_parent_upcaster =
		so_5::extra::errors::msg_hierarchy_errors;

/*!
 * @brief An attempt to deliver signal via msg_hierarchy-related mbox.
 *
 * msg_hierarchy and signals are incompatible.
 *
 * @since v.1.6.2
 */
const int rc_signal_cannot_be_delivered =
		so_5::extra::errors::msg_hierarchy_errors + 1;

/*!
 * @brief An attempt to deliver a message that type is not derived from root.
 *
 * @since v.1.6.2
 */
const int rc_message_is_not_derived_from_root =
		so_5::extra::errors::msg_hierarchy_errors + 2;

/*!
 * @brief An attempt to create receiving mbox for a mutable message.
 *
 * @since v.1.6.2
 */
const int rc_mpmc_demuxer_cannot_handler_mutable_msg =
		so_5::extra::errors::msg_hierarchy_errors + 3;

/*!
 * @brief There are more than one subscriber for a mutable message.
 *
 * A demuxer can't deliver an instance of a mutable message if there are
 * more than one subscriber for this message.
 *
 * @since v.1.6.2
 */
const int rc_more_than_one_subscriber_for_mutable_msg =
		so_5::extra::errors::msg_hierarchy_errors + 4;

} /* namespace errors */

namespace impl
{

class message_upcaster_t;

//
// upcaster_factory_t
//
//FIXME: document this!
using upcaster_factory_t =
	message_upcaster_t (*)(::so_5::message_mutability_t) noexcept;

//
// message_upcaster_t
//
//FIXME: document this!
class message_upcaster_t
	{
		std::type_index m_self_type;

		upcaster_factory_t m_parent_factory;

	public:
		message_upcaster_t(
			std::type_index self_type,
			upcaster_factory_t parent_factory )
			: m_self_type{ std::move(self_type) }
			, m_parent_factory{ parent_factory }
			{}

		[[nodiscard]] const std::type_index &
		self_type() const noexcept
			{
				return m_self_type;
			}

		[[nodiscard]] bool
		has_parent_factory() const noexcept
			{
				return nullptr != m_parent_factory;
			}

		[[nodiscard]] message_upcaster_t
		parent_upcaster( message_mutability_t mutability ) const
			{
				if( !has_parent_factory() )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::msg_hierarchy::errors::rc_no_parent_upcaster,
							"no parent upcaster_factory" );

				return (*m_parent_factory)( mutability );
			}
	};

//
// root_base_t
//
class root_base_t : public message_t
	{
		upcaster_factory_t m_factory{};

	public:
		[[nodiscard]] upcaster_factory_t
		so_message_upcaster_factory() const noexcept
			{
				return m_factory;
			}

		void
		so_set_message_upcaster_factory(
			upcaster_factory_t factory) noexcept
			{
				m_factory = factory;
			}
	};

} /* namespace impl */

template<typename Base>
class root_t : public impl::root_base_t
	{
		static_assert( !::so_5::is_mutable_message< Base >::value,
				"the Base can't be mutable_msg<T>" );

	public:
		[[nodiscard]] static impl::message_upcaster_t
		so_make_upcaster_root( ::so_5::message_mutability_t mutability ) noexcept
			{
				if( ::so_5::message_mutability_t::mutable_message == mutability )
					return { typeid(::so_5::mutable_msg<Base>), nullptr };
				else
					return { typeid(Base), nullptr };
			}

		root_t()
			{
				this->so_set_message_upcaster_factory(
						&root_t::so_make_upcaster_root );
			}
	};

namespace impl
{

template< typename B, typename = std::void_t<> >
struct has_so_make_upcaster_method : public std::false_type {};

template< typename B >
struct has_so_make_upcaster_method<
	B, std::void_t< decltype(&B::so_make_upcaster) >
> : public std::true_type {};

} /* namespace impl */

//
// node_t
//
//FIXME: document this!
template<typename Derived, typename Base>
class node_t
	{
		static_assert( !::so_5::is_mutable_message< Base >::value,
				"the Base can't be mutable_msg<T>" );

		static_assert( !::so_5::is_mutable_message< Derived >::value,
				"the Derived can't be mutable_msg<T>" );

	public:
		[[nodiscard]] static impl::message_upcaster_t
		so_make_upcaster( message_mutability_t mutability ) noexcept
			{
				if constexpr( impl::has_so_make_upcaster_method<Base>::value )
					{
						const auto upcaster = &Base::so_make_upcaster;
						if( ::so_5::message_mutability_t::mutable_message == mutability )
							return { typeid(::so_5::mutable_msg<Derived>), upcaster };
						else
							return { typeid(Derived), upcaster };
					}
				else
					{
						const auto upcaster = &Base::so_make_upcaster_root;
						if( ::so_5::message_mutability_t::mutable_message == mutability )
							return { typeid(::so_5::mutable_msg<Derived>), upcaster };
						else
							return { typeid(Derived), upcaster };
					}
			}

		node_t( Derived & derived )
			{
				static_assert(
						std::is_base_of_v<impl::root_base_t, Derived> &&
						std::is_base_of_v<Base, Derived> );

				derived.so_set_message_upcaster_factory( &node_t::so_make_upcaster );
			}
	};

namespace impl
{

/*!
 * @brief Type of numeric ID for consumer_id.
 */
using consumer_numeric_id_t = std::uint_least64_t;

//! Special value that means that a consumer_id is not valid.
inline constexpr consumer_numeric_id_t invalid_consumer_id{ 0 };

//
// demuxing_controller_iface_t
//
//FIXME: document this!
class demuxing_controller_iface_t : public ::so_5::atomic_refcounted_t
	{
	public:
		virtual ~demuxing_controller_iface_t() noexcept = default;

		[[nodiscard]]
		virtual ::so_5::environment_t &
		environment() const noexcept = 0;

		virtual void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept = 0;

		[[nodiscard]]
		virtual ::so_5::mbox_type_t
		mbox_type() const noexcept = 0;

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
// demuxing_controller_iface_shptr_t
//
//FIXME: document this!
using demuxing_controller_iface_shptr_t =
		::so_5::intrusive_ptr_t< demuxing_controller_iface_t >;

//
// basic_demuxing_controller_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class basic_demuxing_controller_t : public demuxing_controller_iface_t
	{
	protected:
		::so_5::environment_t & m_env;

		Lock_Type m_lock;

		const ::so_5::mbox_type_t m_mbox_type;

		consumer_numeric_id_t m_consumer_id_counter{};

	public:
		basic_demuxing_controller_t(
			::so_5::outliving_reference_t< ::so_5::environment_t > env,
			::so_5::mbox_type_t mbox_type )
			: m_env{ env.get() }
			, m_mbox_type{ mbox_type }
			{}

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

		[[nodiscard]] ::so_5::mbox_type_t
		mbox_type() const noexcept override
			{
				return m_mbox_type;
			}
	};

//
// demuxing_controller_shptr_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
using demuxing_controller_shptr_t =
		::so_5::intrusive_ptr_t< basic_demuxing_controller_t< Root, Lock_Type > >;

//
// controller_consumers_mixin_t
//
/*!
 * @brief Helper type to be used as mixin for actual demuxing controllers.
 *
 * Contains map of consumers and implements delivery procedure for
 * immutable messages.
 */
struct controller_consumers_mixin_t
	{
		//! Type of map of mboxes for one consumer.
		using one_consumer_mboxes_map_t = std::map< std::type_index, ::so_5::mbox_t >;

		//! Type of map of all consumers.
		using consumers_map_t = std::map<
				consumer_numeric_id_t,
				one_consumer_mboxes_map_t
			>;

		//! Map of all consumers.
		consumers_map_t m_consumers_with_mboxes;

		/*!
		 * @brief Perform delivery of the message.
		 *
		 * It's assumed that all necessary check have been performed earlier.
		 */
		void
		do_delivery_procedure_for_immutable_message(
			//! How message has to be delivered.
			::so_5::message_delivery_mode_t delivery_mode,
			//! Message to be delivered.
			const ::so_5::message_ref_t & message,
			//! Redirection deep for overload control.
			unsigned int redirection_deep,
			//! The pointer to the root of the hierarchy for this message.
			const root_base_t * root )
			{
				const auto msg_mutabilty_flag = message_mutability( *root );

				// Main delivery loop.
				for( const auto & [id, consumer_map] : m_consumers_with_mboxes )
					{
						// Try to deliver message by its actual type and them
						// trying to going hierarchy up.
						auto upcaster = root->so_message_upcaster_factory()(
								msg_mutabilty_flag );

						bool delivery_finished = false;
						do
						{
							const auto type_to_find = upcaster.self_type();
							if( const auto it = consumer_map.find( type_to_find );
									it != consumer_map.end() )
								{
									// Only one delivery for every consumer.
									delivery_finished = true;
									it->second->do_deliver_message(
											delivery_mode,
											type_to_find,
											message,
											redirection_deep );
								}
							else
								{
									delivery_finished = !upcaster.has_parent_factory();
									if( !delivery_finished )
										{
											// It's not the root yet, try to go one level up.
											upcaster = upcaster.parent_upcaster( msg_mutabilty_flag );
										}
								}
						} while( !delivery_finished );
					}
			}
	};

//
// mpmc_demuxing_controller_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class multi_consumer_demuxing_controller_t final
	: public basic_demuxing_controller_t< Root, Lock_Type >
	, private controller_consumers_mixin_t
	{
		using base_type_t = basic_demuxing_controller_t< Root, Lock_Type >;

	public:
		multi_consumer_demuxing_controller_t(
			::so_5::outliving_reference_t< ::so_5::environment_t > env )
			: base_type_t{ env, ::so_5::mbox_type_t::multi_producer_multi_consumer }
			{}

		[[nodiscard]] so_5::mbox_t
		acquire_receiving_mbox_for(
			consumer_numeric_id_t id,
			const std::type_index & msg_type ) override
			{
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				auto [it_consumer, _] = this->m_consumers_with_mboxes.emplace(
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

				this->m_consumers_with_mboxes.erase( id );
			}

		void
		do_deliver_message(
			::so_5::message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const ::so_5::message_ref_t & message,
			unsigned int redirection_deep ) override
			{
				namespace err_ns = ::so_5::extra::msg_hierarchy::errors;

				// Do all necessary checks first...
				if( ::so_5::message_mutability_t::immutable_message !=
						message_mutability( message ) )
					SO_5_THROW_EXCEPTION(
							::so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
							"an attempt to deliver mutable message via MPMC mbox"
							", msg_type=" + std::string(msg_type.name()) );

				const ::so_5::message_t * raw_msg = message.get();
				if( !raw_msg )
					SO_5_THROW_EXCEPTION(
							err_ns::rc_signal_cannot_be_delivered,
							"signal can't be handled by msg_hierarchy's demuxer" );

				const root_base_t * root = dynamic_cast<const root_base_t *>(raw_msg);
				if( !root )
					SO_5_THROW_EXCEPTION(
							err_ns::rc_message_is_not_derived_from_root,
							"a message type has to be derived from root_t" );

				// ...the object has to be locked for the delivery procedure...
				std::shared_lock< Lock_Type > lock{ this->m_lock };

				// ...now the message can be delivered.
				this->do_delivery_procedure_for_immutable_message(
						delivery_mode,
						message,
						redirection_deep,
						root );
			}
	};

//
// single_dest_info_t
//
/*!
 * @brief Information about a single destination for a mutable message.
 */
struct single_dest_info_t
	{
		//! Mbox to be used for delivery.
		so_5::mbox_t m_dest_mbox;

		//! Subscription type to be used for delivery.
		std::type_index m_subscription_type;

		//! Initializing constructor.
		single_dest_info_t(
			so_5::mbox_t dest_mbox,
			std::type_index subscription_type )
			: m_dest_mbox{ std::move(dest_mbox) }
			, m_subscription_type{ std::move(subscription_type) }
			{}
	};

//
// mpsc_demuxing_controller_t
//
//FIXME: document this!
template< typename Root, typename Lock_Type >
class single_consumer_demuxing_controller_t final
	: public basic_demuxing_controller_t< Root, Lock_Type >
	, private controller_consumers_mixin_t
	{
		using base_type_t = basic_demuxing_controller_t< Root, Lock_Type >;

	public:
		//! Initializing constructor.
		single_consumer_demuxing_controller_t(
			::so_5::outliving_reference_t< ::so_5::environment_t > env )
			: base_type_t{ env, ::so_5::mbox_type_t::multi_producer_single_consumer }
			{}

		[[nodiscard]] so_5::mbox_t
		acquire_receiving_mbox_for(
			consumer_numeric_id_t id,
			const std::type_index & msg_type )
			{
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				auto [it_consumer, _] = this->m_consumers_with_mboxes.emplace(
						id, one_consumer_mboxes_map_t{} );
				auto & consumer_map = it_consumer->second;

				auto it_msg = consumer_map.find( msg_type );
				if( it_msg == consumer_map.end() )
					{
						it_msg = consumer_map.emplace(
								msg_type,
								::so_5::make_unique_subscribers_mbox< Lock_Type >( this->m_env ) )
							.first;
					}

				return it_msg->second;
			}

		void
		consumer_destroyed( consumer_numeric_id_t id ) noexcept override
			{
				std::lock_guard< Lock_Type > lock{ this->m_lock };

				this->m_consumers_with_mboxes.erase( id );
			}

		void
		do_deliver_message(
			message_delivery_mode_t delivery_mode,
			const std::type_index & /*msg_type*/,
			const message_ref_t & message,
			unsigned int redirection_deep ) override
			{
				namespace err_ns = ::so_5::extra::msg_hierarchy::errors;

				// Do all necessary checks first...
				const ::so_5::message_t * raw_msg = message.get();
				if( !raw_msg )
					SO_5_THROW_EXCEPTION(
							err_ns::rc_signal_cannot_be_delivered,
							"signal can't be handled by msg_hierarchy's demuxer" );

				const root_base_t * root = dynamic_cast<const root_base_t *>(raw_msg);
				if( !root )
					SO_5_THROW_EXCEPTION(
							err_ns::rc_message_is_not_derived_from_root,
							"a message type has to be derived from root_t" );

				const auto msg_mutabilty_flag = message_mutability( *root );

				// ...the object has to be locked for the delivery procedure...
				std::shared_lock< Lock_Type > lock{ this->m_lock };

				if( ::so_5::message_mutability_t::mutable_message == msg_mutabilty_flag )
					{
						// Is there a single subscriber for a message?
						const auto single_dest_info =
								detect_receiver_for_mutable_msg_or_throw( root );
						if( single_dest_info )
							// The single subscriber is found, the message has to be
							// delivered to it.
							single_dest_info->m_dest_mbox->do_deliver_message(
									delivery_mode,
									single_dest_info->m_subscription_type,
									message,
									redirection_deep );
					}
				else
					{
						// ...now the immutable message can be delivered.
						this->do_delivery_procedure_for_immutable_message(
								delivery_mode,
								message,
								redirection_deep,
								root );
					}
			}

	private:
		//FIXME: document this!
		[[nodiscard]]
		std::optional< single_dest_info_t >
		detect_receiver_for_mutable_msg_or_throw(
			//! Message pointer that is casted to the hierarchy root.
			const root_base_t * root ) const
			{
				std::optional< single_dest_info_t > result;

				// Main loop for subscribers detection.
				for( const auto & [id, consumer_map] : this->m_consumers_with_mboxes )
					{
						// Try to deliver message by its actual type and them
						// trying to going hierarchy up.
						auto upcaster = root->so_message_upcaster_factory()(
								::so_5::message_mutability_t::mutable_message );

						bool delivery_finished = false;
						do
						{
							const auto type_to_find = upcaster.self_type();
							if( const auto it = consumer_map.find( type_to_find );
									it != consumer_map.end() )
								{
									if( result )
										{
											namespace err_ns = ::so_5::extra::msg_hierarchy::errors;
											// Another subscriber detected. The message
											// can't be delivered.
											SO_5_THROW_EXCEPTION(
													err_ns::rc_more_than_one_subscriber_for_mutable_msg,
													"more than one subscriber detected for "
													"a mutable message" );
										}
									else
										{
											result = single_dest_info_t{ it->second, type_to_find };
										}

									// Only one delivery for every consumer.
									delivery_finished = true;
								}
							else
								{
									delivery_finished = !upcaster.has_parent_factory();
									if( !delivery_finished )
										{
											// It's not the root yet, try to go one level up.
											upcaster = upcaster.parent_upcaster(
													::so_5::message_mutability_t::mutable_message );
										}
								}
						} while( !delivery_finished );
					}

				return result;
			}
	};

//
// basic_sending_mbox_t
//
/*!
 * @brief Basic implementation for all kinds of sending_mboxes.
 */
class basic_sending_mbox_t
	: public ::so_5::abstract_message_box_t
	{
		//! Controller to be used.
		demuxing_controller_iface_shptr_t m_controller;

		//! ID of the mbox.
		const ::so_5::mbox_id_t m_id;

	protected:
		/*!
		 * @brief Initializing constructor.
		 *
		 * It's protected to be accessible for derived classes only.
		 */
		basic_sending_mbox_t(
			//! Controller to be used.
			demuxing_controller_iface_shptr_t controller,
			//! ID for this mbox.
			::so_5::mbox_id_t id )
			: m_controller{ std::move(controller) }
			, m_id{ id }
			{}

	public:
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

		void
		do_deliver_message(
			message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int redirection_deep ) override
			{
				m_controller->do_deliver_message(
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
			const std::type_index & /*msg_type*/,
			abstract_message_sink_t & /*subscriber*/ ) noexcept override
			{
				// Nothing to do.
			}

		[[nodiscard]]
		so_5::environment_t &
		environment() const noexcept override
			{
				return m_controller->environment();
			}
	};

//
// multi_consumer_sending_mbox_t
//
//FIXME: document this!
template< typename Root >
class multi_consumer_sending_mbox_t final
	: public basic_sending_mbox_t
	{
	public:
		//! Initializing constructor.
		multi_consumer_sending_mbox_t(
			//! Controller to be used.
			demuxing_controller_iface_shptr_t controller,
			//! ID for this mbox.
			::so_5::mbox_id_t id )
			: basic_sending_mbox_t{ std::move(controller), id }
			{}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=MSG_HIERARCHY_SENDING_MPMC:root="
						<< typeid(Root).name()
						<< ":id=" << this->id() << ">";
				return s.str();
			}

		::so_5::mbox_type_t
		type() const override
			{
				return ::so_5::mbox_type_t::multi_producer_multi_consumer;
			}
	};

//
// single_consumer_sending_mbox_t
//
/*!
 * @brief Implementation of sending_mbox for case of MPSC mbox.
 */
template< typename Root >
class single_consumer_sending_mbox_t final
	: public basic_sending_mbox_t
	{
	public:
		//! Initializing constructor.
		single_consumer_sending_mbox_t(
			//! Controller to be used.
			demuxing_controller_iface_shptr_t controller,
			//! ID for this mbox.
			::so_5::mbox_id_t id )
			: basic_sending_mbox_t{ std::move(controller), id }
			{}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=MSG_HIERARCHY_SENDING_MPMS:root="
						<< typeid(Root).name()
						<< ":id=" << this->id() << ">";
				return s.str();
			}

		::so_5::mbox_type_t
		type() const override
			{
				return ::so_5::mbox_type_t::multi_producer_single_consumer;
			}
	};

} /* namespace impl */

//
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
	typename Lock_Type = std::shared_mutex >
class demuxer_t
	{
		static_assert( std::is_base_of_v<impl::root_base_t, Root>,
				"the Root has to be root_t<Msg> or a type derived from root_t<Msg>" );

		static_assert( !::so_5::is_mutable_message< Root >::value,
				"the Root can't be mutable_msg<T>" );

		impl::demuxing_controller_shptr_t< Root, Lock_Type > m_controller;

		::so_5::mbox_t m_sending_mbox;

		[[nodiscard]]
		static impl::demuxing_controller_shptr_t< Root, Lock_Type >
		make_required_demuxing_controller_object(
			::so_5::outliving_reference_t< ::so_5::environment_t > env,
			::so_5::mbox_type_t mbox_type )
			{
				using result_t = impl::demuxing_controller_shptr_t< Root, Lock_Type >;

				result_t result;
				switch( mbox_type )
					{
					case ::so_5::mbox_type_t::multi_producer_multi_consumer:
						{
							using controller_t = impl::multi_consumer_demuxing_controller_t<
									Root, Lock_Type >;

							result = result_t{ new controller_t{ env } };
						}
					break;

					case ::so_5::mbox_type_t::multi_producer_single_consumer:
						{
							using controller_t = impl::single_consumer_demuxing_controller_t<
									Root, Lock_Type >;

							result = result_t{ new controller_t{ env } };
						}
					break;
					}
				return result;
			}

		[[nodiscard]]
		static ::so_5::mbox_t
		make_required_sending_mbox(
			impl::demuxing_controller_iface_shptr_t controller,
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
										std::move(controller),
										mbox_id )
							};
					break;

					case ::so_5::mbox_type_t::multi_producer_single_consumer:
						result = so_5::mbox_t{
								new impl::single_consumer_sending_mbox_t< Root >(
										std::move(controller),
										mbox_id )
							};
					break;
					}

				return result;
			}

	public:
		friend void
		swap( demuxer_t & a, demuxer_t & b ) noexcept
			{
				using std::swap;

				swap( a.m_controller, b.m_controller );
				swap( a.m_sending_mbox, b.m_sending_mbox );
			}

		demuxer_t(
			::so_5::environment_t & env,
			::so_5::mbox_type_t mbox_type )
			: m_controller{
					make_required_demuxing_controller_object(
						::so_5::outliving_mutable( env ),
						mbox_type )
				}
			, m_sending_mbox{
					make_required_sending_mbox( m_controller, env, mbox_type )
				}
			{}

		demuxer_t( const demuxer_t & ) = delete;
		demuxer_t &
		operator=( const demuxer_t & ) = delete;

		demuxer_t( demuxer_t && o ) noexcept
			: m_controller{
					std::exchange( o.m_controller,
							impl::demuxing_controller_shptr_t< Root, Lock_Type >{} )
				}
			, m_sending_mbox{ std::exchange( o.m_sending_mbox, ::so_5::mbox_t{} ) }
			{}
		demuxer_t &
		operator=( demuxer_t && o ) noexcept
			{
				demuxer_t tmp{ std::move(o) };
				swap( *this, tmp );
				return *this;
			}

		[[nodiscard]]
		consumer_t< Root >
		allocate_consumer();

		[[nodiscard]]
		::so_5::mbox_t
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
		template< typename Demuxer_Root, typename Demuxer_Lock_Type >
		friend class demuxer_t;

		impl::demuxing_controller_iface_shptr_t m_controller;

		impl::consumer_numeric_id_t m_id;

		consumer_t(
			impl::demuxing_controller_iface_shptr_t controller,
			impl::consumer_numeric_id_t id )
			: m_controller{ std::move(controller) }
			, m_id{ id }
			{}

	public:
		friend void
		swap( consumer_t & a, consumer_t & b ) noexcept
			{
				using std::swap;

				swap( a.m_controller, b.m_controller );
				swap( a.m_id, b.m_id );
			}

		~consumer_t()
			{
				if( m_controller )
					{
						m_controller->consumer_destroyed( m_id );
					}
			}

		consumer_t( const consumer_t & ) = delete;
		consumer_t &
		operator=( const consumer_t & ) = delete;

		consumer_t( consumer_t && o ) noexcept
			: m_controller{
					std::exchange( o.m_controller,
							impl::demuxing_controller_iface_shptr_t{} )
				}
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
				if constexpr( ::so_5::is_mutable_message< Msg_Type >::value )
					{
						// There we have Msg_Type as `so_5::mutable_msg<Payload_Type>` and have
						// to extract and handle Payload_Type instead of Msg_Type.
						using payload_type =
								typename ::so_5::message_payload_type< Msg_Type >::payload_type;

						static_assert(
								(std::is_same_v<payload_type, Root>
										|| std::is_base_of_v<Root, payload_type>),
								"Msg_Type has to be a Root type or a type derived from Root" );

						if( ::so_5::mbox_type_t::multi_producer_multi_consumer ==
								m_controller->mbox_type() )
							SO_5_THROW_EXCEPTION(
									errors::rc_mpmc_demuxer_cannot_handler_mutable_msg,
									std::string{ "receiving_mbox can't be created for a mutable msg: " }
											+ typeid(Msg_Type).name() );
					}
				else
					{
						static_assert( (std::is_same_v<Msg_Type, Root> || std::is_base_of_v<Root, Msg_Type>),
								"Msg_Type has to be a Root type or a type derived from Root" );
					}

				return m_controller->acquire_receiving_mbox_for(
						m_id,
						::so_5::message_payload_type< Msg_Type >::subscription_type_index() );
			}
	};

//
// demuxer_t implementation
//

template< typename Root, typename Lock_Type >
[[nodiscard]] consumer_t< Root >
demuxer_t< Root, Lock_Type >::allocate_consumer()
	{
		return consumer_t< Root >{
				m_controller,
				m_controller->acquire_new_consumer_id()
			};
	}

} /* namespace so_5::extra::msg_hierarchy */

