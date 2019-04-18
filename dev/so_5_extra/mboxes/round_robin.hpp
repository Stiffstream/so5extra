/*!
 * \file
 * \brief Implementation of round-robin mbox.
 */

#pragma once

#include <so_5/impl/msg_tracing_helpers.hpp>
#include <so_5/impl/message_limit_internals.hpp>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace round_robin {

namespace details {

//
// subscriber_info_t
//

/*!
 * \brief An information block about one subscriber.
 */
struct subscriber_info_t
{
	//! Subscriber.
	agent_t * m_agent;

	//! Optional message limit for that subscriber.
	const so_5::message_limit::control_block_t * m_limit;

	//! Constructor for the case when subscriber info is being
	//! created during event subscription.
	subscriber_info_t(
		agent_t * agent,
		const so_5::message_limit::control_block_t * limit )
		:	m_agent( agent )
		,	m_limit( limit )
	{}
};

//
// subscriber_container_t
//

/*!
 * \brief Type of container for holding subscribers for one message type.
 */
class subscriber_container_t
	{
	public :
		using storage_t = std::vector< subscriber_info_t >;

		bool
		empty() const noexcept
			{
				return m_subscribers.empty();
			}

		void
		emplace_back(
			agent_t * agent,
			const so_5::message_limit::control_block_t * limit )
			{
				m_subscribers.emplace_back( agent, limit );
			}

		storage_t::iterator
		end() noexcept
			{
				return m_subscribers.end();
			}

		storage_t::iterator
		find( agent_t * subscriber ) noexcept
			{
				return std::find_if( std::begin(m_subscribers), std::end(m_subscribers),
						[&]( const auto & info ) {
							return info.m_agent == subscriber;
						} );
			}

		void
		erase( storage_t::iterator it ) noexcept
			{
				m_subscribers.erase(it);
				ensure_valid_current_subscriber_index();
			}

		const subscriber_info_t &
		current_subscriber() const noexcept
			{
				return m_subscribers[ m_current_subscriber ];
			}

		void
		switch_current_subscriber() noexcept
			{
				++m_current_subscriber;
				ensure_valid_current_subscriber_index();
			}

	private :
		storage_t m_subscribers;
		storage_t::size_type m_current_subscriber{ 0 };

		void
		ensure_valid_current_subscriber_index() noexcept
			{
				if( m_current_subscriber >= m_subscribers.size() )
					m_current_subscriber = 0;
			}
	};

//
// data_t
//

/*!
 * \brief Common part of round-robin mbox implementation.
 *
 * This part depends only on Lock type but not on tracing facilities.
 *
 * \tparam Lock type of lock object to be used.
 */
template< typename Lock >
struct data_t
	{
		//! Initializing constructor.
		data_t(
			environment_t & env,
			mbox_id_t id )
			:	m_env{ env }
			,	m_id{ id }
			{}

		//! SObjectizer Environment to work in.
		environment_t & m_env;

		//! ID of this mbox.
		const mbox_id_t m_id;

		//! Object lock.
		Lock m_lock;

		/*!
		 * \brief Map from message type to subscribers.
		 */
		using messages_table_t = std::map<
						std::type_index,
						subscriber_container_t >;

		//! Map of subscribers to messages.
		messages_table_t m_subscribers;
	};

//
// mbox_template_t
//

//! A template with implementation of round-robin mbox.
/*!
 * \tparam Lock_Type type of lock to be used for thread safety.
 * \tparam Tracing_Base base class with implementation of message
 * delivery tracing methods. Expected to be tracing_enabled_base or
 * tracing_disabled_base from so_5::impl::msg_tracing_helpers namespace.
 */
template<
	typename Lock_Type,
	typename Tracing_Base >
class mbox_template_t
	:	public abstract_message_box_t
	,	private data_t< Lock_Type >
	,	private Tracing_Base
	{
		using data_type = data_t< Lock_Type >;

	public:
		//! Initializing constructor.
		template< typename... Tracing_Args >
		mbox_template_t(
			//! SObjectizer Environment to work in.
			environment_t & env,
			//! ID of this mbox.
			mbox_id_t id,
			//! Optional parameters for Tracing_Base's constructor.
			Tracing_Args &&... args )
			:	data_type{ env, id }
			,	Tracing_Base{ std::forward< Tracing_Args >(args)... }
			{}

		mbox_id_t
		id() const override
			{
				return this->m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & type_wrapper,
			const so_5::message_limit::control_block_t * limit,
			agent_t & subscriber ) override
			{
				std::lock_guard< Lock_Type > lock( this->m_lock );

				auto it = this->m_subscribers.find( type_wrapper );
				if( it == this->m_subscribers.end() )
				{
					// There isn't such message type yet.
					subscriber_container_t container;
					container.emplace_back( &subscriber, limit );

					this->m_subscribers.emplace( type_wrapper, std::move( container ) );
				}
				else
				{
					auto & agents = it->second;

					auto pos = agents.find( &subscriber );
					if( pos == agents.end() )
						// There is no subscriber in the container.
						// It must be added.
						agents.emplace_back( &subscriber, limit );
				}
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & type_wrapper,
			agent_t & subscriber ) override
			{
				std::lock_guard< Lock_Type > lock( this->m_lock );

				auto it = this->m_subscribers.find( type_wrapper );
				if( it != this->m_subscribers.end() )
				{
					auto & agents = it->second;

					auto pos = agents.find( &subscriber );
					if( pos != agents.end() )
					{
						agents.erase( pos );
					}

					if( agents.empty() )
						this->m_subscribers.erase( it );
				}
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=RRMPSC:id=" << this->m_id << ">";

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
						*this, // as Tracing_Base
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
				SO_5_THROW_EXCEPTION(
						rc_delivery_filter_cannot_be_used_on_mpsc_mbox,
						"set_delivery_filter is called for MPSC-mbox" );
			}

		void
		drop_delivery_filter(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) noexcept override
			{
				// Nothing to do.
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return this->m_env;
			}

	private :
		void
		do_deliver_message_impl(
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep )
			{
				std::lock_guard< Lock_Type > lock( this->m_lock );

				auto it = this->m_subscribers.find( msg_type );
				if( it != this->m_subscribers.end() )
					{
						const auto & agent_info = it->second.current_subscriber();
						it->second.switch_current_subscriber();

						do_deliver_message_to_subscriber(
								agent_info,
								tracer,
								msg_type,
								message,
								overlimit_reaction_deep );
					}
				else
					tracer.no_subscribers();
			}

		void
		do_deliver_message_to_subscriber(
			const subscriber_info_t & agent_info,
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep )
			{
				using namespace so_5::message_limit::impl;

				try_to_deliver_to_agent(
						this->m_id,
						*(agent_info.m_agent),
						agent_info.m_limit,
						msg_type,
						message,
						overlimit_reaction_deep,
						tracer.overlimit_tracer(),
						[&] {
							tracer.push_to_queue( agent_info.m_agent );

							agent_t::call_push_event(
									*(agent_info.m_agent),
									agent_info.m_limit,
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
 * \brief Create an implementation of round-robin mbox.
 *
 * Usage example:
 * \code
	so_5::environment_t & env = ...;
	const so_5::mbox_t rrmbox = so_5::extra::mboxes::round_robin::make_mbox<>( env );
	...
	so_5::send< some_message >( rrmbox, ... );
 * \endcode
 *
 * \tparam Lock_Type type of lock to be used for thread safety.
 */
template< typename Lock_Type = std::mutex >
mbox_t
make_mbox( environment_t & env )
	{
		return env.make_custom_mbox(
				[]( const mbox_creation_data_t & data ) {
					mbox_t result;

					if( data.m_tracer.get().is_msg_tracing_enabled() )
						{
							using T = details::mbox_template_t<
									Lock_Type,
									::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;

							result = mbox_t{ std::make_unique<T>(
									data.m_env.get(),
									data.m_id,
									data.m_tracer.get() )
							};
						}
					else
						{
							using T = details::mbox_template_t<
									Lock_Type,
									::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;

							result = mbox_t{ std::make_unique<T>(
									data.m_env.get(),
									data.m_id )
							};
						}

					return result;
				} );
	}

} /* namespace round_robin */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

