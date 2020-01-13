/*!
 * \file
 * \brief Variuos send functions for simplification of sending
 * enveloped messages.
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5_extra/enveloped_msg/errors.hpp>

#include <so_5/send_functions.hpp>

#include <so_5/optional.hpp>

namespace so_5 {

namespace extra {

namespace enveloped_msg {

namespace details {

/*!
 * \brief Internal type that holds a message before it will be enveloped.
 *
 * This type provides such methods as:
 * - envelope() for creation of new envelope;
 * - send_to for sending of ordinary message to the specified mbox (mchain);
 * - send_delayed_to for sending of delayed message;
 * - send_periodic_to for sending of periodic message.
 *
 * \note
 * Object of type payload_holder_t can be in two states: full and empty.
 * When object is full then envelope() creates an envelope and switches
 * the object to empty state. If envelope() is called in empty state
 * an exeception is thrown.
 *
 * \since
 * v.1.2.0
 */
class payload_holder_t final
	{
		struct data_t final
			{
				std::type_index m_msg_type;
				// Can be null pointer in the case of error.
				message_ref_t m_message;
			};

		optional< data_t > m_data;

		void
		ensure_not_empty_object(
			const char * context_name ) const
			{
				if( !m_data )
					SO_5_THROW_EXCEPTION(
							errors::rc_empty_payload_holder,
							std::string( "empty payload_holder can't be used for: " ) +
									context_name );
			}

	public :
		payload_holder_t(
			std::type_index msg_type,
			message_ref_t message )
			:	m_data{ data_t{ msg_type, message } }
			{}

		payload_holder_t( const payload_holder_t & ) = delete;

		payload_holder_t( payload_holder_t && ) = default;
		payload_holder_t & operator=( payload_holder_t && ) = default;

		template< typename Envelope, typename... Args >
		[[nodiscard]]
		payload_holder_t
		envelope( Args && ...args )
			{
				ensure_not_empty_object( "envelope()" );

				message_ref_t envelope{
					std::make_unique< Envelope >(
							m_data->m_message,
							std::forward<Args>(args)... )
				};

				payload_holder_t result{ 
						m_data->m_msg_type,
						std::move(envelope)
				};

				// Now data can be dropped. Payload holder becomes empty.
				m_data.reset();

				return result;
			}

		template< typename Target >
		void
		send_to( Target && to )
			{
				ensure_not_empty_object( "send_to()" );

				// NOTE: there is no need to check mutability of a message.
				// This check should be performed by the target mbox itself.
				so_5::send_functions_details::arg_to_mbox( to )->
						do_deliver_message(
								m_data->m_msg_type,
								m_data->m_message,
								1u );
			}

		void
		send_delayed_to(
			const so_5::mbox_t & to,
			std::chrono::steady_clock::duration pause )
			{
				ensure_not_empty_object( "send_delayed_to()" );

				so_5::low_level_api::single_timer(
						m_data->m_msg_type,
						m_data->m_message,
						to,
						pause );
			}

		template< typename Target >
		void
		send_delayed_to(
			Target && to,
			std::chrono::steady_clock::duration pause )
			{
				return this->send_delayed_to(
						so_5::send_functions_details::arg_to_mbox( to ),
						pause );
			}

		[[nodiscard]]
		auto
		send_periodic_to(
			const so_5::mbox_t & to,
			std::chrono::steady_clock::duration pause,
			std::chrono::steady_clock::duration period )
			{
				ensure_not_empty_object( "send_periodic_to()" );

				return so_5::low_level_api::schedule_timer(
						m_data->m_msg_type,
						m_data->m_message,
						to,
						pause,
						period );
			}

		template< typename Target >
		[[nodiscard]]
		auto
		send_periodic_to(
			Target && to,
			std::chrono::steady_clock::duration pause,
			std::chrono::steady_clock::duration period )
			{
				return this->send_periodic_to(
						so_5::send_functions_details::arg_to_mbox( to ),
						pause,
						period );
			}
	};

} /* namespace details */

/*!
 * \brief A special message builder that allows to wrap a message into an
 * envelope.
 *
 * This function creates an instance of a specified message type and creates
 * a chain of builders that envelope this instance into an envelope and send
 * the enveloped message as ordinary or delayed/periodic message.
 *
 * Usage examples:
 * \code
 * namespace msg_ns = so_5::extra::enveloped_msg;
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the mbox mb1.
 * so_5::mbox_t mb1 = ...;
 * msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_to(mb1);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the mchain ch1.
 * so_5::mchain_t ch1 = ...;
 * msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_to(ch1);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the direct mbox of the agent a1.
 * so_5::agent_t & a1 = ...;
 * msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_to(a1);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the mbox mb1 as delayed message.
 * so_5::mbox_t mb1 = ...;
 * msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_delayed_to(mb1, 10s);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the mchain ch1 as delayed message.
 * so_5::mchain_t ch1 = ...;
 * msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_delayed_to(ch1, 10s);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the direct mbox of the agent a1 as delayed message.
 * so_5::agent_t & a1 = ...;
 * msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_delayed_to(a1, 10s);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the mbox mb1 as periodic message.
 * so_5::mbox_t mb1 = ...;
 * auto timer_id = msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_periodic_to(mb1, 10s, 30s);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the mchain ch1 as delayed message.
 * so_5::mchain_t ch1 = ...;
 * auto timer_id = msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_periodic_to(ch1, 10s, 30s);
 *
 * // Create message of type my_message, envelop it into my_envelope
 * // and then send it to the direct mbox of the agent a1 as delayed message.
 * so_5::agent_t & a1 = ...;
 * auto timer_id = msg_ns::make<my_message>(...)
 * 		.envelope<my_envelope>(...)
 * 		.send_periodic_to(a1, 10s, 30s);
 * \endcode
 *
 * \since
 * v.1.2.0
 */
template< typename Message, typename... Args >
[[nodiscard]]
details::payload_holder_t
make( Args && ...args )
	{
		message_ref_t message{
				so_5::details::make_message_instance< Message >(
						std::forward<Args>(args)... )
		};

		so_5::details::mark_as_mutable_if_necessary< Message >( *message );

		return {
				message_payload_type< Message >::subscription_type_index(),
				std::move(message)
		};
	}

} /* namespace enveloped_msg */

} /* namespace extra */

} /* namespace so_5 */

