/*!
 * \file
 * \brief Implementation of proxy mbox with inflight limit.
 *
 * \since v.1.5.2
 */

#pragma once

#include <so_5_extra/mboxes/proxy.hpp>

#include <so_5_extra/error_ranges.hpp>

#include <so_5_extra/enveloped_msg/just_envelope.hpp>

#include <so_5/impl/msg_tracing_helpers.hpp>

#include <so_5/environment.hpp>

#include <atomic>

namespace so_5 {

namespace extra::mboxes::inflight_limit {

//FIXME: document this!
using underlying_counter_t = unsigned int;

} /* namespace extra::mboxes::inflight_limit */

namespace impl::msg_tracing_helpers::details {

// Special extension for inflight_limit specific data.
namespace extra_inflight_limit_specifics {

struct limit_info
	{
		so_5::extra::mboxes::inflight_limit::underlying_counter_t m_limit;
		so_5::extra::mboxes::inflight_limit::underlying_counter_t m_current_number;
	};

} /* namespace extra_inflight_limit_specifics */

inline void
make_trace_to_1( std::ostream & s, extra_inflight_limit_specifics::limit_info info )
	{
		s << "[inflight_limit=" << info.m_limit << ",inflight_current="
				<< info.m_current_number << "]";
	}

inline void
fill_trace_data_1(
	actual_trace_data_t & /*d*/,
	extra_inflight_limit_specifics::limit_info /*info*/ )
	{
		// Nothing to do.
	}

} /* namespace impl::msg_tracing_helpers::details */

namespace extra {

namespace mboxes {

namespace inflight_limit {

namespace errors {

/*!
 * \brief An attempt to use a message type that differs from mbox's message
 * type.
 *
 * Type of message to be used with inflight_limit_mbox
 * is fixed as part of inflight_limit_mbox type.
 * An attempt to use different message type (for subscription, delivery or
 * setting delivery filter) will lead to an exception with this error code.
 *
 * \since v.1.5.2
 */
const int rc_different_message_type =
		so_5::extra::errors::mboxes_inflight_limit_errors;

/*!
 * \brief Null pointer to underlying mbox.
 *
 * A inflight_limit_mbox uses underlying mbox and delegates all actions to that mbox.
 * Because of that underlying mbox can't be nullptr.
 *
 * \since v.1.5.2
 */
const int rc_nullptr_as_underlying_mbox =
		::so_5::extra::errors::mboxes_inflight_limit_errors + 1;

} /* namespace impl */

namespace impl {

//FIXME: document this!
struct instances_counter_t
	{
		//! Counter of inflight instances.
		std::atomic< underlying_counter_t > m_instances{};
	};

//FIXME: document this!
using instances_counter_shptr_t = std::shared_ptr< instances_counter_t >;

//FIXME: document this!
class counter_incrementer_t
	{
		instances_counter_t & m_counter;
		const underlying_counter_t m_value;

		bool m_should_decrement_in_destructor{ true };

	public:
		counter_incrementer_t(
			outliving_reference_t< instances_counter_t > counter )
			:	m_counter{ counter.get() }
			,	m_value{ ++(m_counter.m_instances) }
			{}

		~counter_incrementer_t() noexcept
			{
				if( m_should_decrement_in_destructor )
					(m_counter.m_instances)--;
			}

		void
		do_not_decrement_in_destructor() noexcept
			{
				m_should_decrement_in_destructor = false;
			}

		[[nodiscard]]
		underlying_counter_t
		value() const noexcept
			{
				return m_value;
			}
	};

//FIXME: document this!
class special_envelope_t final : public so_5::extra::enveloped_msg::just_envelope_t
	{
		using base_type_t = so_5::extra::enveloped_msg::just_envelope_t;

		instances_counter_shptr_t m_counter;

	public:
		//! Initializing constructor.
		special_envelope_t(
			message_ref_t payload,
			instances_counter_shptr_t counter )
			:	base_type_t{ std::move(payload) }
			,	m_counter{ std::move(counter) }
			{}

		~special_envelope_t() noexcept override
			{
				// Counter should always be decremented because it was
				// incremented before the creation of envelope instance.
				(m_counter->m_instances)--;
			}
	};

//FIXME: document this!
/*!
 * \tparam Tracing_Base base class with implementation of message
 * delivery tracing methods.
 *
 * \since v.1.5.2
 */
template< typename Tracing_Base >
class actual_mbox_t final
	: public so_5::abstract_message_box_t
	, private Tracing_Base
	{
		//! Actual underlying mbox to be used for all calls.
		/*!
		 * \attention Should not be nullptr.
		 */
		const so_5::mbox_t m_underlying_mbox;

		//! Type of a message for that mbox is created.
		const std::type_index m_msg_type;

		//! The limit of inflight messages.
		const underlying_counter_t m_limit;

		//! Counter for inflight instances.
		instances_counter_shptr_t m_instances_counter;

		//! Ensure that underlying mbox is not nullptr.
		/*!
		 * \throw so_5::exception_t if \a mbox is nullptr.
		 */
		[[nodiscard]]
		so_5::mbox_t
		ensure_underlying_mbox_not_null(
			so_5::mbox_t mbox )
			{
				if( !mbox )
					SO_5_THROW_EXCEPTION(
							errors::rc_nullptr_as_underlying_mbox,
							"nullptr is used as underlying mbox" );

				return mbox;
			}

	public:
		/*!
		 * \brief Initializing constructor.
		 *
		 * \tparam Tracing_Args parameters for Tracing_Base constructor
		 * (can be empty list if Tracing_Base have only the default constructor).
		 */
		template< typename... Tracing_Args >
		actual_mbox_t(
			//! Destination mbox.
			so_5::mbox_t dest_mbox,
			//! Type of a message for that mbox.
			std::type_index msg_type,
			//! The limit of inflight messages.
			underlying_counter_t limit,
			//! Optional parameters for Tracing_Base's constructor.
			Tracing_Args &&... args )
			:	Tracing_Base{ std::forward< Tracing_Args >(args)... }
			,	m_underlying_mbox{
					ensure_underlying_mbox_not_null( std::move(dest_mbox) ) }
			,	m_msg_type{ std::move(msg_type) }
			,	m_limit{ limit }
			,	m_instances_counter{ std::make_shared< instances_counter_t >() }
			{}

		mbox_id_t
		id() const override
			{
				return m_underlying_mbox->id();
			}

		void
		subscribe_event_handler(
			const std::type_index & msg_type,
			const ::so_5::message_limit::control_block_t * limit,
			::so_5::agent_t & subscriber ) override
			{
				ensure_expected_msg_type(
						msg_type,
						//FIXME: is such message appropriate?
						"subscribe_event_handler" );

				m_underlying_mbox->subscribe_event_handler(
						msg_type, limit, subscriber );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & msg_type,
			::so_5::agent_t & subscriber ) override
			{
				ensure_expected_msg_type(
						msg_type,
						//FIXME: is such message appropriate?
						"unsubscribe_event_handlers" );

				m_underlying_mbox->unsubscribe_event_handlers(
						msg_type, subscriber );
			}

		std::string
		query_name() const override
			{
				return m_underlying_mbox->query_name();
			}

		mbox_type_t
		type() const override
			{
				return m_underlying_mbox->type();
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const ::so_5::message_ref_t & message,
			unsigned int overlimit_reaction_deep ) override
			{
				ensure_expected_msg_type(
						msg_type,
						//FIXME: is such message appropriate?
						"do_deliver_message" );

				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_base
						*this, // as abstract_message_box_t
						"deliver_message",
						msg_type, message, overlimit_reaction_deep };

				// Step 1: increment the counter and check that the limit
				// isn't exeeded yet.
				counter_incrementer_t incrementer{ 
						outliving_mutable( *m_instances_counter )
					};
				if( incrementer.value() <= m_limit )
					{
						// NOTE: if there will be an exception then the number
						// of instance will be decremented by incrementer.
						message_ref_t our_envelope{
								std::make_unique< special_envelope_t >(
										std::move(message),
										m_instances_counter )
							};

						// incrementer shouldn't control the number of instances
						// anymore.
						incrementer.do_not_decrement_in_destructor();

						// Our envelope object has to be sent.
						m_underlying_mbox->do_deliver_message(
								msg_type,
								our_envelope,
								overlimit_reaction_deep );
					}
				else
					{
						using namespace so_5::impl::msg_tracing_helpers::details::
								extra_inflight_limit_specifics;

						tracer.make_trace(
								"too_many_inflight_messages",
								limit_info{ m_limit, incrementer.value() } );
					}
			}

		void
		set_delivery_filter(
			const std::type_index & msg_type,
			const ::so_5::delivery_filter_t & filter,
			::so_5::agent_t & subscriber ) override
			{
				ensure_expected_msg_type(
						msg_type,
						//FIXME: is such message appropriate?
						"set_delivery_filter" );

				m_underlying_mbox->set_delivery_filter(
						msg_type,
						filter,
						subscriber );
			}

		void
		drop_delivery_filter(
			const std::type_index & msg_type,
			::so_5::agent_t & subscriber ) noexcept override
			{
				ensure_expected_msg_type(
						msg_type,
						//FIXME: is such message appropriate?
						"drop_delivery_filter" );

				m_underlying_mbox->drop_delivery_filter(
						msg_type,
						subscriber );
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return m_underlying_mbox->environment();
			}

	private:
		/*!
		 * Throws an error if msg_type differs from expected message type.
		 */
		void
		ensure_expected_msg_type(
			const std::type_index & msg_type,
			std::string_view error_description ) const
			{
				if( msg_type != m_msg_type )
					SO_5_THROW_EXCEPTION(
							errors::rc_different_message_type,
							//FIXME: we have to create std::string object because
							//so_5::exception_t::raise expects std::string.
							//This should be fixed after resolving:
							//https://github.com/Stiffstream/sobjectizer/issues/46
							std::string{ error_description } );
			}
	};

} /* namespace impl */

//FIXME: document this!
template<
	typename Msg_Type >
[[nodiscard]]
mbox_t
make_mbox(
	//! Actual destination mbox.
	mbox_t dest_mbox,
	//! The limit of inflight messages.
	underlying_counter_t inflight_limit )
	{
		//FIXME: should we check dest_mbox for nullptr?
		auto & env = dest_mbox->environment();
		return env.make_custom_mbox(
				[&dest_mbox, inflight_limit]( const mbox_creation_data_t & data )
				{
					mbox_t result;

					if( data.m_tracer.get().is_msg_tracing_enabled() )
						{
							using T = impl::actual_mbox_t<
									::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;

							result = mbox_t{ new T{
									std::move(dest_mbox),
									message_payload_type< Msg_Type >::subscription_type_index(),
									inflight_limit,
									data.m_tracer.get()
								} };
						}
					else
						{
							using T = impl::actual_mbox_t<
									::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;

							result = mbox_t{ new T{
									std::move(dest_mbox),
									message_payload_type< Msg_Type >::subscription_type_index(),
									inflight_limit
								} };
						}

					return result;
				} );
	}

} /* namespace inflight_limit */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

