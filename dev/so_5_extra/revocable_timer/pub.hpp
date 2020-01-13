/*!
 * \file
 * \brief Implementation of revocable timers
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5/version.hpp>

#include <so_5_extra/revocable_msg/pub.hpp>

#include <so_5_extra/error_ranges.hpp>

#include <so_5/timers.hpp>
#include <so_5/enveloped_msg.hpp>
#include <so_5/send_functions.hpp>

#include <atomic>

namespace so_5 {

namespace extra {

namespace revocable_timer {

namespace details {

//
// envelope_t
//
/*!
 * \brief A special envelope to be used for revocable timer messages.
 *
 * Just a synonim for so_5::extra::revocable_msg::details::envelope_t.
 *
 * \since
 * v.1.2.0
 */
using envelope_t = so_5::extra::revocable_msg::details::envelope_t;

} /* namespace details */

namespace impl {

// Just forward declaration. Definition will be below definition of timer_id_t.
struct timer_id_maker_t;

} /* namespace impl */

//
// timer_id_t
//
/*!
 * \brief The ID of revocable timer message/signal.
 *
 * This type plays the same role as so_5::timer_id_t. But provide
 * guaranteed revocation of delayed/periodic message/signal.
 *
 * There are several implementations of send_delayed() and send_periodic()
 * functions in so_5::extra::revocable_timer namespace. They all return
 * instances of timer_id_t.
 *
 * An instance of timer_id_t returned from send_delayed/send_periodic need
 * to be store somewhere. Otherwise the timer message will be revoked
 * just after completion of send_delayed/send_periodic function. It is
 * because the destructor of timer_id_t will be called and that destructor
 * revokes the timer message.
 *
 * An instance of timer_id_t can be used for revocation of a timer message.
 * Revocation can be performed by two ways:
 *
 * 1. Destructor of timer_id_t automatically revokes the timer message.
 * 2. Method timer_id_t::release() or timer_id_t::revoke() is called
 *    by an user.
 *
 * For example:
 * \code
 * namespace timer_ns = so_5::extra::revocable_timer;
 * void demo(so_5::mchain_t work_queue) {
 * 	// Send a delayed demand to work queue and store the ID returned.
 * 	auto id = timer_ns::send_delayed<flush_data>(work_queue, 10s, ...);
 * 	... // Do some work.
 * 	if(some_condition)
 * 		// Our previous message should be revoked if it is not delivered yet.
 * 		id.release();
 * 	...
 * 	// Message will be automatically revoked here because ID is destroyed
 * 	// on leaving the scope.
 * }
 * \endcode
 *
 * \note
 * The timer_id_t is Movable, not Copyable.
 *
 * \attention
 * This is not a thread-safe class. It means that it is dangerous to
 * call methods of that class (like revoke() or is_active()) from
 * different threads at the same time.
 *
 * \since
 * v.1.2.0
 */
class timer_id_t final
	{
		friend struct ::so_5::extra::revocable_timer::impl::timer_id_maker_t;

	private :
		//! The envelope that was sent.
		/*!
		 * \note Can be nullptr if default constructor was used.
		 */
		::so_5::intrusive_ptr_t< details::envelope_t > m_envelope;

		//! Timer ID for the envelope.
		::so_5::timer_id_t m_actual_id;

		timer_id_t(
			::so_5::intrusive_ptr_t< details::envelope_t > envelope,
			::so_5::timer_id_t actual_id )
			:	m_envelope{ std::move(envelope) }
			,	m_actual_id{ std::move(actual_id) }
			{}

	public :
		timer_id_t() = default;
		/*!
		 * \note The destructor automatically revokes the message if it is
		 * not delivered yet.
		 */
		~timer_id_t() noexcept
			{
				release();
			}

		// This class is not copyable.
		timer_id_t( const timer_id_t & ) = delete;
		timer_id_t & operator=( const timer_id_t & ) = delete;

		// But this class is moveable.
		timer_id_t( timer_id_t && ) noexcept = default;
		timer_id_t & operator=( timer_id_t && ) noexcept = default;

		friend void
		swap( timer_id_t & a, timer_id_t & b ) noexcept
			{
				a.m_envelope.swap( b.m_envelope );
				a.m_actual_id.swap( b.m_actual_id );
			}

		//! Is message delivery still in progress?
		/*!
		 * \note Please take care when using this method.
		 * Message delivery in SObjectizer is asynchronous operation.
		 * It means that you can receve \a true from is_active() but
		 * this value will already be obsolete because the message
		 * can be delivered just before return from is_active().
		 * The return value of is_active() can be useful in that context:
		 * \code
		 * namespace timer_ns = so_5::extra::revocable_timer;
		 * void demo(so_5::mchain_t work_queue) {
		 * 	auto id = timer_ns::send_delayed(work_queue, 10s, ...);
		 * 	... // Do some work.
		 * 	if(some_condition)
		 * 		id.revoke();
		 * 	... // Do some more work.
		 * 	if(another_condition)
		 * 		id.revoke();
		 * 	...
		 * 	if(id.is_active()) {
		 * 		// No previous calls to revoke().
		 * 		...
		 * 	}
		 * }
		 * \endcode
		 */
		bool
		is_active() const noexcept
			{
				return m_actual_id.is_active();
			}

		//! Revoke the message and release the timer.
		/*!
		 * \note
		 * It is safe to call release() for already revoked message.
		 */
		void
		release() noexcept
			{
				if( m_envelope )
					{
						m_envelope->revoke();
						m_actual_id.release();

						m_envelope.reset();
					}
			}

		//! Revoke the message and release the timer.
		/*!
		 * Just a synonym for release() method.
		 */
		void
		revoke() noexcept { release(); }
	};

namespace impl {

/*
 * This is helper for creation of initialized timer_id objects.
 */
struct timer_id_maker_t
	{
		template< typename... Args >
		[[nodiscard]] static auto
		make( Args && ...args )
			{
				return ::so_5::extra::revocable_timer::timer_id_t{
						std::forward<Args>(args)... };
			}
	};

/*
 * Helper function for actual sending of periodic message.
 */
[[nodiscard]]
inline so_5::extra::revocable_timer::timer_id_t
make_envelope_and_initiate_timer(
	const so_5::mbox_t & to,
	const std::type_index & msg_type,
	message_ref_t payload,
	std::chrono::steady_clock::duration pause,
	std::chrono::steady_clock::duration period )
	{
		using envelope_t = ::so_5::extra::revocable_timer::details::envelope_t;

		::so_5::intrusive_ptr_t< envelope_t > envelope{
				std::make_unique< envelope_t >( std::move(payload) ) };

		auto actual_id = ::so_5::low_level_api::schedule_timer( 
				msg_type,
				envelope,
				to,
				pause,
				period );

		return timer_id_maker_t::make(
				std::move(envelope), std::move(actual_id) );
	}

/*
 * This is helpers for send_delayed and send_periodic implementation.
 */

template< class Message, bool Is_Signal >
struct instantiator_and_sender_base
	{
		template< typename... Args >
		[[nodiscard]] static ::so_5::extra::revocable_timer::timer_id_t
		send_periodic(
			const ::so_5::mbox_t & to,
			std::chrono::steady_clock::duration pause,
			std::chrono::steady_clock::duration period,
			Args &&... args )
			{
				message_ref_t payload{
						so_5::details::make_message_instance< Message >(
								std::forward< Args >( args )...)
				};

				so_5::details::mark_as_mutable_if_necessary< Message >( *payload );

				return make_envelope_and_initiate_timer(
						to,
						message_payload_type< Message >::subscription_type_index(),
						std::move(payload),
						pause,
						period );
			}
	};

template< class Message >
struct instantiator_and_sender_base< Message, true >
	{
		//! Type of signal to be delivered.
		using actual_signal_type = typename message_payload_type< Message >::subscription_type;

		[[nodiscard]] static so_5::extra::revocable_timer::timer_id_t
		send_periodic(
			const so_5::mbox_t & to,
			std::chrono::steady_clock::duration pause,
			std::chrono::steady_clock::duration period )
			{
				return make_envelope_and_initiate_timer(
						to,
						message_payload_type< Message >::subscription_type_index(),
						message_ref_t{},
						pause,
						period );
			}
	};

template< class Message >
struct instantiator_and_sender
	:	public instantiator_and_sender_base<
			Message,
			is_signal< typename message_payload_type< Message >::payload_type >::value >
	{};

} /* namespace impl */

/*!
 * \brief A utility function for creating and delivering a periodic message
 * to the specified destination.
 *
 * Agent, mbox or mchain can be used as \a target.
 *
 * \note
 * Message chains with overload control must be used for periodic messages
 * with additional care because exceptions can't be thrown during
 * dispatching messages from timer.
 *
 * Usage example 1:
 * \code
 * namespace timer_ns = so_5::extra::revocable_timer;
 * class my_agent : public so_5::agent_t {
 * 	timer_ns::timer_id_t timer_;
 * 	...
 * 	void so_evt_start() override {
 * 		...
 * 		// Initiate a periodic message to self.
 * 		timer_ = timer_ns::send_periodic<do_some_task>(*this,	1s, 1s, ...);
 * 		...
 * 	}
 * 	...
 * };
 * \endcode
 *
 * Usage example 2:
 * \code
 * so_5::wrapped_env_t sobj; // SObjectizer is started here.
 * // Create a worker and get its mbox.
 * so_5::mbox_t worker_mbox = sobj.environment().introduce_coop(
 * 	[&](so_5::coop_t & coop) {
 * 		auto worker = coop.make_agent<worker_agent>(...);
 * 		return worker->so_direct_mbox();
 * 	});
 * // Send revocable periodic message to the worker.
 * auto timer_id = so_5::extra::revocable_timer::send_periodic<tell_status>(
 * 		worker_mbox(),
 * 		1s, 1s,
 * 		... );
 * ... // Do some work.
 * // Revoke the tell_status message.
 * timer_id.release();
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the periodic timer will be cancelled automatically just right after
 * send_periodic returns.
 *
 * \attention
 * Values of \a pause and \a period should be non-negative.
 *
 * \tparam Message type of message or signal to be sent.
 * \tparam Target can be so_5::agent_t, so_5::mbox_t or so_5::mchain_t.
 * \tparam Args list of arguments for Message's constructor.
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename Target, typename... Args >
[[nodiscard]] timer_id_t
send_periodic(
	//! A destination for the periodic message.
	Target && target,
	//! Pause for message delaying.
	std::chrono::steady_clock::duration pause,
	//! Period of message repetitions.
	std::chrono::steady_clock::duration period,
	//! Message constructor parameters.
	Args&&... args )
	{
		return impl::instantiator_and_sender< Message >::send_periodic(
				::so_5::send_functions_details::arg_to_mbox( target ),
				pause,
				period,
				std::forward< Args >( args )... );
	}

/*!
 * \brief A utility function for delivering a periodic
 * from an existing message hood.
 *
 * \attention Message must not be a mutable message if \a period is not 0.
 * Otherwise an exception will be thrown.
 *
 * \tparam Message a type of message to be redirected (it can be
 * in form of Msg, so_5::immutable_msg<Msg> or so_5::mutable_msg<Msg>).
 *
 * Usage example:
 * \code
	namespace timer_ns = so_5::extra::revocable_timer;
	class redirector : public so_5::agent_t {
		...
		void on_some_immutable_message(mhood_t<first_msg> cmd) {
			timer_id = timer_ns::send_periodic(
					another_mbox,
					std::chrono::seconds(1),
					std::chrono::seconds(15),
					cmd);
			...
		}

		void on_some_mutable_message(mhood_t<mutable_msg<second_msg>> cmd) {
			timer_id = timer_ns::send_periodic(
					another_mbox,
					std::chrono::seconds(1),
					std::chrono::seconds(20),
					std::move(cmd));
			// Note: cmd is nullptr now, it can't be used anymore.
			...
		}
	};
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the periodic timer will be cancelled automatically just right after
 * send_periodic returns.
 *
 * \attention
 * Values of \a pause and \a period should be non-negative.
 *
 * \since
 * v.1.2.0
 */
template< typename Message >
[[nodiscard]]
typename std::enable_if<
		!::so_5::is_signal< Message >::value,
		timer_id_t >::type
send_periodic(
	//! Mbox for the message to be sent to.
	const ::so_5::mbox_t & to,
	//! Pause for message delaying.
	std::chrono::steady_clock::duration pause,
	//! Period of message repetitions.
	std::chrono::steady_clock::duration period,
	//! Existing message hood for message to be sent.
	::so_5::mhood_t< Message > mhood )
	{
		return impl::make_envelope_and_initiate_timer(
				to,
				message_payload_type< Message >::subscription_type_index(),
				mhood.make_reference(),
				pause,
				period );
	}

/*!
 * \brief A utility function for periodic redirection of a signal
 * from existing message hood.
 *
 * \tparam Message a type of signal to be redirected (it can be
 * in form of Sig or so_5::immutable_msg<Sig>).
 *
 * Usage example:
 * \code
	class redirector : public so_5::agent_t {
		...
		void on_some_immutable_signal(mhood_t<some_signal> cmd) {
			timer_id = so_5::extra::revocable_timer::send_periodic(
					another_mbox,
					std::chrono::seconds(1),
					std::chrono::seconds(10),
					cmd);
			...
		}
	};
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the periodic timer will be cancelled automatically just right after
 * send_periodic returns.
 *
 * \attention
 * Values of \a pause and \a period should be non-negative.
 *
 * \since
 * v.1.2.0
 */
template< typename Message >
[[nodiscard]]
typename std::enable_if<
		::so_5::is_signal< Message >::value,
		timer_id_t >::type
send_periodic(
	//! Mbox for the message to be sent to.
	const ::so_5::mbox_t & to,
	//! Pause for message delaying.
	std::chrono::steady_clock::duration pause,
	//! Period of message repetitions.
	std::chrono::steady_clock::duration period,
	//! Existing message hood for message to be sent.
	::so_5::mhood_t< Message > /*mhood*/ )
	{
		return impl::make_envelope_and_initiate_timer(
				to,
				message_payload_type< Message >::subscription_type_index(),
				message_ref_t{},
				pause,
				period );
	}

/*!
 * \brief A helper function for redirection of a message/signal as a periodic
 * message/signal.
 *
 * This function can be used if \a target is a reference to agent or if
 * \a target is a mchain
 *
 * Example usage:
 * \code
 * namespace timer_ns = so_5::extra::revocable_timer;
 * class my_agent : public so_5::agent_t {
 * ...
 * 	so_5::mchain_t target_mchain_;
 * 	timer_ns::timer_id_t periodic_msg_id_;
 * ...
 * 	void on_some_msg(mhood_t<some_msg> cmd) {
 * 		if( ... )
 * 			// Message should be resend as a periodic message.
 * 			periodic_msg_id_ = timer_ns::send_periodic(target_mchain_, 10s, 20s, std::move(cmd));
 * 	}
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the periodic timer will be cancelled automatically just right after
 * send_periodic returns.
 *
 * \attention
 * Values of \a pause and \a period should be non-negative.
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename Target >
[[nodiscard]] timer_id_t
send_periodic(
	//! A target for periodic message/signal.
	//! It can be a reference to a target agent or a mchain_t.
	Target && target,
	//! Pause for the first occurence of the message/signal.
	std::chrono::steady_clock::duration pause,
	//! Period of message repetitions.
	std::chrono::steady_clock::duration period,
	//! Existing message hood for message/signal to be sent.
	::so_5::mhood_t< Message > mhood )
	{
		return ::so_5::extra::revocable_timer::send_periodic< Message >(
				::so_5::send_functions_details::arg_to_mbox( target ),
				pause,
				period,
				std::move(mhood) );
	}

/*!
 * \brief A utility function for creating and delivering a delayed message
 * to the specified destination.
 *
 * Agent, mbox or mchain can be used as \a target.
 *
 * \note
 * Message chains with overload control must be used for periodic messages
 * with additional care because exceptions can't be thrown during
 * dispatching messages from timer.
 *
 * Usage example 1:
 * \code
 * namespace timer_ns = so_5::extra::revocable_timer;
 * class my_agent : public so_5::agent_t {
 * 	timer_ns::timer_id_t timer_;
 * 	...
 * 	void so_evt_start() override {
 * 		...
 * 		// Initiate a delayed message to self.
 * 		timer_ = timer_ns::send_periodic<kill_youself>(*this,	60s, ...);
 * 		...
 * 	}
 * 	...
 * };
 * \endcode
 *
 * Usage example 2:
 * \code
 * so_5::wrapped_env_t sobj; // SObjectizer is started here.
 * // Create a worker and get its mbox.
 * so_5::mbox_t worker_mbox = sobj.environment().introduce_coop(
 * 	[&](so_5::coop_t & coop) {
 * 		auto worker = coop.make_agent<worker_agent>(...);
 * 		worker_mbox = worker->so_direct_mbox();
 * 	});
 * // Send revocable delayed message to the worker.
 * auto timer_id = so_5::extra::revocable_timer::send_periodic<kill_yourself>(
 * 		worker_mbox(),
 * 		60s,
 * 		... );
 * ... // Do some work.
 * // Revoke the kill_yourself message.
 * timer_id.release();
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the delayed timer will be cancelled automatically just right after
 * send_delayed returns.
 *
 * \attention
 * Value of \a pause should be non-negative.
 *
 * \tparam Message type of message or signal to be sent.
 * \tparam Target can be so_5::agent_t, so_5::mbox_t or so_5::mchain_t.
 * \tparam Args list of arguments for Message's constructor.
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename Target, typename... Args >
[[nodiscard]] timer_id_t
send_delayed(
	//! A destination for the periodic message.
	Target && target,
	//! Pause for message delaying.
	std::chrono::steady_clock::duration pause,
	//! Message constructor parameters.
	Args&&... args )
	{
		return ::so_5::extra::revocable_timer::send_periodic< Message >(
				::so_5::send_functions_details::arg_to_mbox( target ),
				pause,
				std::chrono::steady_clock::duration::zero(),
				std::forward<Args>(args)... );
	}

/*!
 * \brief A helper function for redirection of existing message/signal
 * as delayed message.
 *
 * Usage example:
 * \code
 * namespace timer_ns = so_5::extra::revocable_timer;
 * class my_agent : public so_5::agent_t {
 * 	const so_5::mbox_t another_worker_;
 *		timer_ns::timer_id_t timer_;
 * 	...
 * 	void on_some_msg(mhood_t<some_message> cmd) {
 * 		// Redirect this message to another worker with delay in 250ms.
 * 		timer_ = timer_ns::send_delayed(
 * 				another_worker_,
 * 				std::chrono::milliseconds(250),
 * 				std::move(cmd));
 * 		...
 * 	}
 * };
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the delayed timer will be cancelled automatically just right after
 * send_delayed returns.
 *
 * \attention
 * Value of \a pause should be non-negative.
 *
 * \tparam Message type of message or signal to be sent.
 *
 * \since
 * v.1.2.0
 */
template< typename Message >
[[nodiscard]] timer_id_t
send_delayed(
	//! Mbox for the message to be sent to.
	const so_5::mbox_t & to,
	//! Pause for message delaying.
	std::chrono::steady_clock::duration pause,
	//! Message to redirect.
	::so_5::mhood_t< Message > cmd )
	{
		return ::so_5::extra::revocable_timer::send_periodic< Message >(
				to,
				pause,
				std::chrono::steady_clock::duration::zero(),
				std::move(cmd) );
	}

/*!
 * \brief A helper function for redirection of existing message/signal
 * as delayed message.
 *
 * Agent or mchain can be used as \a target.
 *
 * Usage example:
 * \code
 * namespace timer_ns = so_5::extra::revocable_timer;
 * class my_agent : public so_5::agent_t {
 * 	const so_5::mchain_t work_queue_;
 *		timer_ns::timer_id_t timer_;
 * 	...
 * 	void on_some_msg(mhood_t<some_message> cmd) {
 * 		// Redirect this message to another worker with delay in 250ms.
 * 		timer_ = timer_ns::send_delayed(work_queue_,
 * 				std::chrono::milliseconds(250),
 * 				std::move(cmd));
 * 		...
 * 	}
 * };
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the delayed timer will be cancelled automatically just right after
 * send_delayed returns.
 *
 * \attention
 * Value of \a pause should be non-negative.
 *
 * \tparam Message type of message or signal to be sent.
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename Target >
[[nodiscard]] timer_id_t
send_delayed(
	//! A destination for the periodic message.
	Target && target,
	//! Pause for message delaying.
	std::chrono::steady_clock::duration pause,
	//! Message to redirect.
	::so_5::mhood_t< Message > cmd )
	{
		return ::so_5::extra::revocable_timer::send_periodic(
				std::forward<Target>(target),
				pause,
				std::chrono::steady_clock::duration::zero(),
				std::move(cmd) );
	}

} /* namespace revocable_timer */

} /* namespace extra */

} /* namespace so_5 */

