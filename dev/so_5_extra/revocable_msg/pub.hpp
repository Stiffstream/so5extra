/*!
 * \file
 * \brief Implementation of revocable messages.
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5/version.hpp>

#include <so_5_extra/error_ranges.hpp>

#include <so_5/enveloped_msg.hpp>
#include <so_5/send_functions.hpp>

#include <atomic>

namespace so_5 {

namespace extra {

namespace revocable_msg {

namespace errors {

//! Mutability of envelope for revocable message can't be changed.
/*!
 * An envelope for a revocable message inherits mutability flag
 * from its payload. It means that mutability should be set for payload
 * first and it can't be changed after enveloping the payload into
 * the special envelope.
 *
 * \since
 * v.1.2.0
 */
const int rc_mutabilty_of_envelope_cannot_be_changed =
	so_5::extra::errors::revocable_msg_errors + 1;

//! An attempt to envelope service request.
/*!
 * Special envelope type, so_5::extra::revocable_msg::details::envelope_t
 * should be used only with messages, signals and envelopes.
 * Service requests can't be enveloped by this type of envelope.
 *
 * \since
 * v.1.2.0
 */
const int rc_invalid_payload_kind =
	so_5::extra::errors::revocable_msg_errors + 2;

} /* namespace errors */

namespace details {

//
// envelope_t
//
/*!
 * \brief A special envelope to be used for revocable messages.
 *
 * This envelope uses an atomic flag. When this flag is set to \a true
 * the message becomes _revoked_. Value of this flag is checked in
 * access_hook(). If the message if revoked that handler do nothing.
 *
 * \note
 * This class is intended to be used with invocation_type_t::event 
 * and invocation_type_t::enveloped_msg.
 * Service requests are not supported.
 *
 * \since
 * v.1.2.0
 */
class envelope_t final : public so_5::enveloped_msg::envelope_t
	{
		//! Has message been revoked?
		std::atomic_bool m_revoked{ false };

		//! Message to be delivered.
		so_5::message_ref_t m_payload;

		void
		do_if_not_revoked_yet(
			handler_invoker_t & invoker ) const noexcept
			{
				if( !has_been_revoked() )
					{
						// Message is not revoked yet.
						// Message handler can be called.
						invoker.invoke( payload_info_t{ m_payload } );
					}
				// Otherwise message should be ignored.
			}

		// Mutability of payload will be returned as mutability
		// of the whole envelope.
		message_mutability_t
		so5_message_mutability() const noexcept override
			{
				return message_mutability( m_payload );
			}

		// Disables changing of mutability by throwing an exception.
		void
		so5_change_mutability(
			message_mutability_t ) override
			{
				SO_5_THROW_EXCEPTION(
						so_5::extra::revocable_msg::errors
								::rc_mutabilty_of_envelope_cannot_be_changed,
						"revocable_msg's envelope prohibit changing of "
						"message mutability" );
			}

	public :
		envelope_t( so_5::message_ref_t payload )
			:	m_payload{ std::move(payload) }
			{}

		void
		revoke() noexcept
			{
				m_revoked.store( true, std::memory_order_release );
			}

		bool
		has_been_revoked() const noexcept
			{
				return m_revoked.load( std::memory_order_acquire );
			}

		void
		access_hook(
			access_context_t /*context*/,
			handler_invoker_t & invoker ) noexcept override
			{
				if( !has_been_revoked() )
					{
						// Message is not revoked yet.
						// Message handler can be called.
						invoker.invoke( payload_info_t{ m_payload } );
					}
				// Otherwise message should be ignored.
			}
	};

} /* namespace details */

namespace impl {

// Just forward declaration. Definition will be below definition of delivery_id_t.
struct delivery_id_maker_t;

} /* namespace impl */

//
// delivery_id_t
//
/*!
 * \brief The ID of revocable message/signal.
 *
 * An instance of delivery_id_t returned from send()-functions need
 * to be store somewhere. Otherwise the message/signal will be revoked
 * just after completion of send() function. It is
 * because the destructor of delivery_id_t will be called and that destructor
 * revokes the message/signal.
 *
 * An instance of delivery_id_t can be used for revocation of a message.
 * Revocation can be performed by two ways:
 *
 * 1. Destructor of delivery_id_t automatically revokes the message.
 * 2. Method delivery_id_t::revoke() is called by an user.
 *
 * For example:
 * \code
 * namespace delivery_ns = so_5::extra::revocable_msg;
 * void demo(so_5::mchain_t work_queue) {
 * 	// Send a demand to work queue and store the ID returned.
 * 	auto id = delivery_ns::send<do_something>(work_queue, ...);
 * 	... // Do some work.
 * 	if(some_condition)
 * 		// Our previous message should be revoked if it is not delivered yet.
 * 		id.revoke();
 * 	...
 * 	// Message will be automatically revoked here because ID is destroyed
 * 	// on leaving the scope.
 * }
 * \endcode
 *
 * \note
 * The delivery_id_t is Movable, not Copyable.
 *
 * \attention
 * This is not a thread-safe class. It means that it is dangerous to
 * call methods of that class (like revoke() or has_been_revoked()) from
 * different threads at the same time.
 *
 * \since
 * v.1.2.0
 */
class delivery_id_t final
	{
		friend struct so_5::extra::revocable_msg::impl::delivery_id_maker_t;

	private :
		//! The envelope that was sent.
		/*!
		 * \note Can be nullptr if default constructor was used.
		 */
		so_5::intrusive_ptr_t< details::envelope_t > m_envelope;

		delivery_id_t(
			so_5::intrusive_ptr_t< details::envelope_t > envelope )
			:	m_envelope{ std::move(envelope) }
			{}

	public :
		delivery_id_t() = default;
		/*!
		 * \note The destructor automatically revokes the message if it is
		 * not delivered yet.
		 */
		~delivery_id_t() noexcept
			{
				revoke();
			}

		// This class is not copyable.
		delivery_id_t( const delivery_id_t & ) = delete;
		delivery_id_t & operator=( const delivery_id_t & ) = delete;

		// But this class is moveable.
		delivery_id_t( delivery_id_t && ) noexcept = default;
		delivery_id_t & operator=( delivery_id_t && ) noexcept = default;

		friend void
		swap( delivery_id_t & a, delivery_id_t & b ) noexcept
			{
				a.m_envelope.swap( b.m_envelope );
			}

		//! Revoke the message.
		/*!
		 * \note
		 * It is safe to call revoke() for already revoked message.
		 */
		void
		revoke() noexcept
			{
				if( m_envelope )
					{
						m_envelope->revoke();
						m_envelope.reset();
					}
			}

		//! Has the message been revoked?
		/*!
		 * \note
		 * This method can return \a true for an empty instance of
		 * delivery_id_t. For example:
		 * \code
		 * namespace delivery_ns = so_5::extra::revokable_msg;
		 * delivery_id_t null_id;
		 * assert(null_id.has_been_revoked());
		 *
		 * auto id1 = delivery_ns::send<my_message>(mbox, ...);
		 * assert(!id1.has_been_revoked());
		 *
		 * delivery_id_t id2{ std::move(id1) }; // Move id1 to id2.
		 * assert(id1.has_been_revoked());
		 * assert(!id2.has_been_revoked());
		 * \endcode
		 */
		bool
		has_been_revoked() const noexcept
			{
				return m_envelope ? m_envelope->has_been_revoked() : true;
			}
	};

namespace impl {

/*
 * This is helper for creation of initialized delivery_id objects.
 */
struct delivery_id_maker_t
	{
		template< typename... Args >
		[[nodiscard]] static auto
		make( Args && ...args )
			{
				return delivery_id_t{ std::forward<Args>(args)... };
			}
	};

/*
 * Helper function for actual sending of revocable message.
 */
[[nodiscard]]
inline so_5::extra::revocable_msg::delivery_id_t
make_envelope_and_deliver(
	const so_5::mbox_t & to,
	const std::type_index & msg_type,
	message_ref_t payload )
	{
		using envelope_t = so_5::extra::revocable_msg::details::envelope_t;

		so_5::intrusive_ptr_t< envelope_t > envelope{
				std::make_unique< envelope_t >( std::move(payload) ) };

		to->do_deliver_message( msg_type, envelope, 1u );

		return delivery_id_maker_t::make( std::move(envelope) );
	}

/*
 * This is helpers for send() implementation.
 */

template< class Message, bool Is_Signal >
struct instantiator_and_sender_base
	{
		template< typename... Args >
		[[nodiscard]] static so_5::extra::revocable_msg::delivery_id_t
		send(
			const so_5::mbox_t & to,
			Args &&... args )
			{
				message_ref_t payload{
						so_5::details::make_message_instance< Message >(
								std::forward< Args >( args )...)
				};

				so_5::details::mark_as_mutable_if_necessary< Message >( *payload );

				return make_envelope_and_deliver(
						to,
						message_payload_type< Message >::subscription_type_index(),
						std::move(payload) );
			}
	};

template< class Message >
struct instantiator_and_sender_base< Message, true >
	{
		//! Type of signal to be delivered.
		using actual_signal_type = typename message_payload_type< Message >::subscription_type;

		[[nodiscard]] static so_5::extra::revocable_msg::delivery_id_t
		send(
			const so_5::mbox_t & to )
			{
				return make_envelope_and_deliver(
						to,
						message_payload_type< Message >::subscription_type_index(),
						message_ref_t{} );
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
 * \brief A utility function for creating and delivering a revocable message.
 *
 * This function can be used for sending messages and signals to
 * mboxes and mchains, and to the direct mboxes of agents and
 * ad-hoc agents.
 *
 * Message/signal sent can be revoked by using delivery_id_t::revoke()
 * method:
 * \code
 * auto id = so_5::extra::revocable_msg::send<my_message>(...);
 * ...
 * id.revoke();
 * \endcode
 * Please note that revoked message is not removed from queues where it
 * wait for processing. But revoked message/signal will be ignored just
 * after extraction from a queue.
 *
 * Usage examples:
 * \code
 * namespace delivery_ns = so_5::extra::revocable_msg;
 *
 * // Send a revocable message to mbox mb1.
 * so_5::mbox_t mb1 = ...;
 * auto id1 = delivery_ns::send<my_message>(mb1, ...);
 *
 * // Send a revocable message to mchain ch1 and revoke it after some time.
 * so_5::mchain_t ch1 = ...;
 * auto id2 = delivery_ns::send<my_message>(ch1, ...);
 * ...
 * id2.revoke();
 *
 * // Send a revocable message to the direct mbox of agent a1.
 * so_5::agent_t & a1 = ...;
 * auto id3 = delivery_ns::send<my_message>(a1, ...);
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the revocable message will be revoked automatically just right after
 * send() returns.
 *
 * \since
 * v.1.2.0
 *
 */
template< typename Message, typename Target, typename... Args >
[[nodiscard]] delivery_id_t
send(
	//! Target for the message.
	//! Can be a reference to mbox, mchain, agent or ad-hod agent.
	Target && to,
	//! Message constructor parameters.
	Args&&... args )
	{
		return impl::instantiator_and_sender< Message >::send(
				so_5::send_functions_details::arg_to_mbox(
						std::forward< Target >( to ) ),
				std::forward< Args >( args )... );
	}

/*!
 * \brief A helper function for redirection of an existing message
 * as a revocable one.
 *
 * Usage example:
 * \code
 * class my_agent : public so_5::agent_t {
 * 	...
 * 	so_5::extra::revocable_msg::delivery_id_t id_;
 * 	...
 * 	void on_some_event(mhood_t<my_message> cmd) {
 * 		... // Some processing.
 * 		// Redirection to another destination.
 * 		id_ = so_5::extra::revocable_msg::send(another_mbox_, cmd);
 * 	}
 * };
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the revocable message will be revoked automatically just right after
 * send() returns.
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename Target >
[[nodiscard]]
typename std::enable_if<
		!::so_5::is_signal< Message >::value,
		delivery_id_t >::type
send(
	//! Target for the message.
	//! Can be a reference to mbox, mchain, agent or ad-hod agent.
	Target && to,
	//! Message to be delivered.
	mhood_t<Message> cmd )
	{
		return impl::make_envelope_and_deliver(
				so_5::send_functions_details::arg_to_mbox(
						std::forward< Target >( to ) ),
				message_payload_type< Message >::subscription_type_index(),
				cmd.make_reference() );
	}

/*!
 * \brief A helper function for redirection of an existing signal
 * as a revocable one.
 *
 * This function can be useful in template classes where it is unknown
 * is template parameter message of signal.
 *
 * Usage example:
 * \code
 * template<typename Msg>
 * class my_agent : public so_5::agent_t {
 * 	...
 * 	so_5::extra::revocable_msg::delivery_id_t id_;
 * 	...
 * 	void on_some_event(mhood_t<Msg> cmd) {
 * 		... // Some processing.
 * 		// Redirection to another destination.
 * 		id_ = so_5::extra::revocable_msg::send(another_mbox_, cmd);
 * 	}
 * };
 * \endcode
 *
 * \note
 * The return value of that function must be stored somewhere. Otherwise
 * the revocable message will be revoked automatically just right after
 * send() returns.
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename Target >
[[nodiscard]]
typename std::enable_if<
		::so_5::is_signal< Message >::value,
		delivery_id_t >::type
send(
	//! Target for the message.
	//! Can be a reference to mbox, mchain, agent or ad-hod agent.
	Target && to,
	//! Signal to be delivered.
	mhood_t<Message> /*cmd*/ )
	{
		return impl::make_envelope_and_deliver(
				so_5::send_functions_details::arg_to_mbox(
						std::forward< Target >( to ) ),
				message_payload_type< Message >::subscription_type_index(),
				message_ref_t{} );
	}

} /* namespace revocable_msg */

} /* namespace extra */

} /* namespace so_5 */

