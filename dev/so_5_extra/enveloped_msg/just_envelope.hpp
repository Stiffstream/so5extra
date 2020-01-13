/*!
 * \file
 * \brief An implementation of just_envelope_t class.
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5_extra/enveloped_msg/errors.hpp>

#include <so_5/enveloped_msg.hpp>

namespace so_5 {

namespace extra {

namespace enveloped_msg {

//
// just_envelope_t
//
/*!
 * \brief A very simple implementation of envelope which do nothing except
 * holding a payload.
 *
 * This class can be used for:
 *
 * * testing purposes. When you need an eveloped message but do not to create
 *   own envelope class;
 * * as a base type for more complex envelopes.
 *
 * An example of usage of just_envelope_t as base class for your own
 * evelope class:
 * \code
 * class my_envelope : public so_5::extra::enveloped_msg::just_envelope_t
 * {
 * 	using base_type = so_5::extra::enveloped_msg::just_envelope_t;
 * public:
 * 	// Inherit constructor from base class.
 * 	using base_type::base_type;
 *
 * 	// Override access_hook and do some action after
 * 	// processing of payload.
 * 	void
 * 	access_hook(
 * 		access_context_t context,
 * 		handler_invoker_t & invoker ) noexcept override
 * 	{
 * 		// Delegate payload extraction to the base type.
 * 		base_type::access_hook( context, invoker );
 *
 * 		// Do our own logic.
 * 		do_some_action();
 * 	}
 * };
 * \endcode
 *
 * \attention
 * This type of envelope inherits mutability from the payload. If the payload
 * is mutable then the envelope is also mutable. If the payload immutable
 * the the envelope is immutable too.
 * Mutability of an envelope can't changed.
 * Method so5_change_mutability() will throw on attempt to
 * set different mutibility value.
 *
 * \note
 * This class is not Copyable nor Moveable.
 *
 * \since
 * v.1.2.0
 */
class just_envelope_t : public so_5::enveloped_msg::envelope_t
	{
		//! Actual payload.
		/*!
		 * This attribute is mutable because there can be a need to
		 * have non-const reference to message_ref in const-methods.
		 *
		 * \note
		 * It can be nullptr if payload is a signal.
		 */
		mutable message_ref_t m_payload;

	protected :
		//! Get access to content of envelope.
		[[nodiscard]]
		payload_info_t
		whole_payload() const noexcept
			{
				return { m_payload };
			}

		//! Get access to payload only.
		[[nodiscard]]
		message_ref_t &
		payload() const noexcept { return m_payload; }

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
			message_mutability_t new_value ) override
			{
				if( new_value != so5_message_mutability() )
					SO_5_THROW_EXCEPTION(
							so_5::extra::enveloped_msg::errors
									::rc_mutabilty_of_envelope_cannot_be_changed,
							"just_envelope_t prohibit changing of message mutability" );
			}

	public :
		//! Initializing constructor.
		just_envelope_t(
			so_5::message_ref_t payload )
			:	m_payload{ std::move(payload) }
			{}
		~just_envelope_t() noexcept override = default;

		// Disable Copy and Move for that class.
		just_envelope_t( const just_envelope_t & ) = delete;
		just_envelope_t( just_envelope_t && ) = delete;

		// Implementation of inherited methods.
		void
		access_hook(
			access_context_t /*context*/,
			handler_invoker_t & invoker ) noexcept override
			{
				invoker.invoke( whole_payload() );
			}
	};

} /* namespace enveloped_msg */

} /* namespace extra */

} /* namespace so_5 */

