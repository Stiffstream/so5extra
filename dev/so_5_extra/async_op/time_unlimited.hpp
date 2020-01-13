/*!
 * \file
 * \brief Implementation of time-unlimited asynchronous one-time operation.
 *
 * \since
 * v.1.0.4
 */

#pragma once

#include <so_5_extra/async_op/details.hpp>
#include <so_5_extra/async_op/errors.hpp>

#include <so_5/details/invoke_noexcept_code.hpp>

#include <so_5/agent.hpp>

#include <so_5/outliving.hpp>

#include <vector>

namespace so_5 {

namespace extra {

namespace async_op {

namespace time_unlimited {

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
		cancelled
	};

// Forward declarations.
// Necessary to declare friends for details::op_data_t.

template< typename Operation_Data >
class cancellation_point_t;

template< typename Operation_Data >
class definition_point_t;

namespace details
{

/*!
 * \brief A main class for implementation of time-unlimited
 * asynchronous one-time operation.
 *
 * This class contains an information about completion handlers for
 * async operation.
 *
 * Do not create objects of this class directly (unless you really know
 * what you are doing).
 * Use so_5::extra::async_op::time_unlimited::make() helper function for that.
 *
 * \note
 * Instances of that class should be created only as dynamically allocated
 * objects. It is because smart pointers are created inside the instance
 * (for example completed_on() creates intrusive_ptr_t for `this`). Because
 * of that this class has a protected destructor.
 *
 * \attention
 * This is not a thread-safe type. It is beter and safer to work with
 * an object of that type inside an agent for which that object is created.
 *
 * \since
 * v.1.0.4
 */
class op_data_t : protected ::so_5::atomic_refcounted_t
	{
		friend class ::so_5::intrusive_ptr_t<op_data_t>;

		template< typename Operation_Data >
		friend class ::so_5::extra::async_op::time_unlimited::cancellation_point_t;
		template< typename Operation_Data >
		friend class ::so_5::extra::async_op::time_unlimited::definition_point_t;

	private :
		//! Description of one subscription.
		struct subscription_data_t
			{
				//! Mbox from that a message is expected.
				::so_5::mbox_t m_mbox;

				//! State for that a subscription should be created.
				/*!
				 * \attention
				 * Can't be nullptr.
				 */
				const ::so_5::state_t * m_state;

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
				subscription_data_t(
					so_5::mbox_t mbox,
					const so_5::state_t & state,
					std::type_index subscription_type,
					so_5::event_handler_method_t handler )
					:	m_mbox( std::move(mbox) )
					,	m_state( &state )
					,	m_subscription_type( std::move(subscription_type) )
					,	m_handler( std::move(handler) )
					{}
			};

		//! Owner of async operation.
		::so_5::outliving_reference_t<::so_5::agent_t> m_owner;

		//! The status of the async operation.
		status_t m_status = status_t::not_activated;

		//! Subscriptions which should be created on activation.
		std::vector< subscription_data_t > m_subscriptions;

	protected :
		// NOTE: constructor and destructor will be available only
		// for friends and for derived classes.

		//! Initializing constructor.
		op_data_t(
			//! An agent which owns async operation.
			::so_5::outliving_reference_t<::so_5::agent_t> owner )
			:	m_owner(owner)
			{}

		~op_data_t() noexcept
			{}

		//! Reserve a capacity for vector with subscriptions' data.
		void
		reserve( std::size_t capacity )
			{
				m_subscriptions.reserve( capacity );
			}

		//! Add an operation completion handler.
		/*!
		 * This method stores description of a competion handler. This
		 * description will be used later in activate() method for subscription
		 * to a completion message/signal.
		 *
		 * \tparam Operation_Data An actual type of operation data object.
		 * In the most cases it will be op_data_t itself. But for debugging or
		 * testing purposes it can also be a derived class. This type is
		 * necessary to store the right smart pointer to actual operation
		 * data object in event handler wrapper.
		 *
		 * \tparam Msg_Target A type of destination for message/signal about
		 * the completion of the async operation.
		 * It can be a const reference to so_5::mbox_t, so_5::agent_t,
		 * so_5::adhoc_agent_definition_proxy_t.
		 *
		 * \tparam Event_Handler A type of handler for message/signal about
		 * the completion of the async operation.
		 */
		template<
				typename Operation_Data,
				typename Msg_Target,
				typename Event_Handler >
		void
		add_completion_handler(
			//! A smart pointer to the actual instance of operation data.
			::so_5::intrusive_ptr_t< Operation_Data > actual_data,
			//! A destination for message about operation completion.
			Msg_Target && msg_target,
			//! A state in which message should be handled.
			const ::so_5::state_t & state,
			//! A message handler.
			Event_Handler && evt_handler )
			{
				ensure_not_activated();

				const auto mbox = ::so_5::extra::async_op::details::target_to_mbox(
						msg_target );

				auto evt_handler_info =
						::so_5::preprocess_agent_event_handler(
								mbox,
								m_owner.get(),
								std::forward<Event_Handler>(evt_handler) );

				::so_5::event_handler_method_t actual_handler =
					[self = std::move(actual_data),
					user_handler = std::move(evt_handler_info.m_handler)](
						::so_5::message_ref_t & msg )
					{
						self->completed();
						user_handler( msg );
					};

				m_subscriptions.emplace_back(
						mbox,
						state,
						evt_handler_info.m_msg_type,
						std::move(actual_handler) );
			}

		/*!
		 * \brief Performs all necessary activation actions.
		 *
		 * \note
		 * It throws if:
		 * - the operation is already activated;
		 * - there is no defined completion handlers.
		 */
		void
		activate()
			{
				ensure_not_activated();

				if( !m_subscriptions.empty() )
					{
						create_subscriptions();
						m_status = status_t::activated;
					}
				else
					SO_5_THROW_EXCEPTION(
							::so_5::extra::async_op::errors::rc_no_completion_handler,
							"Operation can't be activated without any completion "
							"handler" );
			}

		//! Cancel async operation.
		/*!
		 * If an async operation is in progress then all subscriptions
		 * will be destroyed and all information about completion handlers
		 * will be erased.
		 *
		 * \note
		 * It is safe to cancel an operation which wasn't activacted or
		 * was already finished.
		 * If cancel() is called before activate() then all information
		 * about completion handlers created by previous calls to
		 * add_completion_handler() will be lost.
		 *
		 * \attention
		 * This method is marked as noexcept because event unsubscription
		 * operations shouldn't throw. But if such operation throws then
		 * there is no way to recover.
		 */
		void
		cancel() noexcept
			{
				if( status_t::activated == m_status )
					{
						destroy_and_clear_subscriptions();
						m_status = status_t::cancelled;
					}
				else if( status_t::not_activated == m_status )
					{
						m_subscriptions.clear();
					}
			}

		//! Get the current status of the operation.
		[[nodiscard]] status_t
		current_status() const noexcept
			{
				return m_status;
			}

		//! Is there any completion handler?
		[[nodiscard]] bool
		has_completion_handlers() const noexcept
			{
				return !m_subscriptions.empty();
			}

	private :
		//! Check if operation is activated and throw an exception if it is.
		void
		ensure_not_activated()
			{
				if( status_t::activated == m_status )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::async_op::errors::rc_async_op_activated,
							"Operation can't be performed when async_op is already "
							"activated" );
			}

		//! Perform operation completion procedure.
		/*!
		 * All subscriptions will be destroyed. All information about
		 * subscriptions will be deleted.
		 *
		 * Status will be changed to status_t::completed.
		 */
		void
		completed() noexcept
			{
				destroy_and_clear_subscriptions();
				m_status = status_t::completed;
			}

		//! Subscribe agent for all subscriptions in the subscriptions' container.
		/*!
		 * \note
		 * If an exception is thrown during subscription then all previously
		 * created subscriptions will be destroyed.
		 */
		void
		create_subscriptions()
			{
				// All subscriptions must be destroyed if an exception
				// is thrown.
				std::size_t current_index = 0;
				try
					{
						for(; current_index != m_subscriptions.size(); ++current_index )
							{
								auto & sd = m_subscriptions[ current_index ];
								m_owner.get().so_create_event_subscription(
										sd.m_mbox,
										sd.m_subscription_type,
										*(sd.m_state),
										sd.m_handler,
										::so_5::thread_safety_t::unsafe,
										::so_5::event_handler_kind_t::final_handler );
							}
					}
				catch(...)
					{
						// All created subscriptions should be dropped.
						destroy_subscriptions_up_to( current_index );

						throw;
					}
			}

		//! Destroy all subscriptions and clean subscriptions' container.
		void
		destroy_and_clear_subscriptions() noexcept
			{
				destroy_subscriptions_up_to( m_subscriptions.size() );
				m_subscriptions.clear();
			}

		//! Destroy subscriptions in range [0..n).
		void
		destroy_subscriptions_up_to(
			//! Last index of subscription in m_subscriptions container
			//! which shouldn't be included.
			const std::size_t n ) noexcept
			{
				for( std::size_t i = 0; i != n; ++i )
					{
						const auto & sd = m_subscriptions[ i ];
						m_owner.get().so_destroy_event_subscription(
								sd.m_mbox,
								sd.m_subscription_type,
								*(sd.m_state) );
					}
			}
	};

//
// op_shptr_t
//
/*!
 * \brief An alias for smart pointer to operation data.
 *
 * \since
 * v.1.0.4
 */
template< typename Operation_Data >
using op_shptr_t = ::so_5::intrusive_ptr_t< Operation_Data >;

} /* namespace details */

/*!
 * \brief An object that allows to cancel async operation.
 *
 * Usage example:
 * \code
 * namespace asyncop = so_5::extra::async_op::time_unlimited;
 * class demo : public so_5::agent_t {
 * 	asyncop::cancellation_point_t<> cp_;
 * 	...
 * 	void initiate_async_op() {
 * 		auto op = asyncop::make<timeout>(*this);
 * 		op.completed_on(...);
 * 		cp_ = op.activate(...);
 * 	}
 * 	void on_interruption_signal(mhood_t<interrupt_activity> cmd) {
 * 		// Operation should be cancelled.
 * 		cp_.cancel();
 * 		...
 * 	}
 * };
 * \endcode
 *
 * \note
 * This class is DefaultConstructible and Moveable, but not Copyable
 * and not CopyConstructible.
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
		template<typename Op_Data> friend class definition_point_t;

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
		 * data and this async operation is not completed yet.
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
		 * cancelled or completed. In that case the call to
		 * cancel() will have no effect.
		 */
		void
		cancel() noexcept
			{
				if( is_cancellable() )
					{
						m_op->cancel();
					}
			}

		//! Throw out a reference to the async operation data.
		/*!
		 * A cancellation_point holds a reference to the async operation
		 * data. It means that the async operation data will be destroyed
		 * only when the cancellation_point will be destroyed. For example,
		 * in that case:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_unlimited;
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
		 * completed or cancelled, then the operation data
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
 * namespace asyncop = so_5::extra::async_op::time_unlimited;
 * class demo : public so_5::agent_t {
 * 	...
 * 	void initiate_async_op() {
 * 		// Create a definition_point for a new async operation...
 * 		asyncop::make(*this)
 * 			// ...then set up completion handler(s)...
 * 			.completed_on(
 * 				*this,
 * 				so_default_state(),
 * 				&demo::on_first_completion_msg )
 * 			.completed_on(
 * 				some_external_mbox_,
 * 				some_user_defined_state_,
 * 				[this](mhood_t<another_completion_msg> cmd) {...})
 * 			// ...and now we can activate the operation.
 * 			.activate();
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
 * namespace asyncop = so_5::extra::async_op::time_unlimited;
 * class demo : public so_5::agent_t {
 * 	...
 * 	// Cancellation point for the async operation.
 * 	asyncop::cancellation_point_t<> cp_;
 * 	...
 * 	void initiate_async_op() {
 * 		// Create a definition_point for a new async operation
 * 		// and store the cancellation point after activation.
 * 		cp_ = asyncop::make(*this)
 * 			// ...then set up completion handler(s)...
 * 			.completed_on(
 * 				*this,
 * 				so_default_state(),
 * 				&demo::on_first_completion_msg )
 * 			.completed_on(
 * 				some_external_mbox_,
 * 				some_user_defined_state_,
 * 				[this](mhood_t<another_completion_msg> cmd) {...})
 * 			// ...and now we can activate the operation.
 * 			.activate();
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
 * There are two forms of activate() method. The first one doesn't receive
 * any arguments. It was shown in the examples above. The second one receives
 * the lambda-function as argument. It can be used when some addtional
 * actions should be performed during the activation of the async operation.
 * For example:
 * \code
 * void initiate_async_op() {
 * 	asyncop::make(*this)
 * 	.completed_on(...)
 * 	.completed_on(...)
 * 	.activate([this] {
 * 			// Several messages must be sent at the start of async op.
 * 			so_5::send<first_initial_msg>(some_target, ...);
 * 			so_5::send<second_initial_msg>(some_target, ...);
 * 			...
 * 		});
 * }
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
	typename Operation_Data = details::op_data_t >
class definition_point_t
	{
		//! Actual operation data.
		/*!
		 * \note
		 * This pointer can be nullptr after activation or after the
		 * content of the object is moved away.
		 */
		details::op_shptr_t< Operation_Data > m_op;

		//! Checks that the definition_point owns async operation data.
		void
		ensure_not_empty() const
			{
				if( !m_op )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::async_op::errors::rc_empty_definition_point_object,
							"an attempt to use empty definition_point object" );
			}

	public :
		//! Initializing constructor.
		definition_point_t(
			//! The owner of the async operation.
			::so_5::outliving_reference_t< ::so_5::agent_t > owner )
			:	m_op( new Operation_Data( owner ) )
			{}

		~definition_point_t()
			{
				// If operation data is still here then it means that
				// there wasn't call to `activate()` and we should cancel
				// all described handlers.
				// This will lead to deallocation of operation data.
				if( this->m_op )
					this->m_op->cancel();
			}

		definition_point_t( const definition_point_t & ) = delete;
		definition_point_t &
		operator=( const definition_point_t & ) = delete;

		definition_point_t( definition_point_t && ) = default;
		definition_point_t &
		operator=( definition_point_t && ) = default;

		/*!
		 * \brief Reserve a space for storage of completion handlers.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_unlimited;
		 * auto op = asyncop::make(some_agent);
		 * // Reserve space for four completion handlers.
		 * op.reserve_completion_handlers_capacity(4);
		 * op.completed_on(...);
		 * op.completed_on(...);
		 * op.completed_on(...);
		 * op.completed_on(...);
		 * op.activate();
		 * \endcode
		 */
		definition_point_t &
		reserve_completion_handlers_capacity(
			//! A required capacity.
			std::size_t capacity ) &
			{
				ensure_not_empty();

				m_op->reserve( capacity );

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
		 * \brief Checks if the async operation can be activated.
		 *
		 * The operation can be activated if the definition_point still
		 * holds the operation data (e.g. operation is not activated yet) and
		 * there is at least one completion handler for the operation.
		 */
		[[nodiscard]] bool
		is_activable() const noexcept
			{
				// Operation is activable if we still hold the operation data.
				return m_op && m_op->has_completion_handlers();
			}

		/*!
		 * \brief Add a completion handler for the async operation.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_unlimited;
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
				ensure_not_empty();

				this->m_op->add_completion_handler(
						this->m_op,
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

		//! Activate async operation with addition starting action.
		/*!
		 * This method performs two steps:
		 *
		 * 1. Activates the async operation.
		 * 2. Calls \a action lambda-function (or functional object).
		 *
		 * It an exception is thrown from \a action then the activated
		 * async operation will be cancelled automatically.
		 *
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_unlimited;
		 * class demo : public so_5::agent_t {
		 * public:
		 * 	...
		 * 	virtual void so_evt_start() override {
		 * 		// Set up operation completion handlers...
		 * 		asyncop::make(*this)
		 * 			.completed_on(...)
		 * 			.completed_on(...)
		 * 			.completed_on(...)
		 * 			// And now the operation can be activated.
		 * 			.activate([this] {
		 * 				... // Some initial actions like sending a message...
		 * 			});
		 * 		...
		 * 	}
		 * };
		 * \endcode
		 *
		 * \attention
		 * It throws if `!is_activable()`.
		 *
		 * \attention
		 * If an exception is thrown during the activation procedure then
		 * the definition_point will become empty. It means that after an
		 * exception from activate() the definition_point object should not
		 * be used.
		 *
		 * \tparam Activation_Action A type of lambda-function or functional
		 * object to be excecuted on activation of operation.
		 */
		template< typename Activation_Action >
		cancellation_point_t< Operation_Data >
		activate(
			//! A lambda-function or functional object which will be called
			//! after creation of all necessary subscriptions and switching
			//! to activated status.
			Activation_Action && action ) &
			{
				if( !this->is_activable() )
					SO_5_THROW_EXCEPTION(
							::so_5::extra::async_op::errors::rc_op_cant_be_activated,
							"definition_point_t doesn't hold operation data anymore." );
				auto op = std::move(m_op);

				op->activate();

				::so_5::details::do_with_rollback_on_exception(
					[&] { action(); },
					[&] { op->cancel(); } );

				return { std::move(op) };
			}

		//! Activate async operation.
		/*!
		 * Usage example:
		 * \code
		 * namespace asyncop = so_5::extra::async_op::time_unlimited;
		 * class demo : public so_5::agent_t {
		 * public:
		 * 	...
		 * 	virtual void so_evt_start() override {
		 * 		// Create an async operation...
		 * 		asyncop::make(*this)
		 * 			// Then set up completion handlers...
		 * 			->completed_on(...)
		 * 			.completed_on(...)
		 * 			.completed_on(...)
		 * 			// And now the operation can be activated.
		 * 			.activate();
		 * 		...
		 * 	}
		 * };
		 * \endcode
		 *
		 * \attention
		 * It throws if `!is_activable()`.
		 *
		 * \attention
		 * If an exception is thrown during the activation procedure then
		 * the definition_point will become empty. It means that after an
		 * exception from activate() the definition_point object should not
		 * be used.
		 */
		cancellation_point_t< Operation_Data >
		activate() &
			{
				return this->activate( []{/* Nothing to do*/} );
			}

		//! Just a proxy for actual %activate() methods.
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
 * \brief Helper function for creation an instance of async operation.
 *
 * Instead of creation of op_data_t's instances directly by hand this
 * factory function should be used:
 * \code
 * namespace asyncop = so_5::extra::async_op::time_unlimited;
 * class demo : public so_5::agent_t {
 * public:
 * 	...
 * 	virtual void so_evt_start() override {
 * 		// Create an async operation...
 * 		asyncop::make(*this)
 * 			// ...set up operation completion handlers...
 * 			.completed_on(...)
 * 			.completed_on(...)
 * 			.completed_on(...)
 * 			// And now the operation can be activated.
 * 			.activate();
 * 		...
 * 	}
 * };
 * \endcode
 *
 * \since
 * v.1.0.4
 */
[[nodiscard]] inline definition_point_t<details::op_data_t>
make(
	//! Agent for that this async operation will be created.
	::so_5::agent_t & owner )
	{
		return { ::so_5::outliving_mutable(owner) };
	}

} /* namespace time_unlimited */

} /* namespace async_op */

} /* namespace extra */

} /* namespace so_5 */

