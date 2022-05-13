/*!
 * \file
 * \brief Implementation of unique_subscribers mbox.
 *
 * \since
 * v.1.5.0
 */

#pragma once

#include <so_5/version.hpp>

#if SO_5_VERSION < SO_5_VERSION_MAKE(5u, 7u, 4u)
#error "SObjectizer-5.7.4 of newest is required"
#endif

#include <so_5_extra/error_ranges.hpp>

#include <so_5/details/sync_helpers.hpp>

#include <so_5/mbox.hpp>

#include <so_5/impl/agent_ptr_compare.hpp>
#include <so_5/impl/message_limit_internals.hpp>
#include <so_5/impl/msg_tracing_helpers.hpp>
#include <so_5/impl/local_mbox_basic_subscription_info.hpp>

#include <so_5/details/invoke_noexcept_code.hpp>

#include <memory>
#include <tuple>
#include <utility>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace unique_subscribers {

namespace errors {

/*!
 * \brief An attempt to make another subscription to the same message type.
 *
 * This error is reported when there is an existing subscription to
 * the same message type. And this subscription is made for another agent.
 *
 * \since
 * v.1.5.0
 */
const int rc_subscription_exists =
		so_5::extra::errors::mboxes_unique_subscribers_errors;

/*!
 * \brief An attempt to set a delivery filter.
 *
 * Delivery filters are not supported by unique_subscribers mbox at the moment.
 *
 * \since
 * v.1.5.0
 */
const int rc_delivery_filters_not_supported =
		so_5::extra::errors::mboxes_unique_subscribers_errors + 1;

} /* namespace errors */

namespace details {

//
// subscriber_info_t
//
/*!
 * \brief Description of a subscriber.
 *
 * \since v.1.5.0, v.1.5.1
 */
class subscriber_info_t
	:	public so_5::impl::local_mbox_details::basic_subscription_info_t
	{
		using base_type_t = so_5::impl::local_mbox_details::basic_subscription_info_t;

		//! Subscriber.
		/*!
		 * \attention
		 * It's assumed that this pointer can't be null.
		 */
		agent_t * m_agent;

	public:
		//! Constructor for case when agent and limits are known.
		subscriber_info_t(
			agent_t & agent,
			const so_5::message_limit::control_block_t * limit )
			:	base_type_t{ limit }
			,	m_agent{ &agent }
			{}

		[[nodiscard]]
		agent_t *
		receiver() const noexcept
		{
			return m_agent;
		}
	};

//
// data_t
//

/*!
 * \brief A coolection of data required for local mbox implementation.
 *
 * \since v.1.5.0
 */
struct data_t
	{
		data_t( mbox_id_t id, environment_t & env )
			:	m_id{ id }
			,	m_env{ env }
			{}

		//! ID of this mbox.
		const mbox_id_t m_id;

		//! Environment for which the mbox is created.
		environment_t & m_env;

		/*!
		 * \brief Map from message type to subscribers.
		 */
		using messages_table_t = std::map< std::type_index, subscriber_info_t >;

		//! Map of subscribers to messages.
		messages_table_t m_subscribers;
	};

//
// actual_mbox_t
//

//! Actual implementation of unique_subscribers mbox.
/*!
 * \tparam Mutex type of lock to be used for thread safety.
 * \tparam Tracing_Base base class with implementation of message
 * delivery tracing methods.
 *
 * \since v.1.5.0
 */
template<
	typename Mutex,
	typename Tracing_Base >
class actual_mbox_t final
	:	public abstract_message_box_t
	,	private ::so_5::details::lock_holder_detector< Mutex >::type
	,	private data_t
	,	private Tracing_Base
	{
	public:
		template< typename... Tracing_Args >
		actual_mbox_t(
			//! ID of this mbox.
			mbox_id_t id,
			//! Environment for which the mbox is created.
			outliving_reference_t< environment_t > env,
			//! Optional parameters for Tracing_Base's constructor.
			Tracing_Args &&... args )
			:	data_t{ id, env.get() }
			,	Tracing_Base{ std::forward< Tracing_Args >(args)... }
			{}

		mbox_id_t
		id() const override
			{
				return this->m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & msg_type,
			const so_5::message_limit::control_block_t * limit,
			agent_t & subscriber ) override
			{
				insert_or_modify_subscriber(
						msg_type,
						subscriber,
						[&] {
							return subscriber_info_t{ subscriber, limit };
						},
						[&]( subscriber_info_t & info ) {
							info.set_limit( limit );
						} );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & msg_type,
			agent_t & subscriber ) override
			{
				modify_and_remove_subscriber_if_needed(
						msg_type,
						subscriber,
						[]( subscriber_info_t & info ) {
							info.drop_limit();
						} );
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=UNIQUESUBSCRIBERS:id=" << m_id << ">";

				return s.str();
			}

		mbox_type_t
		type() const override
			{
				return mbox_type_t::multi_producer_single_consumer;
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep ) override
			{
				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_base
						*this, // as abstract_message_box_t
						"deliver_message",
						msg_type, message, overlimit_reaction_deep };

				do_deliver_message_impl(
						tracer,
						msg_type,
						message,
						overlimit_reaction_deep );
			}

		void
		set_delivery_filter(
			const std::type_index & /*msg_type*/,
			const delivery_filter_t & /*filter*/,
			agent_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION( errors::rc_delivery_filters_not_supported,
						"delivery filters can't be used with "
						"unique_subscribers mboxes" );
			}

		void
		drop_delivery_filter(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) noexcept override
			{
				// No nothing.
			}

		environment_t &
		environment() const noexcept override
			{
				return m_env;
			}

	private :
		template< typename Info_Maker, typename Info_Changer >
		void
		insert_or_modify_subscriber(
			const std::type_index & msg_type,
			agent_t & subscriber,
			Info_Maker maker,
			Info_Changer changer )
			{
				this->lock_and_perform( [&] {
					auto it = this->m_subscribers.find( msg_type );
					if( it == this->m_subscribers.end() )
						{
							// There isn't such message type yet.
							m_subscribers.emplace( msg_type, maker() );
						}
					else
						{
							// If subscription or delivery filter is already set by
							// a different agent then we can't continue.
							if( it->second.receiver() != &subscriber )
								SO_5_THROW_EXCEPTION(
										errors::rc_subscription_exists,
										std::string{ "subscription is already exists "
														"for message type '" }
												+ msg_type.name()
												+ "'" );
							else
								changer( it->second );
						}
				} );
			}

		template< typename Info_Changer >
		void
		modify_and_remove_subscriber_if_needed(
			const std::type_index & msg_type,
			agent_t & subscriber,
			Info_Changer changer )
			{
				this->lock_and_perform( [&] {
					auto it = this->m_subscribers.find( msg_type );
					if( it != this->m_subscribers.end() )
						{
							auto & subscriber_info = it->second;

							// Skip all other actions if the subscription is
							// made for a different agent.
							if( &subscriber == subscriber_info.receiver() )
								{
									// Subscriber is found and must be modified.
									changer( subscriber_info );

									// If info about subscriber becomes empty after
									// modification then subscriber info must be removed.
									if( subscriber_info.empty() )
										this->m_subscribers.erase( it );
								}
						}
				} );
			}

		void
		remove_subscriber_if_needed(
			const std::type_index & msg_type,
			agent_t * subscriber )
			{
				this->lock_and_perform( [&] {
					auto it = this->m_subscribers.find( msg_type );
					if( it != this->m_subscribers.end() )
					{
						auto & subscriber_info = it->second;

						// Skip all other actions if the subscription is
						// made for a different agent.
						if( subscriber == subscriber_info.receiver() )
						{
							// Subscriber must be removed.
							this->m_subscribers.erase( it );
						}
					}
				} );
			}

		void
		do_deliver_message_impl(
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep )
			{
				this->lock_and_perform( [&] {
					auto it = this->m_subscribers.find( msg_type );
					if( it != this->m_subscribers.end() )
						{
							do_deliver_message_to_subscriber(
									it->second,
									tracer,
									msg_type,
									message,
									overlimit_reaction_deep );
						}
					else
						tracer.no_subscribers();
				} );
			}

		void
		do_deliver_message_to_subscriber(
			const subscriber_info_t & agent_info,
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep ) const
			{
				using namespace so_5::message_limit::impl;

				try_to_deliver_to_agent(
						this->m_id,
						*(agent_info.receiver()),
						agent_info.limit(),
						msg_type,
						message,
						overlimit_reaction_deep,
						tracer.overlimit_tracer(),
						[&] {
							tracer.push_to_queue( agent_info.receiver() );

							agent_t::call_push_event(
									*(agent_info.receiver()),
									agent_info.limit(),
									this->m_id,
									msg_type,
									message );
						} );
			}
	};

} /* namespace details */

//
// make_mbox
//
/*!
 * \brief Factory function for creation of a new instance of unique_subscribers
 * mbox.
 *
 * Usage examples:
 *
 * Create a mbox with std::mutex as Lock_Type (this mbox can safely be
 * used in multi-threaded environments):
 * \code
 * so_5::environment_t & env = ...;
 * auto mbox = so_5::extra::mboxes::unique_subscribers::make_mbox(env);
 * \endcode
 *
 * Create a mbox with so_5::null_mutex_t as Lock_Type (this mbox can only
 * be used in single-threaded environments):
 * \code
 * so_5::environment_t & env = ...;
 * auto mbox = so_5::extra::mboxes::unique_subscribers::make_mbox<so_5::null_mutex_t>(env);
 * \endcode
 *
 * \tparam Lock_Type type of lock to be used for thread safety. It can be
 * std::mutex or so_5::null_mutex_t (or any other type which can be used
 * with std::lock_quard).
 *
 * \since v.1.5.0
 */
template<
	typename Lock_Type = std::mutex >
[[nodiscard]]
mbox_t
make_mbox( so_5::environment_t & env )
	{
		return env.make_custom_mbox(
				[&]( const mbox_creation_data_t & data ) {
					mbox_t result;

					if( data.m_tracer.get().is_msg_tracing_enabled() )
						{
							using T = details::actual_mbox_t<
									Lock_Type,
									::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;

							result = mbox_t{ new T{
									data.m_id,
									data.m_env,
									data.m_tracer.get()
							} };
						}
					else
						{
							using T = details::actual_mbox_t<
									Lock_Type,
									::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;
							result = mbox_t{ new T{
									data.m_id,
									data.m_env
							} };
						}

					return result;
				} );
	}

} /* namespace unique_subscribers */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

