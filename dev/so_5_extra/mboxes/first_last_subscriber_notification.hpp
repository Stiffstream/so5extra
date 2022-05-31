/*!
 * \file
 * \brief Implementation of mbox that informs about the first and the
 * last subscriptions.
 *
 * \since v.1.5.2
 */

#pragma once

#include <so_5/version.hpp>

#if SO_5_VERSION < SO_5_VERSION_MAKE(5u, 7u, 4u)
#error "SObjectizer-5.7.4 of newest is required"
#endif

#include <so_5_extra/error_ranges.hpp>

#include <so_5/impl/agent_ptr_compare.hpp>
#include <so_5/impl/message_limit_internals.hpp>
#include <so_5/impl/msg_tracing_helpers.hpp>
#include <so_5/impl/local_mbox_basic_subscription_info.hpp>

#include <so_5/details/sync_helpers.hpp>
#include <so_5/details/invoke_noexcept_code.hpp>

#include <so_5/mbox.hpp>
#include <so_5/send_functions.hpp>

#include <string_view>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace first_last_subscriber_notification {

namespace errors {

/*!
 * \brief An attempt to use a message type that differs from mbox's message
 * type.
 *
 * Type of message to be used with first_last_subscriber_notification_mbox
 * is fixed as part of first_last_subscriber_notification_mbox type.
 * An attempt to use different message type (for subscription, delivery or
 * setting delivery filter) will lead to an exception with this error code.
 *
 * \since v.1.5.2
 */
const int rc_different_message_type =
		so_5::extra::errors::mboxes_first_last_subscriber_notification_errors;

} /* namespace errors */

/*!
 * \brief Signal to be sent when the first subscriber arrives.
 *
 * \since v.1.5.2
 */
struct msg_first_subscriber final : public so_5::signal_t {};

/*!
 * \brief Signal to be sent when the last subscriber gone.
 *
 * \since v.1.5.2
 */
struct msg_last_subscriber final : public so_5::signal_t {};

namespace details {

/*!
 * \brief An information block about one subscriber.
 *
 * \since v.1.5.2
 */
using subscriber_info_t = so_5::impl::local_mbox_details::basic_subscription_info_t;

//
// template_independent_mbox_data_t
//
/*!
 * \brief A mixin with actual data which is necessary for implementation
 * of actual mbox.
 *
 * This data type doesn't depend on any template parameters.
 *
 * \since v.1.5.2
 */
struct template_independent_mbox_data_t
	{
		//! A special coparator for agents with respect to
		//! agent's priority.
		struct agent_ptr_comparator_t
			{
				bool operator()( agent_t * a, agent_t * b ) const noexcept
				{
					return ::so_5::impl::special_agent_ptr_compare( *a, *b );
				}
			};

		//! Type of subscribers map.
		using subscribers_map_t =
				std::map< agent_t *, subscriber_info_t, agent_ptr_comparator_t >;

		//! SObjectizer Environment to work in.
		environment_t & m_env;

		//! ID of the mbox.
		const mbox_id_t m_id;

		//! Mbox for notifications about the first/last subscribers.
		const mbox_t m_notification_mbox;

		//! Type of this mbox (MPMC or MPSC).
		const mbox_type_t m_mbox_type;

		//! Subscribers.
		/*!
		 * Can be empty.
		 */
		subscribers_map_t m_subscribers;

		//! Number of actual subscriptions.
		/*!
		 * \note
		 * There could be cases when a delivery filter is set, but subscription
		 * isn't made yet. Such cases shouldn't be treated as subscriptions.
		 * So we have to store the number of actual subscriptions separately
		 * and don't rely on the size of m_subscribers.
		 */
		std::size_t m_subscriptions_count{};

		template_independent_mbox_data_t(
			environment_t & env,
			mbox_id_t id,
			mbox_t notification_mbox,
			mbox_type_t mbox_type )
			:	m_env{ env }
			,	m_id{ id }
			,	m_notification_mbox{ std::move(notification_mbox) }
			,	m_mbox_type{ mbox_type }
		{}
	};

//
// actual_mbox_t
//

/*!
 * \brief An actual implementation of first/last subscriber message mbox.
 *
 * \tparam Msg_Type type of message to be used with this mbox.
 *
 * \tparam Lock_Type type of lock object to be used for thread safety.
 *
 * \tparam Tracing_Base base class with implementation of message
 * delivery tracing methods.
 *
 * \since v.1.5.2
 */
template<
	typename Msg_Type,
	typename Lock_Type,
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
			//! Mbox for notifications about the first/last subscriber.
			mbox_t notification_mbox,
			//! Type of this mbox (MPSC or MPMC).
			mbox_type_t mbox_type,
			//! Optional parameters for Tracing_Base's constructor.
			Tracing_Args &&... args )
			:	Tracing_Base{ std::forward< Tracing_Args >(args)... }
			,	m_data{ env, id, std::move(notification_mbox), mbox_type }
			{}

		mbox_id_t
		id() const override
			{
				return this->m_data.m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & msg_type,
			const so_5::message_limit::control_block_t * limit,
			agent_t & subscriber ) override
			{
				ensure_expected_msg_type(
						msg_type,
						"an attempt to subscribe with different message type" );

				insert_or_modify_subscriber(
						subscriber,
						[limit] {
							return subscriber_info_t{ limit };
						},
						[limit]( subscriber_info_t & info ) {
							info.set_limit( limit );
						},
						[this]() {
							this->m_data.m_subscriptions_count += 1u;
						} );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & msg_type,
			agent_t & subscriber ) override
			{
				ensure_expected_msg_type(
						msg_type,
						"an attempt to unsubscribe with different message type" );

				modify_and_remove_subscriber_if_needed(
						subscriber,
						[]( subscriber_info_t & info ) {
							info.drop_limit();
						},
						[this]() {
							this->m_data.m_subscriptions_count -= 1u;
						} );
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=FIRST_LAST_SUBSCR_NOTIFY";

				switch( this->m_data.m_mbox_type )
				{
				case mbox_type_t::multi_producer_multi_consumer:
					s << "(MPMC)";
				break;

				case mbox_type_t::multi_producer_single_consumer:
					s << "(MPSC)";
				break;
				}

				s << ":id=" << this->m_data.m_id << ">";

				return s.str();
			}

		mbox_type_t
		type() const override
			{
				return this->m_data.m_mbox_type;
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep ) override
			{
				ensure_expected_msg_type(
						msg_type,
						"an attempt to deliver with different message type" );

				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_base
						*this, // as abstract_message_box_t
						"deliver_message",
						msg_type, message, overlimit_reaction_deep };

				//FIXME: mbox_type should be taken into account here.
				ensure_immutable_message( msg_type, message );

				do_deliver_message_impl(
						tracer,
						msg_type,
						message,
						overlimit_reaction_deep );
			}

		void
		set_delivery_filter(
			const std::type_index & msg_type,
			const delivery_filter_t & filter,
			agent_t & subscriber ) override
			{
				ensure_expected_msg_type(
						msg_type,
						"an attempt to set delivery_filter with "
						"different message type" );

				insert_or_modify_subscriber(
						subscriber,
						[flt = &filter] {
							return subscriber_info_t{ flt };
						},
						[flt = &filter]( subscriber_info_t & info ) {
							info.set_filter( *flt );
						},
						[]() { /* nothing to do */ } );
			}

		void
		drop_delivery_filter(
			const std::type_index & msg_type,
			agent_t & subscriber ) noexcept override
			{
				ensure_expected_msg_type(
						msg_type,
						"an attempt to drop delivery_filter with "
						"different message type" );

				modify_and_remove_subscriber_if_needed(
						subscriber,
						[]( subscriber_info_t & info ) {
							info.drop_filter();
						},
						[]() { /* nothing to do */ } );
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
		Lock_Type m_lock;

		/*!
		 * Throws an error if msg_type differs from Config::msg_type.
		 */
		static void
		ensure_expected_msg_type(
			const std::type_index & msg_type,
			std::string_view error_description )
		{
			if( msg_type != typeid(Msg_Type) )
				SO_5_THROW_EXCEPTION(
						errors::rc_different_message_type,
						//FIXME: we have to create std::string object because
						//so_5::exception_t::raise expects std::string.
						//This should be fixed after resolving:
						//https://github.com/Stiffstream/sobjectizer/issues/46
						std::string{ error_description } );
		}

		template<
			typename Info_Maker,
			typename Info_Changer,
			typename Post_Action >
		void
		insert_or_modify_subscriber(
			agent_t & subscriber,
			Info_Maker maker,
			Info_Changer changer,
			Post_Action post_action )
			{
				std::lock_guard< Lock_Type > lock( m_lock );

				auto it_subscriber = this->m_data.m_subscribers.find( &subscriber );
				if( it_subscriber == this->m_data.m_subscribers.end() )
					// There is no subscriber yet. It must be added.
					this->m_data.m_subscribers.emplace(
							&subscriber, maker() );
				else
					// Subscriber is known. It must be updated.
					changer( it_subscriber->second );

				// All following actions shouldn't throw.
				so_5::details::invoke_noexcept_code( [this, &post_action]()
					{
						// post_action can increment number of actual subscribers so
						// we have to store the old value before calling post_action.
						const auto old_subscribers_count =
								this->m_data.m_subscriptions_count;
						post_action();

						if( old_subscribers_count < this->m_data.m_subscriptions_count &&
								1u == this->m_data.m_subscriptions_count )
							{
								// We've got the first subscriber.
								so_5::send< msg_first_subscriber >(
										this->m_data.m_notification_mbox );
							}
					} );
			}

		template<
			typename Info_Changer,
			typename Post_Action >
		void
		modify_and_remove_subscriber_if_needed(
			agent_t & subscriber,
			Info_Changer changer,
			Post_Action post_action )
			{
				std::lock_guard< Lock_Type > lock( m_lock );

				auto it_subscriber = this->m_data.m_subscribers.find(
						&subscriber );
				if( it_subscriber != this->m_data.m_subscribers.end() )
					{
						// Subscriber is found and must be modified.
						changer( it_subscriber->second );

						// If info about subscriber becomes empty after
						// modification then subscriber info must be removed.
						if( it_subscriber->second.empty() )
							this->m_data.m_subscribers.erase( it_subscriber );

						// All following actions shouldn't throw.
						so_5::details::invoke_noexcept_code( [this, &post_action]()
							{
								// post_action can increment number of actual
								// subscribers so we have to store the old value before
								// calling post_action.
								const auto old_subscribers_count =
										this->m_data.m_subscriptions_count;
								post_action();

								if( old_subscribers_count > this->m_data.m_subscriptions_count &&
										0u == this->m_data.m_subscriptions_count )
								{
									// We've lost the last subscriber.
									so_5::send< msg_last_subscriber >(
											this->m_data.m_notification_mbox );
								}
							} );
					}
			}

		void
		do_deliver_message_impl(
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep )
			{
				std::lock_guard< Lock_Type > lock( m_lock );

				auto & subscribers = this->m_data.m_subscribers;
				if( !subscribers.empty() )
					for( const auto & kv : subscribers )
						do_deliver_message_to_subscriber(
								*(kv.first),
								kv.second,
								tracer,
								msg_type,
								message,
								overlimit_reaction_deep );
				else
					tracer.no_subscribers();
			}

		void
		do_deliver_message_to_subscriber(
			agent_t & subscriber,
			const subscriber_info_t & subscriber_info,
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep ) const
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

						try_to_deliver_to_agent(
								this->m_data.m_id,
								subscriber,
								subscriber_info.limit(),
								msg_type,
								message,
								overlimit_reaction_deep,
								tracer.overlimit_tracer(),
								[&] {
									tracer.push_to_queue( std::addressof(subscriber) );

									agent_t::call_push_event(
											subscriber,
											subscriber_info.limit(),
											this->m_data.m_id,
											msg_type,
											message );
								} );
					}
				else
					tracer.message_rejected(
							std::addressof(subscriber), delivery_status );
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
// make_mbox
//
//FIXME: document this!
/*!
 * \brief Create an instance of first_last_subscriber_notification mbox.
 *
 * \tparam Msg_Type type of message to be used with a new mbox.
 *
 * \tparam Lock_Type type of lock to be used for thread safety. It can be
 * std::mutex or so_5::null_mutex_t (or any other type which can be used
 * with std::lock_quard).
 *
 * \since v.1.5.2
 */
template<
	typename Msg_Type,
	typename Lock_Type = std::mutex >
mbox_t
make_mbox(
	//! SObjectizer Environment to work in.
	environment_t & env,
	//! Mbox for notifications about the first/last subscriber.
	mbox_t notification_mbox,
	//! Type of this mbox (MPSC or MPMC).
	mbox_type_t mbox_type )
	{
		return env.make_custom_mbox(
				[&notification_mbox, mbox_type]( const mbox_creation_data_t & data )
				{
					mbox_t result;

					if( data.m_tracer.get().is_msg_tracing_enabled() )
						{
							using T = details::actual_mbox_t<
									Msg_Type,
									Lock_Type,
									::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;

							result = mbox_t{ new T{
									data.m_env.get(),
									data.m_id,
									std::move(notification_mbox),
									mbox_type,
									data.m_tracer.get()
								} };
						}
					else
						{
							using T = details::actual_mbox_t<
									Msg_Type,
									Lock_Type,
									::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;
							result = mbox_t{ new T{
									data.m_env.get(),
									data.m_id,
									std::move(notification_mbox),
									mbox_type
								} };
						}

					return result;
				} );
	}

} /* namespace first_last_subscriber_notification */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

