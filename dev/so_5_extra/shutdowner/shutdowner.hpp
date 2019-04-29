/*!
 * \file
 * \brief Implementation of shutdowner-related stuff.
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/impl/msg_tracing_helpers.hpp>
#include <so_5/impl/message_limit_internals.hpp>
#include <so_5/impl/agent_ptr_compare.hpp>

#include <so_5/details/sync_helpers.hpp>

#include <so_5/mbox.hpp>
#include <so_5/send_functions.hpp>

#include <so_5/outliving.hpp>

namespace so_5 {

namespace extra {

namespace shutdowner {

namespace errors {

/*!
 * \brief An attempt to use illegal message type.
 *
 * For example: shutdowner mbox allows subscription only to
 * shutdown_initiated message.
 */
const int rc_illegal_msg_type =
		so_5::extra::errors::shutdowner_errors;

/*!
 * \brief Subscription to shutdowner mbox when shutdown in progess
 * is prohibited.
 */
const int rc_subscription_disabled_during_shutdown =
		so_5::extra::errors::shutdowner_errors + 1;

} /* namespace errors */

/*!
 * \brief A message to be used to inform about start of shutdown operation.
 *
 * \note
 * This is a message, not a signal. This message is empty now but it
 * can be extended in future versions of so_5_extra.
 */
struct shutdown_initiated_t : public so_5::message_t {};

namespace details {

/*!
 * \brief Implementation of stop_guard for shutdowner's purposes.
 *
 * This implementation sends shutdown_initiated_t message to the specified
 * mbox.
 */
class shutdowner_guard_t : public so_5::stop_guard_t
	{
		//! Mbox to which shutdown_initiated must be sent.
		const so_5::mbox_t m_notify_mbox;

	public :
		//! Initializing constructor.
		shutdowner_guard_t( so_5::mbox_t notify_mbox )
			:	m_notify_mbox( std::move(notify_mbox) )
			{}

		virtual void
		stop() noexcept override
			{
				so_5::send< shutdown_initiated_t >( m_notify_mbox );
			}
	};

/*!
 * \brief A signal which is used to limit time of shutdown operation.
 */
struct shutdown_time_elapsed_t final : public so_5::message_t {};

/*!
 * \brief Special mbox to receive and handle a signal about time limit.
 *
 * This mbox implements just one method: do_deliver_message(). A std::abort()
 * is called in this method if shutdown operation is not finished yet.
 */
class time_elapsed_mbox_t
	: public abstract_message_box_t
	{
	public :
		//! Initializing constructor.
		time_elapsed_mbox_t(
			//! SObjectizer environment to work in.
			//! It is necessary to access error_logging before calling std::abort().
			outliving_reference_t< environment_t > env,
			//! Unique ID of this mbox.
			mbox_id_t id )
			:	m_env( env )
			,	m_id( id )
			{}

		mbox_id_t
		id() const override
			{
				return m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & /*type_index*/,
			const message_limit::control_block_t * /*limit*/,
			agent_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION( rc_not_implemented,
						"subscribe_event_handler is not implemented for "
						"time_elapsed_mbox" );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & /*type_index*/,
			agent_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION( rc_not_implemented,
						"subscribe_event_handler is not implemented for "
						"time_elapsed_mbox" );
			}

		std::string
		query_name() const override
			{
				std::ostringstream ss;
				ss << "<mbox:type=MPSC:shutdowner_time_elapsed:id=" << m_id << ">";
				return ss.str();
			}

		mbox_type_t
		type() const override
			{
				return mbox_type_t::multi_producer_single_consumer;
			}

		void
		do_deliver_message(
			const std::type_index & /*msg_type*/,
			const message_ref_t & /*message*/,
			unsigned int /*overlimit_reaction_deep*/ ) override
			{
				m_env.get().error_logger().log(
						__FILE__, __LINE__,
						"Time of shutdown operation is elapsed. "
						"Application will be terminated." );
				std::abort();
			}

		void
		set_delivery_filter(
			const std::type_index & /*msg_type*/,
			const delivery_filter_t & /*filter*/,
			agent_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION( rc_not_implemented,
						"set_delivery_filter is not implemented for "
						"time_elapsed_mbox" );
			}

		void
		drop_delivery_filter(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) noexcept override
			{
				/* Nothing to do. */
			}

		environment_t &
		environment() const noexcept override
			{
				return m_env.get();
			}

	private :
		//! SOEnv to work in.
		outliving_reference_t< environment_t > m_env;
		//! Unique ID of that mbox.
		const mbox_id_t m_id;
	};

//
// subscriber_info_t
//
/*!
 * \brief Description of one subscriber.
 */
struct subscriber_info_t
	{
		//! Actual subscriber.
		/*!
		 * Can't be nullptr.
		 */
		agent_t * m_subscriber;
		//! Message limit for that subscriber.
		/*!
		 * Can be nullptr if message limit is not used.
		 */
		const message_limit::control_block_t * m_limits;

		//! Initializing constructor.
		subscriber_info_t(
			agent_t * subscriber,
			const message_limit::control_block_t * limits )
			:	m_subscriber(subscriber)
			,	m_limits(limits)
			{}

		//! Comparison operator.
		/*!
		 * Compares only pointers to m_subscriber with respects
		 * to agent's priority.
		 */
		bool
		operator<( const subscriber_info_t & o ) const
			{
				return ::so_5::impl::special_agent_ptr_compare(
						*m_subscriber, *(o.m_subscriber) );
			}
	};

//
// subscriber_container_t
//
/*!
 * \brief Type of subscriber's container.
 */
using subscriber_container_t = std::vector< subscriber_info_t >;

namespace status {

//! Avaliable statuses of shutdown operation.
enum class value_t
	{
		//! Shutdown is not started yet.
		not_started,
		//! Shutdown is started but there are some pending subscribers.
		started,
		//! All subscribers are unsubscribed.
		//! Shutdown can and should be completed.
		must_be_completed
	};

constexpr const value_t not_started = value_t::not_started;
constexpr const value_t started = value_t::started;
constexpr const value_t must_be_completed = value_t::must_be_completed;

//! Which action must be performed after updating status of shutdown operation.
enum class deferred_action_t
	{
		//! No action is required.
		do_nothing,
		//! Shutdown action must be completed.
		complete_shutdown
	};

//! Special object which holds status.
/*!
 * Updates for the status are enabled only to instances of updater_t class.
 */
class holder_t
	{
		friend class updater_t;

		value_t m_status = value_t::not_started;

	public :
		value_t
		current() const noexcept { return m_status; }
	};

//! Special object to change the status and detect deferred action to be performed.
class updater_t
	{
		outliving_reference_t< holder_t > m_status;
		deferred_action_t m_action = deferred_action_t::do_nothing;

	public :
		updater_t(
			outliving_reference_t< holder_t > status )
			:	m_status( status )
			{}

		value_t
		current() const noexcept { return m_status.get().current(); }

		void
		update( value_t new_status ) noexcept
			{
				m_status.get().m_status = new_status;
				if( value_t::must_be_completed == new_status )
					m_action = deferred_action_t::complete_shutdown;
				else
					m_action = deferred_action_t::do_nothing;
			}

		deferred_action_t
		action() const noexcept { return m_action; }
	};

} /* namespace status */

//
// notify_mbox_data_t
//
/*!
 * \brief An internal data of notify_mbox.
 *
 * It is collected in separate struct to provide ability to mark
 * object of that struct as mutable. It is necessary because some
 * of abstract_message_box_t's methods are declared as const by
 * historical reasons.
 */
struct notify_mbox_data_t
{
	//! Status of the shutdown operation.
	status::holder_t m_status;

	//! List of actual subscribers.
	subscriber_container_t m_subscribers;

	//! Mbox to be used for delayed shutdown_time_elapsed message.
	mbox_t m_time_elapsed_mbox;
	//! A time for shutdown operation.
	const std::chrono::steady_clock::duration m_max_shutdown_time;

	//! Timer ID for shutdown_time_elapsed message.
	/*!
	 * Will be used for canceling of delayed message when shutdown is
	 * completed.
	 *
	 * Will receive actual value only when shutdown operation started.
	 */
	timer_id_t m_shutdown_timer;

	//! Initializing constructor.
	notify_mbox_data_t(
		mbox_t time_elapsed_mbox,
		std::chrono::steady_clock::duration max_shutdown_time )
		:	m_time_elapsed_mbox( std::move(time_elapsed_mbox) )
		,	m_max_shutdown_time( max_shutdown_time )
		{}
};

//
// notify_mbox_t
//
/*!
 * \brief A special mbox which must be used for notification about
 * shutdown operation.
 *
 * \tparam Lock_Type type of lock to be used for thread safety
 * \tparam Tracing_Base type that implements message tracing stuff
 * It is expected to be so_5::impl::msg_tracing_helpers::tracing_enabled_base
 * or so_5::impl::msg_tracing_helpers::tracing_disabled_base
 */
template< typename Lock_Type, typename Tracing_Base >
class notify_mbox_t
	: public abstract_message_box_t
	, private ::so_5::details::lock_holder_detector<Lock_Type>::type
	, private Tracing_Base
	{
	public :
		template< typename... Tracing_Base_Args >
		notify_mbox_t(
			//! SObjectizer Environment to work in.
			outliving_reference_t< environment_t > env,
			//! A mbox for delayed message shutdown_time_elapsed_t.
			mbox_t time_elapsed_mbox,
			//! Time for the whole shutdown operation.
			std::chrono::steady_clock::duration max_shutdown_time,
			//! Unique ID of that mbox.
			mbox_id_t id,
			//! Params for tracing stuff.
			Tracing_Base_Args && ...tracing_args )
			:	Tracing_Base( std::forward<Tracing_Base_Args>(tracing_args)... )
			,	m_env( env )
			,	m_id( id )
			,	m_data( std::move(time_elapsed_mbox), max_shutdown_time )
			{
				// Now we can create and install shutdowner_guard to
				// prevent SObjectizer from shutdown.
				auto guard = std::make_shared< shutdowner_guard_t >( mbox_t(this) );
				m_env.get().setup_stop_guard( guard );
				m_shutdowner_guard = std::move(guard);
			}

		mbox_id_t
		id() const override
			{
				return m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & type_index,
			const message_limit::control_block_t * limit,
			agent_t & subscriber ) override
			{
				ensure_valid_message_type( type_index );
				this->lock_and_perform( [&] {
					do_event_subscription( limit, subscriber );
				} );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & type_index,
			agent_t & subscriber ) override
			{
				ensure_valid_message_type( type_index );
				const auto action = this->lock_and_perform( [&] {
					return do_event_unsubscripton( subscriber );
				} );

				if( status::deferred_action_t::complete_shutdown == action )
					complete_shutdown();
			}

		std::string
		query_name() const override
			{
				std::ostringstream ss;
				ss << "<mbox:type=MPMC:shutdowner:id=" << m_id << ">";
				return ss.str();
			}

		mbox_type_t
		type() const override
			{
				return mbox_type_t::multi_producer_multi_consumer;
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int /*overlimit_reaction_deep*/ ) override
			{
				ensure_valid_message_type( msg_type );
				const auto action = do_initiate_shutdown( msg_type, message );
				if( status::deferred_action_t::complete_shutdown == action )
					complete_shutdown();
			}

		void
		set_delivery_filter(
			const std::type_index & msg_type,
			const delivery_filter_t & /*filter*/,
			agent_t & /*subscriber*/ ) override
			{
				ensure_valid_message_type( msg_type );
				SO_5_THROW_EXCEPTION( rc_not_implemented,
						"unable to set delivery filter to shutdowner mbox" );
			}

		void
		drop_delivery_filter(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) noexcept override
			{
				/* Nothing to do. */
			}

		environment_t &
		environment() const noexcept override
			{
				return m_env.get();
			}

	private :
		//! SObjectizer Environment to work in.
		outliving_reference_t< environment_t > m_env;

		//! Stop_guard which prevents SObjectizer from shutdown.
		stop_guard_shptr_t m_shutdowner_guard;

		//! Unique ID of that mbox.
		const mbox_id_t m_id;

		//! Actual mbox data.
		notify_mbox_data_t m_data;

		//! Check for valid type of message to be handled.
		/*!
		 * \note
		 * Only shutdown_initiated_t message can be handled by that mbox type.
		 */
		static void
		ensure_valid_message_type( const std::type_index & type_index )
			{
				if( type_index != typeid(shutdown_initiated_t) )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::shutdowner::errors::rc_illegal_msg_type,
							"only shutdown_initiated_t message type is allowed to "
							"be used with shutdowner mbox" );
			}

		//! Main subscription actions.
		/*!
		 * May throw if shutdown is in progress or already completed.
		 */
		void
		do_event_subscription(
			const message_limit::control_block_t * limit,
			agent_t & subscriber )
			{
				if( status::not_started != m_data.m_status.current() )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::shutdowner::errors::rc_subscription_disabled_during_shutdown,
							"a creation of new subscription is disabled during "
							"shutdown procedure" );

				subscriber_info_t info{ &subscriber, limit };
				auto it = std::lower_bound(
						std::begin(m_data.m_subscribers),
						std::end(m_data.m_subscribers),
						info );
				m_data.m_subscribers.insert( it, info );
			}

		//! Main unsubscription actions.
		/*!
		 * Returns the action to be performed (shutdown completion may be
		 * necessary).
		 */
		status::deferred_action_t
		do_event_unsubscripton( agent_t & subscriber )
			{
				status::updater_t status_updater(
						outliving_mutable( m_data.m_status ) );

				subscriber_info_t info{ &subscriber, nullptr };
				auto it = std::lower_bound(
						std::begin(m_data.m_subscribers),
						std::end(m_data.m_subscribers), 
						info );
				if( it != std::end(m_data.m_subscribers) &&
						it->m_subscriber == &subscriber )
					{
						m_data.m_subscribers.erase( it );

						if( status::started == status_updater.current() )
							{
								if( m_data.m_subscribers.empty() )
									{
										status_updater.update(
												status::must_be_completed );
									}
							}
					}

				return status_updater.action();
			}

		//! Do all necessary actions for completion of shutdown.
		void
		complete_shutdown()
			{
				m_data.m_shutdown_timer.release();

				m_env.get().remove_stop_guard( m_shutdowner_guard );
			}

		//! Do all necessary actions for start of shutdown operation.
		/*!
		 * Returns the action to be performed (shutdown completion may be
		 * necessary).
		 */
		status::deferred_action_t
		do_initiate_shutdown(
			const std::type_index & msg_type,
			const message_ref_t & message )
			{
				status::updater_t updater(
						outliving_mutable( m_data.m_status ) );

				if( status::not_started == updater.current() )
					{
						updater.update( status::started );
						if( !m_data.m_subscribers.empty() )
							{
								send_shutdown_initated_to_all( msg_type, message );
								start_shutdown_clock();
							}
						else
							{
								updater.update( status::must_be_completed );
							}
					}

				return updater.action();
			}

		//! Send shutdown_initiated message to all actual subscribers.
		void
		send_shutdown_initated_to_all(
			const std::type_index & msg_type,
			const message_ref_t & message )
			{
				constexpr unsigned int overlimit_reaction_deep = 0u;

				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_Base
						*this, // as abstract_message_box_t
						"deliver_message",
						msg_type, message, overlimit_reaction_deep };

				for( const auto & subscriber : m_data.m_subscribers )
					{
						using namespace so_5::message_limit::impl;

						try_to_deliver_to_agent(
								m_id,
								*(subscriber.m_subscriber),
								subscriber.m_limits,
								msg_type,
								message,
								overlimit_reaction_deep,
								tracer.overlimit_tracer(),
								[&] {
									tracer.push_to_queue( subscriber.m_subscriber );

									agent_t::call_push_event(
											*(subscriber.m_subscriber),
											subscriber.m_limits,
											m_id,
											msg_type,
											message );
								} );
					}
			}

		//! Initiate delayed shutdown_time_elapsed message.
		void
		start_shutdown_clock()
			{
				m_data.m_shutdown_timer = so_5::send_periodic< shutdown_time_elapsed_t >(
						m_data.m_time_elapsed_mbox,
						m_data.m_max_shutdown_time,
						std::chrono::milliseconds::zero() );
			}
	};

} /* namespace details */

//
// layer_t
//
/*!
 * \brief An interface of shutdowner layer.
 *
 * This is a public interface of actual layer. An user should use only
 * that interface when it want to work with shutdown layer.
 *
 * For example, to subscribe to shutdown_initiated_t message it is neccessary
 * to receive a pointer/reference to layer_t and call notify_mbox() method:
 * \code
 * // To make a subscription to shutdown notification.
 * class my_agent final : public so_5::agent_t {
 * public :
 * 	my_agent(context_t ctx) : so_5::agent_t(std::move(ctx)) {
 * 		// Long way:
 * 		so_5::extra::shutdowner::layer_t * s =
 * 			so_environment().query_layer<so_5::extra::shutdowner::layer_t>();
 * 		so_subscribe(s->notify_mbox()).event(&my_agent::on_shutdown);
 *
 * 		// Shortest way:
 * 		so_subscribe(so_5::extra::shutdowner::layer(so_environment()).notify_mbox())
 * 				.event(&my_agent::on_shutdown);
 * 		...
 * 	}
 * 	...
 * private :
 * 	void on_shutdown(mhood_t<so_5::extra::shutdowner::shutdown_initiated_t>)
 * 	{...}
 * };
 * \endcode
 *
 * To initiate shutdown it is necessary to call so_5::environment_t::stop()
 * method:
 * \code
 * so_environment().stop();
 * \endcode
 */
class layer_t : public ::so_5::layer_t
	{
	public :
		//! Get a mbox which can be used for subscription to
		//! shutdown_initiated message.
		virtual mbox_t
		notify_mbox() const = 0;
	};

namespace details {

//
// layer_template_t
//
/*!
 * \brief A template-based implementation of shutdowner layer.
 *
 * \note
 * It creates all shutdowner-related stuff in start() method.
 */
template< typename Lock_Type >
class layer_template_t : public ::so_5::extra::shutdowner::layer_t
	{
	public :
		//! Initializing constructor.
		layer_template_t(
			//! Maximum time for the shutdown operation.
			std::chrono::steady_clock::duration shutdown_time )
			:	m_shutdown_time( shutdown_time )
			{}

		virtual mbox_t
		notify_mbox() const override
			{
				return m_notify_mbox;
			}

		virtual void
		start() override
			{
				auto time_elapsed_mbox = so_environment().make_custom_mbox(
						[&]( const mbox_creation_data_t & data ) {
							return do_make_time_elapsed_mbox( data );
						} );

				m_notify_mbox = so_environment().make_custom_mbox(
						[&]( const mbox_creation_data_t & data ) {
							return do_make_notification_mbox(
									data,
									std::move( time_elapsed_mbox ) );
						} );
			}

	private :
		//! Maximum time for the shutdown operation.
		const std::chrono::steady_clock::duration m_shutdown_time;

		//! Notification mbox.
		/*!
		 * Will be created in start() method. Until then it is a null pointer.
		 */
		mbox_t m_notify_mbox;

		//! Create mbox for delayed shutdown_time_elapsed message.
		mbox_t
		do_make_time_elapsed_mbox(
			//! Parameters for the new mbox.
			const mbox_creation_data_t & data )
			{
				return mbox_t( new details::time_elapsed_mbox_t(
							outliving_mutable( so_environment() ),
							data.m_id ) );
			}

		//! Create notification mbox.
		/*!
		 * A new mbox will be created with respect to message tracing stuff.
		 */
		mbox_t
		do_make_notification_mbox(
			//! Parameters for the new mbox.
			const mbox_creation_data_t & data,
			//! A mbox to be used for delayed shutdown_time_elapsed message.
			mbox_t time_elapsed_mbox )
			{
				mbox_t result;

				if( data.m_tracer.get().is_msg_tracing_enabled() )
					{
						using T = details::notify_mbox_t<
								Lock_Type,
								::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;
						result = mbox_t( new T(
									outliving_mutable( so_environment() ),
									std::move( time_elapsed_mbox ),
									m_shutdown_time,
									data.m_id,
									data.m_tracer.get() ) );
					}
				else
					{
						using T = details::notify_mbox_t<
								Lock_Type,
								::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;
						result = mbox_t( new T(
									outliving_mutable( so_environment() ),
									std::move( time_elapsed_mbox ),
									m_shutdown_time,
									data.m_id ) );
					}

				return result;
			}
	};

} /* namespace details */

//
// make_layer
//
/*!
 * \brief Main function to create an instance of shutdowner layer.
 *
 * Usage example:
 * \code
 * // Creation of layer with default mutex type.
 * so_5::launch([](so_5::environment_t & env) {...},
 * 	[](so_5::environment_params_t & params) {
 * 		params.add_layer( so_5::extra::shutdowner::make_layer<>(30s) );
 * 	});
 *
 * // Creation of layer with user-provided spinlock type.
 * so_5::launch([](so_5::environment_t & env) {...},
 * 	[](so_5::environment_params_t & params) {
 * 		params.add_layer( so_5::extra::shutdowner::make_layer<my_spinlock_t>(30s));
 * 	});
 *
 * // Creation of layer with null_mutex.
 * // Note: null_mutex must be used only for non thread-safe environments.
 * so_5::launch([](so_5::environment_t & env) {...},
 * 	[](so_5::environment_params_t & params) {
 * 		// Use single-threaded and not thread-safe environment.
 * 		params.infrastructure_factory(
 * 			so_5::env_infrastructures::simple_not_mtsafe::factory());
 * 		// Shutdowner layer with null_mutex can be used in that environment.
 * 		params.add_layer( so_5::extra::shutdowner::make_layer<so_5::null_mutex_t>(30s));
 * 	});
 * \endcode
 *
 * \tparam Lock_Type type of lock to be used for thread safety.
 */
template< typename Lock_Type = std::mutex >
std::unique_ptr< ::so_5::extra::shutdowner::layer_t >
make_layer(
	//! A maximum time for timeout operation.
	std::chrono::steady_clock::duration shutdown_max_time )
	{
		std::unique_ptr< ::so_5::extra::shutdowner::layer_t > result(
				new details::layer_template_t< Lock_Type >(
						shutdown_max_time ) );

		return result;
	}

//
// layer
//
/*!
 * \brief A helper function to receive a reference to shutdowner layer.
 *
 * Usage example:
 * \code
 * // To make a subscription to shutdown notification.
 * class my_agent final : public so_5::agent_t {
 * public :
 * 	my_agent(context_t ctx) : so_5::agent_t(std::move(ctx)) {
 * 		auto & s = so_5::extra::shutdowner::layer( so_environment() );
 * 		so_subscribe( s.notify_mbox() ).event( &my_agent::on_shutdown );
 * 		...
 * 	}
 * 	...
 * private :
 * 	void on_shutdown(mhood_t<so_5::extra::shutdowner::shutdown_initiated_t>)
 * 	{...}
 * };
 *
 * // To initiate shutdown.
 * so_5::extra::shutdowner::layer(env).initiate_shutdown();
 * \endcode
 *
 * \note May throw an exception if shutdowner layer is not defined.
 */
inline ::so_5::extra::shutdowner::layer_t &
layer( ::so_5::environment_t & env )
	{
		return *(env.query_layer< ::so_5::extra::shutdowner::layer_t >());
	}

} /* namespace shutdowner */

} /* namespace extra */

} /* namespace so_5 */

