/*!
 * \file
 * \brief Implementation of mbox which holds last sent message.
 *
 * \since
 * v.1.0.3
 */

#pragma once

#include <so_5/version.hpp>

#if SO_5_VERSION < SO_5_VERSION_MAKE(5u, 8u, 0u)
#error "SObjectizer-5.8.0 of newest is required"
#endif

#include <so_5_extra/error_ranges.hpp>

#include <so_5/impl/message_sink_ptr_compare.hpp>
#include <so_5/impl/msg_tracing_helpers.hpp>
#include <so_5/impl/local_mbox_basic_subscription_info.hpp>

#include <so_5/details/sync_helpers.hpp>

#include <so_5/mbox.hpp>

#include <memory>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace retained_msg {

namespace errors {

// Note: this namespace is empty for now.

} /* namespace errors */

namespace details {

/*!
 * \brief A helper type which is a collection of type parameters.
 *
 * This type is used to simplify code of retained_msg_mbox internals.
 * Instead of writting something like:
 * \code
 * template< typename Traits >
 * class ... {...};
 *
 * template< typename Traits, typename Lock_Type >
 * class ... {...};
 * \endcode
 * this config_type allows to write like that:
 * \code
 * template< typename Config_Type >
 * class ... {...};
 *
 * template< typename Config_Type >
 * class ... {...};
 * \endcode
 *
 * \tparam Traits traits type to be used.
 *
 * \tparam Lock_Type type of object to be used for thread-safety (like
 * std::mutex or so_5::null_mutex_t).
 *
 * \since
 * v.1.0.3
 */
template<
	typename Traits,
	typename Lock_Type >
struct config_type
	{
		using traits_type = Traits;
		using lock_type = Lock_Type;
	};

/*!
 * \name Type extractors for config_type
 * \{
 */
template< typename Config_Type >
using traits_t = typename Config_Type::traits_type;

template< typename Config_Type >
using lock_t = typename Config_Type::lock_type;
/*!
 * \}
 */

/*!
 * \brief An information block about one subscriber.
 *
 * \since v.1.0.3, v.1.5.1
 */
using subscriber_info_t =
		so_5::impl::local_mbox_details::subscription_info_without_sink_t;

//
// messages_table_item_t
//
/*!
 * \brief A type of item of message table for retained message mbox.
 *
 * For each message type is necessary to store:
 * - a list of subscriber for that message;
 * - the last message sent.
 *
 * This type is intended to be used as a container for such data.
 *
 * \since
 * v.1.0.3
 */
struct messages_table_item_t
	{
		//! A special coparator for sinks with respect to
		//! sinks's priority.
		struct sink_ptr_comparator_t
			{
				bool operator()(
					abstract_message_sink_t * a,
					abstract_message_sink_t * b ) const noexcept
				{
					return ::so_5::impl::special_message_sink_ptr_compare( a, b );
				}
			};

		//! Type of subscribers map.
		using subscribers_map_t = std::map<
				abstract_message_sink_t *,
				subscriber_info_t,
				sink_ptr_comparator_t
			>;

		//! Subscribers.
		/*!
		 * Can be empty. This is for case when the first message was sent
		 * when there is no subscribers yet.
		 */
		subscribers_map_t m_subscribers;

		//! Retained message.
		/*!
		 * Can be nullptr. It means that there is no any attempts to send
		 * a message of this type.
		 */
		message_ref_t m_retained_msg;
	};

//
// template_independent_mbox_data_t
//
/*!
 * \brief A mixin with actual data which is necessary for implementation
 * of retained mbox.
 *
 * This data type doesn't depend on any template parameters.
 *
 * \since
 * v.1.0.3
 */
struct template_independent_mbox_data_t
	{
		//! SObjectizer Environment to work in.
		environment_t & m_env;

		//! ID of the mbox.
		const mbox_id_t m_id;

		//! Type of messages table.
		using messages_table_t =
				std::map< std::type_index, messages_table_item_t >;

		//! Table of current subscriptions and messages.
		messages_table_t m_messages_table;

		template_independent_mbox_data_t(
			environment_t & env,
			mbox_id_t id )
			:	m_env{ env }
			,	m_id{id}
		{}
	};

//
// actual_mbox_t
//

/*!
 * \brief An actual implementation of retained message mbox.
 *
 * \tparam Config type with main definitions for this message box type.
 *
 * \tparam Tracing_Base base class with implementation of message
 * delivery tracing methods.
 *
 * \since
 * v.1.0.3
 */
template<
	typename Config,
	typename Tracing_Base >
class actual_mbox_t final
	:	public abstract_message_box_t
	,	private Tracing_Base
	{
	public:
		/*!
		 * \brief Initializing constructor.
		 *
		 * \tparam Tracing_Args parameters for Tracing_Base constructor
		 * (can be empty list if Tracing_Base have only the default constructor).
		 */
		template< typename... Tracing_Args >
		actual_mbox_t(
			//! SObjectizer Environment to work in.
			environment_t & env,
			//! ID of this mbox.
			mbox_id_t id,
			//! Optional parameters for Tracing_Base's constructor.
			Tracing_Args &&... args )
			:	Tracing_Base{ std::forward< Tracing_Args >(args)... }
			,	m_data{ env, id }
			{}

		mbox_id_t
		id() const override
			{
				return this->m_data.m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & msg_type,
			abstract_message_sink_t & subscriber ) override
			{
				insert_or_modify_subscriber(
						msg_type,
						subscriber,
						[&] {
							return subscriber_info_t{
									subscriber_info_t::subscription_present_t{}
								};
						},
						[&]( subscriber_info_t & info ) {
							info.subscription_defined();
						} );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & msg_type,
			abstract_message_sink_t & subscriber ) override
			{
				modify_and_remove_subscriber_if_needed(
						msg_type,
						subscriber,
						[]( subscriber_info_t & info ) {
							info.subscription_dropped();
						} );
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=RETAINED_MPMC:id=" << this->m_data.m_id << ">";

				return s.str();
			}

		mbox_type_t
		type() const override
			{
				return mbox_type_t::multi_producer_multi_consumer;
			}

		void
		do_deliver_message(
			message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int redirection_deep ) override
			{
				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_base
						*this, // as abstract_message_box_t
						"deliver_message",
						delivery_mode,
						msg_type, message, redirection_deep };

				ensure_immutable_message( msg_type, message );

				do_deliver_message_impl(
						tracer,
						delivery_mode,
						msg_type,
						message,
						redirection_deep );
			}

		void
		set_delivery_filter(
			const std::type_index & msg_type,
			const delivery_filter_t & filter,
			abstract_message_sink_t & subscriber ) override
			{
				insert_or_modify_subscriber(
						msg_type,
						subscriber,
						[&] {
							return subscriber_info_t{ filter };
						},
						[&]( subscriber_info_t & info ) {
							info.set_filter( filter );
						} );
			}

		void
		drop_delivery_filter(
			const std::type_index & msg_type,
			abstract_message_sink_t & subscriber ) noexcept override
			{
				modify_and_remove_subscriber_if_needed(
						msg_type,
						subscriber,
						[]( subscriber_info_t & info ) {
							info.drop_filter();
						} );
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return this->m_data.m_env;
			}

	private :
		//! Data of this message mbox.
		template_independent_mbox_data_t m_data;

		//! Object lock.
		lock_t<Config> m_lock;

		template< typename Info_Maker, typename Info_Changer >
		void
		insert_or_modify_subscriber(
			const std::type_index & msg_type,
			abstract_message_sink_t & subscriber,
			Info_Maker maker,
			Info_Changer changer )
			{
				std::lock_guard< lock_t<Config> > lock( m_lock );

				// If there is no item for this message type it will be
				// created automatically.
				auto & table_item = this->m_data.m_messages_table[ msg_type ];

				auto it_subscriber = table_item.m_subscribers.find(
						std::addressof(subscriber) );
				if( it_subscriber == table_item.m_subscribers.end() )
					// There is no subscriber yet. It must be added.
					it_subscriber = table_item.m_subscribers.emplace(
							std::addressof(subscriber), maker() ).first;
				else
					// Subscriber is known. It must be updated.
					changer( it_subscriber->second );

				// If there is a retained message then delivery attempt
				// must be performed.
				// NOTE: an exception at this stage doesn't remove new subscription.
				if( table_item.m_retained_msg )
					try_deliver_retained_message_to(
							msg_type,
							table_item.m_retained_msg,
							subscriber,
							it_subscriber->second );
			}

		template< typename Info_Changer >
		void
		modify_and_remove_subscriber_if_needed(
			const std::type_index & msg_type,
			abstract_message_sink_t & subscriber,
			Info_Changer changer )
			{
				std::lock_guard< lock_t<Config> > lock( m_lock );

				auto it_table_item = this->m_data.m_messages_table.find( msg_type );
				if( it_table_item != this->m_data.m_messages_table.end() )
					{
						auto & table_item = it_table_item->second;

						auto it_subscriber = table_item.m_subscribers.find(
								std::addressof(subscriber) );
						if( it_subscriber != table_item.m_subscribers.end() )
							{
								// Subscriber is found and must be modified.
								changer( it_subscriber->second );

								// If info about subscriber becomes empty after
								// modification then subscriber info must be removed.
								if( it_subscriber->second.empty() )
									table_item.m_subscribers.erase( it_subscriber );
							}
					}
			}

		void
		do_deliver_message_impl(
			typename Tracing_Base::deliver_op_tracer const & tracer,
			message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int redirection_deep )
			{
				std::lock_guard< lock_t<Config> > lock( m_lock );

				// If there is no item for this message type it will be
				// created automatically.
				auto & table_item = this->m_data.m_messages_table[ msg_type ];

				// Message must be stored as retained.
				table_item.m_retained_msg = message;

				auto & subscribers = table_item.m_subscribers;
				if( !subscribers.empty() )
					for( const auto & kv : subscribers )
						do_deliver_message_to_subscriber(
								*(kv.first),
								kv.second,
								tracer,
								delivery_mode,
								msg_type,
								message,
								redirection_deep );
				else
					tracer.no_subscribers();
			}

		void
		do_deliver_message_to_subscriber(
			abstract_message_sink_t & subscriber,
			const subscriber_info_t & subscriber_info,
			typename Tracing_Base::deliver_op_tracer const & tracer,
			message_delivery_mode_t delivery_mode,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int redirection_deep ) const
			{
				const auto delivery_status =
						subscriber_info.must_be_delivered(
								subscriber,
								message,
								[]( const message_ref_t & msg ) -> message_t & {
									return *msg;
								} );

				if( delivery_possibility_t::must_be_delivered == delivery_status )
					{
						using namespace so_5::message_limit::impl;

						subscriber.push_event(
								this->id(),
								delivery_mode,
								msg_type,
								message,
								redirection_deep,
								tracer.overlimit_tracer() );
					}
				else
					tracer.message_rejected(
							std::addressof(subscriber), delivery_status );
			}

		/*!
		 * \brief An attempt to deliver retained message to the new subscriber.
		 *
		 * This attempt will be performed only if there is the retained message.
		 */
		void
		try_deliver_retained_message_to(
			const std::type_index & msg_type,
			const message_ref_t & retained_msg,
			abstract_message_sink_t & subscriber,
			const subscriber_info_t & subscriber_info )
			{
				if( retained_msg )
					{
						const unsigned int redirection_deep = 0;

						typename Tracing_Base::deliver_op_tracer tracer{
								*this, // as Tracing_base
								*this, // as abstract_message_box_t
								"deliver_message_on_subscription",
								message_delivery_mode_t::ordinary,
								msg_type,
								retained_msg,
								redirection_deep };

						do_deliver_message_to_subscriber(
								subscriber,
								subscriber_info,
								tracer,
								message_delivery_mode_t::ordinary,
								msg_type,
								retained_msg,
								redirection_deep );
					}
			}

		/*!
		 * \brief Ensures that message is an immutable message.
		 *
		 * Checks mutability flag and throws an exception if message is
		 * a mutable one.
		 */
		void
		ensure_immutable_message(
			const std::type_index & msg_type,
			const message_ref_t & what ) const
			{
				if( message_mutability_t::immutable_message !=
						message_mutability( what ) )
					SO_5_THROW_EXCEPTION(
							so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
							"an attempt to deliver mutable message via MPMC mbox"
							", msg_type=" + std::string(msg_type.name()) );
			}
	};

} /* namespace details */

//
//
// default_traits_t
//
/*!
 * \brief Default traits for retained message mbox.
 */
struct default_traits_t {};

//
// make_mbox
//
/*!
 * \brief Create an instance of retained message mbox.
 *
 * Simple usage example:
 * \code
 * so_5::environment_t & env = ...;
 * const so_5::mbox_t retained_mbox = so_5::extra::mboxes::retained_msg::make_mbox<>(env);
 * so_5::send<Some_Message>(retained_mbox, ...);
 * \endcode
 * An instance of default implementation retained message mbox will be created.
 * This instance will be protected by std::mutex.
 *
 * If you want to use retained_mbox in a single-threaded environment
 * without a multithreaded protection then so_5::null_mutex_t (or any
 * similar null-mutex implementation) can be used:
 * \code
 * so_5::environment_t & env = ...
 * const so_5::mbox_t retained_mbox =
 * 		so_5::extra::mboxes::retained_msg::make_mbox<
 * 				so_5::extra::mboxes::retained_msg::default_traits_t,
 * 				so_5::null_mutex_t>(env);
 * so_5::send<Some_Message>(retained_mbox, ...);
 * \endcode
 *
 * If you want to use your own mutex-like object (with interface which
 * allows to use your mutex-like class with std::lock_guard) then you can
 * do it similar way:
 * \code
 * so_5::environment_t & env = ...
 * const so_5::mbox_t retained_mbox =
 * 		so_5::extra::mboxes::retained_msg::make_mbox<
 * 				so_5::extra::mboxes::retained_msg::default_traits_t,
 * 				Your_Own_Mutex_Class>(env);
 * so_5::send<Some_Message>(retained_mbox, ...);
 * \endcode
 *
 * \tparam Traits type with traits of mbox implementation.
 *
 * \tparam Lock_Type a type of mutex to be used for protection of
 * retained message mbox content. This must be a DefaultConstructible
 * type with interface which allows to use Lock_Type with std::lock_guard.
 *
 * \since
 * v.1.0.3
 */
template<
	typename Traits = default_traits_t,
	typename Lock_Type = std::mutex >
mbox_t
make_mbox( environment_t & env )
	{
		using config_type = details::config_type< Traits, Lock_Type >;

		return env.make_custom_mbox(
				[]( const mbox_creation_data_t & data )
				{
					mbox_t result;

					if( data.m_tracer.get().is_msg_tracing_enabled() )
						{
							using T = details::actual_mbox_t<
									config_type,
									::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;

							result = mbox_t{ new T{
									data.m_env.get(), data.m_id, data.m_tracer.get()
								}
							};
						}
					else
						{
							using T = details::actual_mbox_t<
									config_type,
									::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;
							result = mbox_t{ new T{ data.m_env.get(), data.m_id } };
						}

					return result;
				} );
	}

} /* namespace retained_msg */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

