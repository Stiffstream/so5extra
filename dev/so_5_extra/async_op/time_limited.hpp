/*!
 * \file
 * \brief Implementation of time-limited asynchronous one-time operation.
 *
 * \since
 * v.1.0.4
 */

#pragma once

#include <so_5_extra/async_op/details.hpp>
#include <so_5_extra/async_op/errors.hpp>

#include <so_5/details/invoke_noexcept_code.hpp>

#include <so_5/impl/internal_env_iface.hpp>

#include <so_5/agent.hpp>
#include <so_5/send_functions.hpp>

#include <so_5/timers.hpp>

#include <so_5/outliving.hpp>

#include <chrono>
#include <vector>

namespace so_5 {

namespace extra {

namespace async_op {

namespace time_limited {

//! Enumeration for status of operation.
enum class status_t
	{
		//! Status of operation is unknown because the
		//! operation data has been moved to another proxy-object.
		unknown_moved_away,
		//! Operation is not activated yet.
		not_activated,
		//! Operation is activated.
		activated,
		//! Operation is completed.
		completed,
		//! Operation is cancelled.
		cancelled,
		//! Operation is timed-out.
		timedout
	};

namespace details {

/*!
 * \brief Container of all data related to async operation.
 *
 * \attention
 * There can be cyclic references from op_data_t instance to
 * completion/timeout handlers and back from completion/timeout
 * handlers to op_data_t. Because of that op_data_t can't
 * perform cleanup in its destructor because the destructor
 * will not be called until these cyclic references exist.
 * It requires special attention: content of op_data_t must
 * be cleared by owners of op_data_t instances.
 *
 * \since
 * v.1.0.4
 */
class op_data_t : protected ::so_5::atomic_refcounted_t
	{
	private :
		friend class ::so_5::intrusive_ptr_t<op_data_t>;

		//! Description of one completion handler subscription.
		struct completion_handler_subscription_t
			{
				//! Mbox from that a message is expected.
				::so_5::mbox_t m_mbox;

				//! State for that a subscription should be created.
				::so_5::outliving_reference_t< const ::so_5::state_t > m_state;

				//! Subscription type.
				/*!
				 * \note
				 * This is a subscription type. Not a type which will
				 * be passed to the event handler.
				 */
				std::type_index m_subscription_type;

				//! Event handler.
				::so_5::event_handler_method_t m_handler;

				//! Initializing constructor.
				completion_handler_subscription_t(
					so_5::mbox_t mbox,
					const so_5::state_t & state,
					std::type_index subscription_type,
					so_5::event_handler_method_t handler )
					:	m_mbox( std::move(mbox) )
					,	m_state( ::so_5::outliving_const(state) )
					,	m_subscription_type( std::move(subscription_type) )
					,	m_handler( std::move(handler) )
					{}
			};

		//! Description of one timeout handler subscription.
		struct timeout_handler_subscription_t
			{
				//! State for that a subscription should be created.
				::so_5::outliving_reference_t< const ::so_5::state_t > m_state;

				//! Event handler.
				::so_5::event_handler_method_t m_handler;

				//! Initializing constructor.
				timeout_handler_subscription_t(
					const so_5::state_t & state,
					so_5::event_handler_method_t handler )
					:	m_state( ::so_5::outliving_const(state) )
					,	m_handler( std::move(handler) )
					{}
			};

		//! Owner of async operation.
		::so_5::outliving_reference_t<::so_5::agent_t> m_owner;

		//! Type of timeout message.
		const std::type_index m_timeout_msg_type;

		//! The status of the async operation.
		status_t m_status = status_t::not_activated;

		//! Subscriptions for completion handlers which should be created on
		//! activation.
		std::vector< completion_handler_subscription_t > m_completion_handlers;

		//! Subscriptions for timeout handlers which should be created on
		//! activation.
		std::vector< timeout_handler_subscription_t > m_timeout_handlers;

		//! A default timeout handler which will be used as
		//! deadletter handler for timeout message/signal.
		/*!
		 * \note
		 * Can be nullptr. If so the default timeout handler will
		 * be created during activation procedure.
		 */
		::so_5::event_handler_method_t m_default_timeout_handler;

		//! A mbox to which timeout message/signal will be sent.
		/*!
		 * \note
		 * It will be limitless MPSC-mbox.
		 */
		const ::so_5::mbox_t m_timeout_mbox;

		//! An ID of timeout message/signal.
		/*!
		 * Will be used for cancellation of async operation.
		 */
		::so_5::timer_id_t m_timeout_timer_id;

		//! Create subscriptions for all defined completion handlers.
		/*!
		 * This method will rollback all subscriptions made in case of
		 * an exception. It means that if an exception is throw during
		 * subscription then all already subscribed completion handlers
		 * will be removed.
		 */
		void
		create_completion_handlers_subscriptions()
			{
				std::size_t i = 0;
				::so_5::details::do_with_rollback_on_exception( [&] {
						for(; i != m_completion_handlers.size(); ++i )
							{
								auto & ch = m_completion_handlers[ i ];
								m_owner.get().so_create_event_subscription(
										ch.m_mbox,
										ch.m_subscription_type,
										ch.m_state.get(),
										ch.m_handler,
										::so_5::thread_safety_t::unsafe,
										::so_5::event_handler_kind_t::final_handler );
							}
					},
					[&] {
						drop_completion_handlers_subscriptions_up_to( i );
					} );
			}

		//! Removes subscription for the first N completion handlers.
		void
		drop_completion_handlers_subscriptions_up_to(
			//! Upper bound in m_completion_handlers (not included).
			std::size_t upper_border ) noexcept
			{
				for( std::size_t i = 0; i != upper_border; ++i )
					{
						const auto & ch = m_completion_handlers[ i ];
						m_owner.get().so_destroy_event_subscription(
								ch.m_mbox,
								ch.m_subscription_type,
								ch.m_state.get() );
					}
			}

		//! Create subscriptions for all defined timeout handlers
		//! (including default handler).
		/*!
		 * This method will rollback all subscriptions made in case of
		 * an exception. It means that if an exception is throw during
		 * subscription then all already subscribed timeout handlers
		 * will be removed.
		 */
		void
		create_timeout_handlers_subscriptions()
			{
				do_subscribe_timeout_handlers();
				::so_5::details::do_with_rollback_on_exception( [&] {
						do_subscribe_default_timeout_handler();
					},
					[&] {
						do_unsubscribe_default_timeout_handler();
					} );
			}

		//! An implementation of subscription of timeout handlers.
		/*!
		 * Default timeout handler is not subscribed by this method.
		 */
		void
		do_subscribe_timeout_handlers()
			{
				std::size_t i = 0;
				::so_5::details::do_with_rollback_on_exception( [&] {
						for(; i != m_timeout_handlers.size(); ++i )
							{
								auto & th = m_timeout_handlers[ i ];
								m_owner.get().so_create_event_subscription(
										m_timeout_mbox,
										m_timeout_msg_type,
										th.m_state,
										th.m_handler,
										::so_5::thread_safety_t::unsafe,
										::so_5::event_handler_kind_t::final_handler );
							}
					},
					[&] {
						drop_timeout_handlers_subscriptions_up_to( i );
					} );
			}

		//! An implementation of subscription of default timeout handler.
		void
		do_subscribe_default_timeout_handler()
			{
				m_owner.get().so_create_deadletter_subscription(
						m_timeout_mbox,
						m_timeout_msg_type,
						m_default_timeout_handler,
						::so_5::thread_safety_t::unsafe );
			}

		//! An implementation of unsubscription of the first N timeout handlers.
		/*!
		 * The default timeout handler is not unsubscribed by this method.
		 */
		void
		drop_timeout_handlers_subscriptions_up_to(
			//! Upper bound in m_timeout_handlers (not included).
			std::size_t upper_border ) noexcept
			{
				for( std::size_t i = 0; i != upper_border; ++i )
					{
						const auto & th = m_timeout_handlers[ i ];
						m_owner.get().so_destroy_event_subscription(
								m_timeout_mbox,
								m_timeout_msg_type,
								th.m_state.get() );
					}
			}

		//! Remove subscriptions for all completion handlers.
		void
		drop_all_completion_handlers_subscriptions() noexcept
			{
				drop_completion_handlers_subscriptions_up_to(
						m_completion_handlers.size() );
			}

		//! Remove subscriptions for all timeout handlers
		//! (including the default timeout handler).
		void
		drop_all_timeout_handlers_subscriptions() noexcept
			{
				do_unsubscribe_default_timeout_handler();
				do_unsubscribe_timeout_handlers();
			}

		//! Actual unsubscription for the default timeout handler.
		void
		do_unsubscribe_default_timeout_handler() noexcept
			{
				m_owner.get().so_destroy_deadletter_subscription(
						m_timeout_mbox,
						m_timeout_msg_type );
			}

		//! Actual unsubscription for timeout handlers.
		/*!
		 * The default timeout handler is not unsubscribed by this method.
		 */
		void
		do_unsubscribe_timeout_handlers() noexcept
			{
				drop_timeout_handlers_subscriptions_up_to(
						m_timeout_handlers.size() );
			}

		//! Performs total cleanup of the operation data.
		/*!
		 * All subscriptions are removed.
		 * The delayed message is released.
		 *
		 * Content of m_completion_handlers, m_timeout_handlers and
		 * m_default_timeout_handler will be erased.
		 */
		void
		do_cancellation_actions() noexcept
			{
				// All subscriptions must be removed.
				drop_all_timeout_handlers_subscriptions();
				drop_all_completion_handlers_subscriptions();

				// Timer is no more needed.
				m_timeout_timer_id.release();

				// Information about completion and timeout handlers
				// is no more needed.
				m_completion_handlers.clear();
				m_timeout_handlers.clear();
				m_default_timeout_handler = ::so_5::event_handler_method_t{};
			}

		//! Creates the default timeout handler if it is not set
		//! by the user.
		void
		make_default_timeout_handler_if_necessary()
			{
				if( !m_default_timeout_handler )
					{
						m_default_timeout_handler =
							[op = ::so_5::intrusive_ptr_t<op_data_t>(this)](
								::so_5::message_ref_t & /*msg*/ )
							{
								op->timedout();
							};
					}
			}

		//! Cleans the operation data and sets the status specified.
		void
		clean_with_status(
			//! A new status to be set.
			status_t status )
			{
				do_cancellation_actions();
				m_status = status;
			}

	public :
		//! Initializing constructor.
		op_data_t(
			//! Owner of the async operation.
			::so_5::outliving_reference_t< ::so_5::agent_t > owner,
			//! Type of the timeout message/signal.
			const std::type_index & timeout_msg_type )
			:	m_owner( owner )
			,	m_timeout_msg_type( timeout_msg_type )
				// Timeout mbox must be created right now.
				// It will be used during timeout_handlers processing.
			,	m_timeout_mbox(
						::so_5::impl::internal_env_iface_t{ this->environment() }
						.create_mpsc_mbox(
								&m_owner.get(),
								// No message limits for this mbox.
								nullptr ) )
			{}

		//! Access to timeout mbox.
		[[nodiscard]] const ::so_5::mbox_t &
		timeout_mbox() const noexcept
			{
				return m_timeout_mbox;
			}

		//! Access to type of timeout message/signal.
		[[nodiscard]] const std::type_index &
		timeout_msg_type() const noexcept
			{
				return m_timeout_msg_type;
			}

		//! Access to owner of the async operation.
		[[nodiscard]] ::so_5::agent_t &
		owner() noexcept
			{
				return m_owner.get();
			}

		//! Access to SObjectizer Environment in that the owner is working.
		[[nodiscard]] ::so_5::environment_t &
		environment() noexcept
			{
				return m_owner.get().so_environment();
			}

		//! Reserve a space for storage of completion handlers.
		void
		reserve_completion_handlers_capacity(
			//! A required capacity.
			std::size_t capacity )
			{
				m_completion_handlers.reserve( capacity );
			}

		//! Reserve a space for storage of timeout handlers.
		void
		reserve_timeout_handlers_capacity(
			//! A required capacity.
			std::size_t capacity )
			{
				m_timeout_handlers.reserve( capacity );
			}

		//! Performs activation actions.
		/*!
		 * The default timeout handler is created if necessary.
		 *
		 * Subscriptions for completion handlers and timeout handler are created.
		 * If an exception is thrown during subscription then all already
		 * subscribed completion and timeout handler will be unsubscribed.
		 */
		void
		do_activation_actions()
			{
				make_default_timeout_handler_if_necessary();

				create_completion_handlers_subscriptions();

				::so_5::details::do_with_rollback_on_exception(
					[&] {
						create_timeout_handlers_subscriptions();
					},
					[&] {
						drop_all_completion_handlers_subscriptions();
					} );
			}

		//! Change the status of async operation.
		void
		change_status(
			//! A new status to be set.
			status_t status ) noexcept
			{
				m_status = status;
			}

		//! Get the current status of async operation.
		[[nodiscard]] status_t
		current_status() const noexcept
			{
				return m_status;
			}

		//! Add a new completion handler for the async operation.
		void
		add_completion_handler(
			//! A mbox from which the completion message/signal is expected.
			const ::so_5::mbox_t & mbox,
			//! A state for completion handler.
			const ::so_5::state_t & state,
			//! A type of the completion message/signal.
			const std::type_index msg_type,
			//! Completion handler itself.
			::so_5::event_handler_method_t evt_handler )
			{
				m_completion_handlers.emplace_back(
						mbox,
						state,
						msg_type,
						std::move(evt_handler) );
			}

		//! Add a new timeout handler for the async operation.
		void
		add_timeout_handler(
			//! A state for timeout handler.
			const ::so_5::state_t & state,
			//! Timeout handler itself.
			::so_5::event_handler_method_t evt_handler )
			{
				m_timeout_handlers.emplace_back(
						state,
						std::move(evt_handler) );
			}

		//! Set the default timeout handler.
		/*!
		 * \note
		 * If there already is the default timeout handler then the old
		 * one will be replaced by new handler.
		 */
		void
		default_timeout_handler(
			//! Timeout handler itself.
			::so_5::event_handler_method_t evt_handler )
			{
				m_default_timeout_handler = std::move(evt_handler);
			}

		//! Set the ID of timeout message/signal timer.
		void
		setup_timer_id(
			//! Timer ID of delayed timeout message/signal.
			::so_5::timer_id_t id )
			{
				m_timeout_timer_id = std::move(id);
			}

		//! Mark the async operation as completed.
		void
		completed() noexcept
			{
				clean_with_status( status_t::completed );
			}

		//! Mark the async operation as timedout.
		void
		timedout() noexcept
			{
				clean_with_status( status_t::timedout );
			}

		//! Mark the async operation as cancelled.
		void
		cancelled() noexcept
			{
				clean_with_status( status_t::cancelled );
			}
	};

/*!
 * \brief An alias for smart pointer to operation data.
 *
 * \tparam Operation_Data Type of actual operation data representation.
 * It is expected to be op_data_t or some of its derivatives.
 */
template< typename Operation_Data >
using op_shptr_t = ::so_5::intrusive_ptr_t< Operation_Data >;

} /* namespace details */

/*!
 * \brief An object that allows to cancel async operation.
 *
 * Usage example:
 * \code
 * namespace asyncop = so_5::extra::async_op::time_limited;
 * class demo : public so_5::agent_t {
 * 	asyncop::cancellation_point_t<> cp_;
 * 	...
 * 	void initiate_async_op() {
 * 		auto op = asyncop::make<timeout>(*this);
 * 		op.completed_on(...);
 * 		op.timeout_handler(...);
 * 		cp_ = op.activate(std::chrono::milliseconds(300), ...);
 * 	}
 * 	void on_operation_aborted(mhood_t<op_aborted_notify> cmd) {
 * 		// Operation aborted. There is no need to wait for
 * 		// completion of operation.
 * 		cp_.cancel();
 * 	}
 * };
 * \endcode
 *
 * \note
 * This class is DefaultConstructible and Moveable, but not Copyable.
 *
 * \attention
 * Objects of this class are not thread safe. It means that cancellation
 * point should be used only by agent which created it. And the cancellation
 * point can't be used inside thread-safe event handlers of that agent.
 *
 * \tparam Operation_Data Type of actual operation data representation.
 * Please note that this template parameter is indended to be used for
 * debugging and testing purposes only.
 *
 * \since
 * v.1.0.4
 */
template< typename Operation_Data = details::op_data_t >
class cancellation_point_t
	{
	private :
		template<typename Msg, typename Op_Data> friend class definition_point_t;

		//! Actual data for async op.
		/*!
		 * \note
		 * This can be a nullptr if the default constructor was used,
		 * or if operation is already cancelled, or if the content of
		 * the cancellation_point was moved to another object.
		 */
		details::op_shptr_t< Operation_Data > m_op;

		//! Initializing constructor to be used by definition_point.
		cancellation_point_t(
			//! Actual data for async operation.
			//! Can't be null.
			details::op_shptr_t< Operation_Data > op )
			:	m_op( std::move(op) )
			{}

	public :
		cancellation_point_t() = default;

		cancellation_point_t( const cancellation_point_t & ) = delete;
		cancellation_point_t( cancellation_point_t && ) = default;

		cancellation_point_t &
		operator=( const cancellation_point_t & ) = delete;

		cancellation_point_t &
		operator=( cancellation_point_t && ) = default;

		//! Get the status of the operation.
		/*!
		 * \note
		 * The value status_t::unknown_moved_away can be returned if
		 * the actual data of the async operation was moved to another object
		 * (like another cancellation_point_t). Or after a call to
		 * cleanup() method.
		 */
		[[nodiscard]] status_t
		status() const noexcept
			{
				if( m_op)
					return m_op->current_status();
				return status_t::unknown_moved_away;
			}

		//! Can the async operation be cancelled via this cancellation point?
		/*!
		 * \return true if the cancellation_point holds actual async operation's
		 * data and this async operation is not completed nor timedout yet.
		 */
		[[nodiscard]] bool
		is_cancellable() const noexcept
			{
				return m_op && status_t::activated == m_op->current_status();
			}

		//! An attempt to cancel the async operation.
		/*!
		 * \note
		 * Operation will be cancelled only if (true == is_cancellable()).
		 *
		 * It is safe to call cancel() if the operation is already
		 * cancelled or completed, or timed out. In that case the call to
		 * cancel() will have no effect.
		 */
		void
		cancel() noexcept
			{
				if( is_cancellable() )
					{
						m_op->cancelled();
					}
			}

		//! Throw out a reference to the async operation data.
		/*!
		 * A cancellation_point holds a reference to the async operation
		 * data. It means that the async operation data will be destroyed
		 * only when the cancellation_point will be destroyed. For example,
		 * in that case:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * class demo : public so_5::agent_t {
		 * 	...
		 * 	asyncop::cancellation_point_t<> cp_;
		 * 	...
		 * 	void initiate_async_op() {
		 * 		cp_ = asyncop::make<timeout_msg>(*this)
		 * 				...
		 * 				.activate(...);
		 * 		...
		 * 	}
		 * 	...
		 * 	void on_some_interruption() {
		 * 		// Cancel asyncop.
		 * 		cp_.cancel();
		 * 		// Async operation is cancelled, but the async operation
		 * 		// data is still in memory. The data will be deallocated
		 * 		// only when cp_ receives new value or when cp_ will be
		 * 		// destroyed (e.g. after destruction of demo agent).
		 * 	}
		 * \endcode
		 *
		 * A call to cleanup() method removes the reference to the async
		 * operation data. It means that if the operation is already
		 * completed, timed out or cancelled, then the operation data
		 * fill be deallocated.
		 *
		 * \note
		 * If the operation is still in progress then a call to cleanup()
		 * doesn't break the operation. You need to call cancel() manually
		 * before calling cleanup() to cancel the operation.
		 */
		void
		cleanup() noexcept
			{
				m_op.reset();
			}
	};

namespace details
{

/*!
 * \brief A basic part of implementation of definition_point.
 *
 * This part is independent from timeout message/signal type.
 *
 * \note
 * This is Moveable class. But not DefaultConstructible.
 * And not Copyable.
 *
 * \note
 * Most of methods are declared as protected to be used only in
 * derived class.
 *
 * \since
 * v.1.0.4
 */
template< typename Operation_Data >
class msg_independent_part_of_definition_point_t
	{
	protected :
		//! Actual async operation data.
		/*!
		 * \note This pointer can be nullptr after activation or
		 * when the object is move away.
		 */
		details::op_shptr_t< Operation_Data > m_op;

		//! Initializing constructor.
		msg_independent_part_of_definition_point_t(
			//! Actual async operation data.
			//! This pointer can't be nullptr.
			details::op_shptr_t< Operation_Data > op )
			:	m_op( std::move(op) )
			{}

		msg_independent_part_of_definition_point_t(
			const msg_independent_part_of_definition_point_t & ) = delete;
		msg_independent_part_of_definition_point_t &
		operator=( const msg_independent_part_of_definition_point_t & ) = delete;

		msg_independent_part_of_definition_point_t(
			msg_independent_part_of_definition_point_t && ) = default;
		msg_independent_part_of_definition_point_t &
		operator=( msg_independent_part_of_definition_point_t && ) = default;

		//! Can the async_op be activated?
		/*!
		 * The async operation can't be activated if it is already activated.
		 * Or if the content of the definition_point is moved to another
		 * definition_point.
		 */
		[[nodiscard]] bool
		is_activable() const noexcept
			{
				return m_op;
			}

		//! Throws an exception if the async operation can't be activated.
		void
		ensure_activable() const
			{
				if( !is_activable() )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::async_op::errors::rc_op_cant_be_activated,
							"An attempt to do something on non-activable async_op" );
			}

		//! Reserve a space for storage of completion handlers.
		void
		reserve_completion_handlers_capacity_impl(
			//! A required capacity.
			std::size_t capacity )
			{
				ensure_activable();

				m_op->reserve_completion_handlers_capacity( capacity );
			}

		//! Reserve a space for storage of timeout handlers.
		void
		reserve_timeout_handlers_capacity_impl(
			//! A required capacity.
			std::size_t capacity )
			{
				ensure_activable();

				m_op->reserve_timeout_handlers_capacity( capacity );
			}

		//! The actual implementation of addition of completion handler.
		/*!
		 * \tparam Msg_Target It can be a mbox, or a reference to an agent.
		 * In the case if Msg_Target if a reference to an agent the agent's
		 * direct mbox will be used as message source.
		 *
		 * \tparam Event_Handler Type of actual handler for message/signal.
		 * It can be a pointer to agent's method or lambda (or another
		 * type of functional object).
		 */
		template<
				typename Msg_Target,
				typename Event_Handler >
		void
		completed_on_impl(
			//! A source from which a completion message/signal is expected.
			//! It can be a mbox or an agent (in that case the agent's direct
			//! mbox will be used).
			Msg_Target && msg_target,
			//! A state for that a completion handler is defined.
			const ::so_5::state_t & state,
			//! Completion handler itself.
			Event_Handler && evt_handler )
			{
				ensure_activable();

				const auto mbox = ::so_5::extra::async_op::details::target_to_mbox(
						msg_target );

				auto evt_handler_info =
						::so_5::preprocess_agent_event_handler(
								mbox,
								m_op->owner(),
								std::forward<Event_Handler>(evt_handler) );

				::so_5::event_handler_method_t actual_handler =
					[op = m_op,
					user_handler = std::move(evt_handler_info.m_handler)](
						::so_5::message_ref_t & msg )
					{
						op->completed();
						user_handler( msg );
					};

				m_op->add_completion_handler(
						mbox,
						state,
						evt_handler_info.m_msg_type,
						std::move(actual_handler) );
			}

		//! The actual implementation of addition of timeout handler.
		/*!
		 * \tparam Event_Handler Type of actual handler for message/signal.
		 * It can be a pointer to agent's method or lambda (or another
		 * type of functional object).
		 */
		template< typename Event_Handler >
		void
		timeout_handler_impl(
			//! A state for that a timeout handler is defined.
			const ::so_5::state_t & state,
			//! Timeout handler itself.
			Event_Handler && evt_handler )
			{
				ensure_activable();

				m_op->add_timeout_handler(
						state,
						create_actual_timeout_handler(
								std::forward<Event_Handler>(evt_handler) ) );
			}

		//! The actual implementation of addition of the default timeout handler.
		/*!
		 * \tparam Event_Handler Type of actual handler for message/signal.
		 * It can be a pointer to agent's method or lambda (or another
		 * type of functional object).
		 */
		template< typename Event_Handler >
		void
		default_timeout_handler_impl(
			//! The default timeout handler itself.
			Event_Handler && evt_handler )
			{
				ensure_activable();

				m_op->default_timeout_handler(
						create_actual_timeout_handler(
								std::forward<Event_Handler>(evt_handler) ) );
			}

	private :
		//! A helper method for creation a wrapper for a timeout handler.
		template< typename Event_Handler >
		[[nodiscard]] ::so_5::event_handler_method_t
		create_actual_timeout_handler(
			//! Timeout handler to be wrapped.
			Event_Handler && evt_handler )
			{
				auto evt_handler_info =
						::so_5::preprocess_agent_event_handler(
								m_op->timeout_mbox(),
								m_op->owner(),
								std::forward<Event_Handler>(evt_handler) );
				if( evt_handler_info.m_msg_type != m_op->timeout_msg_type() )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::async_op::errors::rc_msg_type_mismatch,
							std::string(
									"An attempt to register timeout handler for "
									"different message type. Expected type: " )
							+ m_op->timeout_msg_type().name()
							+ ", actual type: " + evt_handler_info.m_msg_type.name() );

				return
					[op = m_op,
					user_handler = std::move(evt_handler_info.m_handler)](
						::so_5::message_ref_t & msg )
					{
						op->timedout();
						user_handler( msg );
					};
			}
	};

} /* namespace details */

/*!
 * \brief An interface for definition of async operation.
 *
 * Object of this type is usually created by make() function and
 * is used for definition of async operation. Completion and timeout
 * handlers are set for async operation by using definition_point object.
 *
 * Then an user calls activate() method and the definition_point transfers
 * the async operation data into cancellation_point object. It means that
 * after a call to activate() method the definition_point object should
 * not be used. Because it doesn't hold any async operation anymore.
 *
 * A simple usage without storing the cancellation_point:
 * \code
 * namespace asyncop = so_5::extra::async_op::time_limited;
 * class demo : public so_5::agent_t {
 * 	struct timeout final : public so_5::signal_t {};
 * 	...
 * 	void initiate_async_op() {
 * 		// Create a definition_point for a new async operation...
 * 		asyncop::make<timeout>(*this)
 * 			// ...then set up completion handler(s)...
 * 			.completed_on(
 * 				*this,
 * 				so_default_state(),
 * 				&demo::on_first_completion_msg )
 * 			.completed_on(
 * 				some_external_mbox_,
 * 				some_user_defined_state_,
 * 				[this](mhood_t<another_completion_msg> cmd) {...})
 * 			// ...then set up timeout handler(s)...
 * 			.timeout_handler(
 * 				so_default_state(),
 * 				&demo::on_timeout )
 * 			.timeout_handler(
 * 				some_user_defined_state_,
 * 				[this](mhood_t<timeout> cmd) {...})
 * 			// ...and now we can activate the operation.
 * 			.activate(std::chrono::milliseconds(300));
 * 		...
 * 	}
 * };
 * \endcode
 * \note
 * There is no need to hold definition_point object after activation
 * of the async operation. This object can be safely discarded.
 *
 * A more complex example using cancellation_point for
 * cancelling the operation.
 * \code
 * namespace asyncop = so_5::extra::async_op::time_limited;
 * class demo : public so_5::agent_t {
 * 	struct timeout final : public so_5::signal_t {};
 * 	...
 * 	// Cancellation point for the async operation.
 * 	asyncop::cancellation_point_t<> cp_;
 * 	...
 * 	void initiate_async_op() {
 * 		// Create a definition_point for a new async operation
 * 		// and store the cancellation point after activation.
 * 		cp_ = asyncop::make<timeout>(*this)
 * 			// ...then set up completion handler(s)...
 * 			.completed_on(
 * 				*this,
 * 				so_default_state(),
 * 				&demo::on_first_completion_msg )
 * 			.completed_on(
 * 				some_external_mbox_,
 * 				some_user_defined_state_,
 * 				[this](mhood_t<another_completion_msg> cmd) {...})
 * 			// ...then set up timeout handler(s)...
 * 			.timeout_handler(
 * 				so_default_state(),
 * 				&demo::on_timeout )
 * 			.timeout_handler(
 * 				some_user_defined_state_,
 * 				[this](mhood_t<timeout> cmd) {...})
 * 			// ...and now we can activate the operation.
 * 			.activate(std::chrono::milliseconds(300));
 * 		...
 * 	}
 * 	...
 * 	void on_abortion_signal(mhood_t<abort_signal>) {
 * 		// The current async operation should be cancelled.
 * 		cp_.cancel();
 * 	}
 * };
 * \endcode
 *
 * \note
 * This class is Moveable, but not DefaultConstructible nor Copyable.
 *
 * \attention
 * Objects of this class are not thread safe. It means that a definition
 * point should be used only by agent which created it. And the definition
 * point can't be used inside thread-safe event handlers of that agent.
 *
 * \since
 * v.1.0.4
 */
template<
	typename Message,
	typename Operation_Data = details::op_data_t >
class definition_point_t
	:	protected details::msg_independent_part_of_definition_point_t< Operation_Data >
	{
		//! A short alias for base type.
		using base_type_t =
				details::msg_independent_part_of_definition_point_t< Operation_Data >;

	public :
		//! Initializing constructor.
		definition_point_t(
			//! The owner of the async operation.
			::so_5::outliving_reference_t< ::so_5::agent_t > owner )
			:	base_type_t(
					details::op_shptr_t< Operation_Data >(
							new Operation_Data( owner, typeid(Message) ) ) )
			{}

		~definition_point_t()
			{
				// If operation data is still here then it means that
				// there wasn't call to `activate()` and we should cancel
				// all described handlers.
				// This will lead to deallocation of operation data.
				if( this->m_op )
					this->m_op->cancelled();
			}

		definition_point_t( const definition_point_t & ) = delete;
		definition_point_t &
		operator=( const definition_point_t & ) = delete;

		definition_point_t( definition_point_t && ) = default;
		definition_point_t &
		operator=( definition_point_t && ) = default;

		using base_type_t::is_activable;

		/*!
		 * \brief Reserve a space for storage of completion handlers.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * auto op = asyncop::make<timeout>(some_agent);
		 * // Reserve space for four completion handlers.
		 * op.reserve_completion_handlers_capacity(4);
		 * op.completed_on(...);
		 * op.completed_on(...);
		 * op.completed_on(...);
		 * op.completed_on(...);
		 * op.activate(...);
		 * \endcode
		 */
		definition_point_t &
		reserve_completion_handlers_capacity(
			//! A required capacity.
			std::size_t capacity ) &
			{
				this->reserve_completion_handlers_capacity_impl( capacity );
				return *this;
			}

		//! Just a proxy for actual version of %reserve_completion_handlers_capacity.
		template< typename... Args >
		auto
		reserve_completion_handlers_capacity( Args && ...args ) &&
			{
				return std::move(this->reserve_completion_handlers_capacity(
							std::forward<Args>(args)... ));
			}

		/*!
		 * \brief Reserve a space for storage of timeout handlers.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * auto op = asyncop::make<timeout>(some_agent);
		 * // Reserve space for eight timeout handlers.
		 * op.reserve_timeout_handlers_capacity(8);
		 * op.timeout_handler(...);
		 * op.timeout_handler(...);
		 * ...
		 * op.activate(...);
		 * \endcode
		 */
		definition_point_t &
		reserve_timeout_handlers_capacity(
			//! A required capacity.
			std::size_t capacity ) &
			{
				this->reserve_timeout_handlers_capacity_impl( capacity );
				return *this;
			}

		//! Just a proxy for actual version of %reserve_timeout_handlers_capacity.
		template< typename... Args >
		auto
		reserve_timeout_handlers_capacity( Args && ...args ) &&
			{
				return std::move(this->reserve_timeout_handlers_capacity(
							std::forward<Args>(args)... ));
			}

		/*!
		 * \brief Add a completion handler for the async operation.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * class demo : public so_5::agent_t {
		 * 	...
		 * 	void initiate_async_op() {
		 *			asyncop::make<timeout>(*this)
		 *				.completed_on(
		 *					*this,
		 *					so_default_state(),
		 *					&demo::some_event )
		 *				.completed_on(
		 *					some_mbox_,
		 *					some_agent_state_,
		 *					[this](mhood_t<some_msg> cmd) {...})
		 *				...
		 *				.activate(...);
		 * 	}
		 * };
		 * \endcode
		 *
		 * \note
		 * The completion handler will be stored inside async operation
		 * data. Actual subscription for it will be made during activation
		 * of the async operation.
		 *
		 * \tparam Msg_Target It can be a mbox, or a reference to an agent.
		 * In the case if Msg_Target if a reference to an agent the agent's
		 * direct mbox will be used as message source.
		 *
		 * \tparam Event_Handler Type of actual handler for message/signal.
		 * It can be a pointer to agent's method or lambda (or another
		 * type of functional object).
		 */
		template<
				typename Msg_Target,
				typename Event_Handler >
		definition_point_t &
		completed_on(
			//! A source from which a completion message is expected.
			//! It \a msg_target is a reference to an agent then
			//! the agent's direct mbox will be used.
			Msg_Target && msg_target,
			//! A state for which that completion handler will be subscribed.
			const ::so_5::state_t & state,
			//! The completion handler itself.
			Event_Handler && evt_handler ) &
			{
				this->completed_on_impl(
						std::forward<Msg_Target>(msg_target),
						state,
						std::forward<Event_Handler>(evt_handler) );

				return *this;
			}

		//! Just a proxy for the main version of %completed_on.
		template< typename... Args >
		definition_point_t &&
		completed_on( Args && ...args ) &&
			{
				return std::move(this->completed_on(std::forward<Args>(args)...));
			}

		/*!
		 * \brief Add a timeout handler for the async operation.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * class demo : public so_5::agent_t {
		 * 	...
		 * 	void initiate_async_op() {
		 *			asyncop::make<timeout>(*this)
		 *				.timeout_handler(
		 *					so_default_state(),
		 *					&demo::some_event )
		 *				.timeout_handler(
		 *					some_agent_state_,
		 *					[this](mhood_t<timeout> cmd) {...})
		 *				...
		 *				.activate(...);
		 * 	}
		 * };
		 * \endcode
		 *
		 * \note
		 * The timeout handler will be stored inside async operation
		 * data. Actual subscription for it will be made during activation
		 * of the async operation.
		 *
		 * \tparam Event_Handler Type of actual handler for message/signal.
		 * It can be a pointer to agent's method or lambda (or another
		 * type of functional object).
		 * Please notice that Event_Handler must receive a message/signal
		 * of type \a Message. It means that Event_Handler can have one of the
		 * following formats:
		 * \code
		 * ret_type handler(Message);
		 * ret_type handler(const Message &);
		 * ret_type handler(mhood_t<Message>); // This is the recommended format.
		 * \endcode
		 */
		template< typename Event_Handler >
		definition_point_t &
		timeout_handler(
			const ::so_5::state_t & state,
			Event_Handler && evt_handler ) &
			{
				this->timeout_handler_impl(
						state,
						std::forward<Event_Handler>(evt_handler) );

				return *this;
			}

		//! Just a proxy for the main version of %timeout_handler.
		template< typename... Args >
		definition_point_t &&
		timeout_handler( Args && ...args ) &&
			{
				return std::move(this->timeout_handler(
						std::forward<Args>(args)...));
			}

		/*!
		 * \brief Add the default timeout handler for the async operation.
		 *
		 * The default timeout handler will be called if no timeout
		 * handler was called for timeout message/signal.
		 * A deadletter handlers from SObjectizer-5.5.21 are used for
		 * implementation of the default timeout handler for async
		 * operation.
		 *
		 * There can be only one the default timeout handler.
		 * If the default timeout handler is already specified a new call
		 * to default_timeout_handler() will replace the old handler with
		 * new one (the old default timeout handler will be lost).
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * class demo : public so_5::agent_t {
		 * 	...
		 * 	void initiate_async_op() {
		 *			asyncop::make<timeout>(*this)
		 *				.default_timeout_handler(
		 *					&demo::some_deadletter_handler )
		 *				...
		 *				.activate(...);
		 * 	}
		 * };
		 * \endcode
		 *
		 * \note
		 * The default timeout handler will be stored inside async operation
		 * data. Actual subscription for it will be made during activation
		 * of the async operation.
		 *
		 * \tparam Event_Handler Type of actual handler for message/signal.
		 * It can be a pointer to agent's method or lambda (or another
		 * type of functional object).
		 * Please notice that Event_Handler must receive a message/signal
		 * of type \a Message. It means that Event_Handler can have one of the
		 * following formats:
		 * \code
		 * ret_type handler(Message);
		 * ret_type handler(const Message &);
		 * ret_type handler(mhood_t<Message>); // This is the recommended format.
		 * \endcode
		 */
		template< typename Event_Handler >
		definition_point_t &
		default_timeout_handler(
			//! The default timeout handler itself.
			Event_Handler && evt_handler ) &
			{
				this->default_timeout_handler_impl(
						std::forward<Event_Handler>(evt_handler) );

				return *this;
			}

		//! Just a proxy for the main version of %default_timeout_handler.
		template< typename... Args >
		definition_point_t &&
		default_timeout_handler( Args && ...args ) &&
			{
				return std::move(this->default_timeout_handler(
						std::forward<Args>(args)...));
			}

		/*!
		 * \brief Activation of the async operation.
		 *
		 * All subscriptions for completion and timeout handlers will be made
		 * here. Then the timeout message/signal will be sent. And then
		 * a cancellation_point for that async operation will be returned.
		 *
		 * An example of activation of the async operation in the case
		 * when \a Message is a signal:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * class demo : public so_5::agent_t {
		 * 	struct timeout final : public so_5::signal_t {};
		 * 	...
		 * 	void initiate_async_op() {
		 * 		asyncop::make<timeout>()
		 * 			... // Set up completion and timeout handlers...
		 * 			// Activation with just one argument - timeout duration.
		 * 			.activate(std::chrono::milliseconds(200));
		 * 		...
		 * 	}
		 * };
		 * \endcode
		 *
		 * An example of activation of the async operation in the case
		 * when \a Message is a message:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_limited;
		 * class demo : public so_5::agent_t {
		 * 	struct timeout final : public so_5::message_t {
		 * 		int id_;
		 * 		const std::string reply_payload_;
		 *			const so_5::mbox_t reply_mbox_;
		 *
		 *			timeout(int id, std::string playload, so_5::mbox_t mbox)
		 *				: id_(id), reply_payload_(std::move(payload), reply_mbox_(std::move(mbox))
		 *				{}
		 * 	};
		 * 	...
		 * 	void initiate_async_op() {
		 * 		asyncop::make<timeout>()
		 * 			... // Set up completion and timeout handlers...
		 * 			// Activation with several arguments.
		 * 			.activate(
		 * 				// This is timeout duration.
		 * 				std::chrono::milliseconds(200),
		 * 				// All these arguments will be passed to the constructor of timeout message.
		 * 				current_op_id,
		 * 				op_reply_payload,
		 * 				op_result_consumer_mbox);
		 * 		...
		 * 	}
		 * };
		 * \endcode
		 *
		 * \note
		 * There is no need to store the cancellation_point returned if you
		 * don't want to cancel async operation. The return value of
		 * activate() can be safely discarded in that case.
		 *
		 * \attention
		 * If cancellation_point is stored then it should be used only on
		 * the working context of the agent for that the async operation is
		 * created. And the cancellation_point should not be used in the
		 * thread-safe event handlers of that agent. It is because
		 * cancellation_point is not a thread safe object.
		 *
		 * \note
		 * If an exception is thrown during the activation procedure all
		 * completion and timeout handlers which were subscribed before
		 * the exception will be unsubscribed. And the async operation
		 * data will be deleted. It means that after an exception in
		 * activate() method the definition_point can't be used anymore.
		 */
		template< typename... Args >
		cancellation_point_t< Operation_Data >
		activate(
			//! The maximum duration for the async operation.
			//! Note: this should be non-negative value.
			std::chrono::steady_clock::duration timeout,
			//! Arguments for the construction of timeout message/signal
			//! of the type \a Message.
			//! This arguments will be passed to so_5::send_delayed
			//! function.
			Args && ...args ) &
			{
				this->ensure_activable();

				// From this point the definition_point will be detached
				// from operation data. It means that is_activable() will
				// return false.
				auto op = std::move(this->m_op);

				::so_5::details::do_with_rollback_on_exception( [&] {
						op->do_activation_actions();
						op->setup_timer_id(
								::so_5::send_periodic<Message>(
										op->timeout_mbox(),
										timeout,
										std::chrono::steady_clock::duration::zero(),
										std::forward<Args>(args)... ) );
						op->change_status( status_t::activated );
					},
					[&] {
						op->cancelled();
					} );

				return cancellation_point_t< Operation_Data >( std::move(op) );
			}

		//! Just a proxy for the main version of %activate.
		template< typename... Args >
		auto
		activate( Args && ...args ) &&
			{
				return this->activate( std::forward<Args>(args)... );
			}
	};

//
// make
//
/*!
 * \brief A factory for creation definition points of async operations.
 *
 * Instead of creation of definition_point_t object by hand this
 * helper function should be used.
 *
 * Usage examples:
 * \code
 * namespace asyncop = so_5::extra::async_op::time_limited;
 * class demo : public so_5::agent_t {
 * 	struct timeout final : public so_5::signal_t {};
 * 	...
 * 	void initiate_async_op() {
 * 		// Create a definition_point for a new async operation...
 * 		asyncop::make<timeout>(*this)
 * 			// ...then set up completion handler(s)...
 * 			.completed_on(
 * 				*this,
 * 				so_default_state(),
 * 				&demo::on_first_completion_msg )
 * 			.completed_on(
 * 				some_external_mbox_,
 * 				some_user_defined_state_,
 * 				[this](mhood_t<another_completion_msg> cmd) {...})
 * 			// ...then set up timeout handler(s)...
 * 			.timeout_handler(
 * 				so_default_state(),
 * 				&demo::on_timeout )
 * 			.timeout_handler(
 * 				some_user_defined_state_,
 * 				[this](mhood_t<timeout> cmd) {...})
 * 			// ...and now we can activate the operation.
 * 			.activate(std::chrono::milliseconds(300));
 * 		...
 * 	}
 * };
 * \endcode
 *
 * \since
 * v.1.0.4
 */
template< typename Message >
[[nodiscard]] definition_point_t< Message >
make(
	//! Agent for that this async operation will be created.
	::so_5::agent_t & owner )
	{
		return definition_point_t<Message>( ::so_5::outliving_mutable(owner) );
	}

} /* namespace time_limited */

} /* namespace async_op */

} /* namespace extra */

} /* namespace so_5 */

