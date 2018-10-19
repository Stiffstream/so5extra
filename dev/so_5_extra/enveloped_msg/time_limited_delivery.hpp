/*!
 * \file
 * \brief An implementation of time_limited_delivery envelope.
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5_extra/enveloped_msg/just_envelope.hpp>

#include <chrono>

namespace so_5 {

namespace extra {

namespace enveloped_msg {

//
// time_limited_delivery_t
//
/*!
 * \brief A special envelope to perform time-limited delivery.
 *
 * This envelope checks the current time before processing/transformation
 * of the enveloped message. If the current time is equal or greater than
 * the specified deadline then message won't be processed/transformed.
 *
 * Usage example:
 * \code
 * namespace msg_ns = so_5::extra::enveloped_msg;
 *
 * // Use time_limited_delivery_t with wallclock deadline.
 * msg_ns::make<my_message>(...)
 * 		.envelope<msg_ns::time_limited_delivery_t>(
 * 				// Limit lifetime of the message by 5 seconds from now.
 * 				std::chrono::steady_clock::now() + 5s)
 * 		.send_to(destination);
 *
 * // The same thing but less verbose:
 * msg_ns::make<my_message>(...)
 * 		.envelope<msg_ns::time_limited_delivery_t>(
 * 				// Limit lifetime of the message by 5 seconds from now.
 * 				5s)
 * 		.send_to(destination);
 * \endcode
 *
 * \since
 * v.1.2.0
 */
class time_limited_delivery_t final : public just_envelope_t
	{
		//! Delivery deadline.
		const std::chrono::steady_clock::time_point m_deadline;

		void
		invoke_if_deadline_not_passed( handler_invoker_t & invoker ) const noexcept
			{
				if( m_deadline > std::chrono::steady_clock::now() )
					invoker.invoke( whole_payload() );
			}

	public :
		//! Initializing constructor.
		/*!
		 * Receives wallclock time as deadline.
		 */
		time_limited_delivery_t(
			//! Message to be delivered.
			message_ref_t message,
			//! Delivery deadline.
			std::chrono::steady_clock::time_point deadline )
			:	just_envelope_t{ std::move(message) }
			,	m_deadline{ deadline }
			{}

		//! Initializing constructor.
		/*!
		 * Receives time interval. Deadline will be calculated automatically
		 * from the current time.
		 */
		time_limited_delivery_t(
			//! Message to be delivered.
			message_ref_t message,
			//! Time interval for calculation of deadline value.
			std::chrono::steady_clock::duration deadline )
			:	time_limited_delivery_t{
					std::move(message),
					std::chrono::steady_clock::now() + deadline }
			{}

		// Implementation of inherited methods.
		void
		handler_found_hook(
			handler_invoker_t & invoker ) noexcept override
			{
				invoke_if_deadline_not_passed( invoker );
			}

		void
		transformation_hook(
			handler_invoker_t & invoker ) noexcept override
			{
				invoke_if_deadline_not_passed( invoker );
			}
	};

} /* namespace enveloped_msg */

} /* namespace extra */

} /* namespace so_5 */

