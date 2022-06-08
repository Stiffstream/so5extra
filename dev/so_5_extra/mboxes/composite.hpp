/*!
 * \file
 * \brief Implementation of composite mbox.
 *
 * \since v.1.5.2
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/impl/msg_tracing_helpers.hpp>

#include <so_5/environment.hpp>
#include <so_5/mbox.hpp>

#include <algorithm>
#include <map>
#include <variant>
#include <vector>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace composite {

namespace errors {

/*!
 * \brief An attempt to send message of a type for that there is no a sink.
 *
 * \since v.1.5.2
 */
const int rc_no_sink_for_message_type =
		so_5::extra::errors::mboxes_composite_errors;

/*!
 * \brief An attempt to add another sink for a message type.
 *
 * Just one destination mbox can be specified for a message type.
 * An attempt to add another destination mbox will lead to this error code.
 *
 * \since v.1.5.2
 */
const int rc_message_type_already_has_sink =
		so_5::extra::errors::mboxes_composite_errors + 1;

/*!
 * \brief An attempt to add MPMC sink to MPSC mbox.
 *
 * If composite mbox is created as MPSC mbox then a MPMC mbox can't be
 * added as a destination.
 *
 * \since v.1.5.2
 */
const int rc_mpmc_sink_can_be_used_with_mpsc_composite =
		so_5::extra::errors::mboxes_composite_errors + 2;

/*!
 * \brief An attempt to use nullptr as the default destination mbox.
 *
 * An attempt use null pointer to mbox as the default destination mbox.
 * For example, an empty mbox_t instance is passed to
 * redirect_to_if_not_found() function.
 *
 * \since v.1.5.2
 */
const int rc_null_as_default_destination_mbox =
		so_5::extra::errors::mboxes_composite_errors + 3;

} /* namespace errors */

/*!
 * \brief Description of a case when messages of unknown type have to be
 * redirected to another mbox.
 *
 * \since v.1.5.2
 */
class redirect_to_if_not_found_case_t
	{
		//! Destination for message of unknown type.
		mbox_t m_dest;

		[[nodiscard]]
		static mbox_t
		ensure_not_null( mbox_t dest )
			{
				if( !dest )
					SO_5_THROW_EXCEPTION(
							errors::rc_null_as_default_destination_mbox,
							"nullptr can't be used as the default destination mbox" );

				return dest;
			}

	public:
		//! Initializing constructor.
		redirect_to_if_not_found_case_t( mbox_t dest )
			:	m_dest{ ensure_not_null( std::move(dest) ) }
			{}

		//! Getter.
		[[nodiscard]]
		const mbox_t &
		dest() const noexcept { return m_dest; }
	};

/*!
 * \brief Description of a case when an exception has to be thrown if
 * the type of a message is unknown.
 *
 * An exception will also be thrown on attempts to subscribe to and/or set
 * delivery filter for unknown message type.
 *
 * \since v.1.5.2
 */
struct throw_if_not_found_case_t
	{};

/*!
 * \brief Description of a case when a message of unknown type has to be dropped.
 *
 * Attempts to make subscriptions and/or set delivery filters for unknown
 * message type will be silently ignored.
 *
 * \since v.1.5.2
 */
struct drop_if_not_found_case_t
	{};

/*!
 * \brief Type that describes the reaction to a message of unknown type.
 *
 * \since v.1.5.2
 */
using type_not_found_reaction_t = std::variant<
		redirect_to_if_not_found_case_t,
		throw_if_not_found_case_t,
		drop_if_not_found_case_t
	>;

/*!
 * \brief Helper function to set a reaction to unknown message type.
 *
 * Message of unknown type has to be redirected to specified mbox.
 * Subscriptions and delivery filters for unknown type have also be
 * handled by \a dest_mbox.
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto mbox = multi_consumer_builder(redirect_to_if_not_found(default_mbox))
 * 	.add<first_message>(first_mbox)
 * 	.add<second_message>(second_mbox)
 * 	.make(env);
 * \endcode
 *
 * \since v.1.5.2
 */
[[nodiscard]]
inline type_not_found_reaction_t
redirect_to_if_not_found( const mbox_t & dest_mbox )
	{
		return { redirect_to_if_not_found_case_t{ dest_mbox } };
	}

/*!
 * \brief Helper function to set a reaction to unknown message type.
 *
 * Attempt to use unknown message type (e.g. sending of a message,
 * subscription or settting delivery filter) should lead to raising
 * an exception (an instance of so_5::exception_t will be thrown).
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto mbox = multi_consumer_builder(throw_if_not_found(default_mbox))
 * 	.add<first_message>(first_mbox)
 * 	.add<second_message>(second_mbox)
 * 	.make(env);
 * \endcode
 *
 * \since v.1.5.2
 */
[[nodiscard]]
inline type_not_found_reaction_t
throw_if_not_found()
	{
		return { throw_if_not_found_case_t{} };
	}

/*!
 * \brief Helper function to set a reaction to unknown message type.
 *
 * Attempt to use unknown message type (e.g. sending of a message,
 * subscription or settting delivery filter) should be silently ignored.
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto mbox = multi_consumer_builder(drop_if_not_found(default_mbox))
 * 	.add<first_message>(first_mbox)
 * 	.add<second_message>(second_mbox)
 * 	.make(env);
 * \endcode
 *
 * \since v.1.5.2
 */
[[nodiscard]]
inline type_not_found_reaction_t
drop_if_not_found()
	{
		return { drop_if_not_found_case_t{} };
	}

// Forward declaration.
class mbox_builder_t;

namespace impl {

/*!
 * \brief Description of one sink.
 *
 * Contains info about message type and destination mbox for messages
 * of that type.
 *
 * \since v.1.5.2
 */
struct sink_t
	{
		//! Type for that the destination has to be used.
		std::type_index m_msg_type;
		//! The destination for messages for that type.
		mbox_t m_dest;

		//! Initializing constructor.
		sink_t( std::type_index msg_type, mbox_t dest )
			:	m_msg_type{ std::move(msg_type) }
			,	m_dest{ std::move(dest) }
			{}
	};

/*!
 * \brief Type of container for holding sinks.
 *
 * \since v.1.5.2
 */
using sink_container_t = std::vector< sink_t >;

/*!
 * \brief Comparator function object to be used with std::lower_bound.
 *
 * \since v.1.5.2
 */
inline const auto sink_compare =
		[]( const sink_t & sink, const std::type_index & msg_type ) -> bool {
			return sink.m_msg_type < msg_type;
		};

namespace unknown_msg_type_handlers
{

/*!
 * \brief Function object to be used with std::visit.
 *
 * Implement logic of so_5::abstract_message_box_t::subscribe_event_handler()
 * in a case when message type is unknown.
 */
class subscribe_event_t
	{
		const std::type_index & m_msg_type;
		const so_5::message_limit::control_block_t * m_limit;
		agent_t & m_subscriber;

	public:
		subscribe_event_t(
			const std::type_index & msg_type,
			const so_5::message_limit::control_block_t * limit,
			agent_t & subscriber )
			:	m_msg_type{ msg_type }
			,	m_limit{ limit }
			,	m_subscriber{ subscriber }
			{}

		void
		operator()( const redirect_to_if_not_found_case_t & c ) const
			{
				c.dest()->subscribe_event_handler(
						m_msg_type,
						m_limit,
						m_subscriber );
			}

		void
		operator()( const throw_if_not_found_case_t & ) const
			{
				SO_5_THROW_EXCEPTION(
						errors::rc_no_sink_for_message_type,
						"no destination for this message type, "
						"msg_type=" + std::string(m_msg_type.name()) );
			}

		void
		operator()( const drop_if_not_found_case_t & ) const
			{
				// Nothing to do.
			}
	};

/*!
 * \brief Function object to be used with std::visit.
 *
 * Implement logic of so_5::abstract_message_box_t::unsubscribe_event_handlers()
 * in a case when message type is unknown.
 */
class unsubscribe_event_t
	{
		const std::type_index & m_msg_type;
		agent_t & m_subscriber;

	public:
		unsubscribe_event_t(
			const std::type_index & msg_type,
			agent_t & subscriber )
			:	m_msg_type{ msg_type }
			,	m_subscriber{ subscriber }
			{}

		void
		operator()( const redirect_to_if_not_found_case_t & c ) const
			{
				c.dest()->unsubscribe_event_handlers(
						m_msg_type,
						m_subscriber );
			}

		void
		operator()( const throw_if_not_found_case_t & ) const
			{
				// Just ignore that case.
			}

		void
		operator()( const drop_if_not_found_case_t & ) const
			{
				// Nothing to do.
			}
	};

/*!
 * \brief Function object to be used with std::visit.
 *
 * Implement logic of so_5::abstract_message_box_t::do_deliver_message()
 * in a case when message type is unknown.
 */
template< typename Tracer >
class deliver_message_t
	{
		Tracer & m_tracer;
		const std::type_index & m_msg_type;
		const message_ref_t & m_msg;
		unsigned int m_overlimit_deep;

	public:
		deliver_message_t(
			Tracer & tracer,
			const std::type_index & msg_type,
			const message_ref_t & msg,
			unsigned int overlimit_deep )
			:	m_tracer{ tracer }
			,	m_msg_type{ msg_type }
			,	m_msg{ msg }
			,	m_overlimit_deep{ overlimit_deep }
			{}

		void
		operator()( const redirect_to_if_not_found_case_t & c ) const
			{
				using namespace ::so_5::impl::msg_tracing_helpers::details;

				m_tracer.make_trace(
						"redirect_to_default_destination",
						mbox_as_msg_destination{ *(c.dest()) } );

				c.dest()->do_deliver_message(
						m_msg_type,
						m_msg,
						m_overlimit_deep );
			}

		void
		operator()( const throw_if_not_found_case_t & ) const
			{
				m_tracer.make_trace(
						"no_destination.throw_exception" );
				SO_5_THROW_EXCEPTION(
						errors::rc_no_sink_for_message_type,
						"no destination for this message type, "
						"msg_type=" + std::string(m_msg_type.name()) );
			}

		void
		operator()( const drop_if_not_found_case_t & ) const
			{
				m_tracer.make_trace(
						"no_destination.drop_message" );
			}
	};

/*!
 * \brief Function object to be used with std::visit.
 *
 * Implement logic of so_5::abstract_message_box_t::set_delivery_filter()
 * in a case when message type is unknown.
 */
class set_delivery_filter_t
	{
		const std::type_index & m_msg_type;
		const delivery_filter_t & m_filter;
		agent_t & m_subscriber;

	public:
		set_delivery_filter_t(
			const std::type_index & msg_type,
			const delivery_filter_t & filter,
			agent_t & subscriber )
			:	m_msg_type{ msg_type }
			,	m_filter{ filter }
			,	m_subscriber{ subscriber }
			{}

		void
		operator()( const redirect_to_if_not_found_case_t & c ) const
			{
				c.dest()->set_delivery_filter(
						m_msg_type,
						m_filter,
						m_subscriber );
			}

		void
		operator()( const throw_if_not_found_case_t & ) const
			{
				SO_5_THROW_EXCEPTION(
						errors::rc_no_sink_for_message_type,
						"no destination for this message type, "
						"msg_type=" + std::string(m_msg_type.name()) );
			}

		void
		operator()( const drop_if_not_found_case_t & ) const
			{
				// Nothing to do.
			}
	};

/*!
 * \brief Function object to be used with std::visit.
 *
 * Implement logic of so_5::abstract_message_box_t::drop_delivery_filter()
 * in a case when message type is unknown.
 */
class drop_delivery_filter_t
	{
		const std::type_index & m_msg_type;
		agent_t & m_subscriber;

	public:
		drop_delivery_filter_t(
			const std::type_index & msg_type,
			agent_t & subscriber ) noexcept
			:	m_msg_type{ msg_type }
			,	m_subscriber{ subscriber }
			{}

		void
		operator()( const redirect_to_if_not_found_case_t & c ) const noexcept
			{
				c.dest()->drop_delivery_filter(
						m_msg_type,
						m_subscriber );
			}

		void
		operator()( const throw_if_not_found_case_t & ) const noexcept
			{
				// Just ignore that case.
			}

		void
		operator()( const drop_if_not_found_case_t & ) const noexcept
			{
				// Nothing to do.
			}
	};

} /* namespace unknown_msg_type_handlers */

/*!
 * \brief Mbox data that doesn't depend on template parameters.
 *
 * \since v.1.5.2
 */
struct mbox_data_t
	{
		//! SObjectizer Environment to work in.
		environment_t * m_env_ptr;

		//! ID of this mbox.
		mbox_id_t m_id;

		//! Type of the mbox.
		mbox_type_t m_mbox_type;

		//! What to do with messages of unknown type.
		type_not_found_reaction_t m_unknown_type_reaction;

		//! Registered sinks.
		sink_container_t m_sinks;

		mbox_data_t(
			environment_t & env,
			mbox_id_t id,
			mbox_type_t mbox_type,
			type_not_found_reaction_t unknown_type_reaction,
			sink_container_t sinks )
			:	m_env_ptr{ &env }
			,	m_id{ id }
			,	m_mbox_type{ mbox_type }
			,	m_unknown_type_reaction{ std::move(unknown_type_reaction) }
			,	m_sinks{ std::move(sinks) }
			{}
	};

/*!
 * \brief Actual implementation of composite mbox.
 *
 * \note
 * An instance of that class is immutable. It doesn't allow modification of its
 * state. It makes the internals of actual_mbox_t thread safe.
 *
 * \since v.1.5.2
 */
template< typename Tracing_Base >
class actual_mbox_t final
	:	public abstract_message_box_t
	,	private Tracing_Base
	{
		friend class ::so_5::extra::mboxes::composite::mbox_builder_t;

		/*!
		 * \brief Initializing constructor.
		 *
		 * \tparam Tracing_Args parameters for Tracing_Base constructor
		 * (can be empty list if Tracing_Base have only the default constructor).
		 */
		template< typename... Tracing_Args >
		actual_mbox_t(
			//! Data for mbox that doesn't depend on template parameters.
			mbox_data_t mbox_data,
			Tracing_Args &&... tracing_args )
			:	Tracing_Base{ std::forward<Tracing_Args>(tracing_args)... }
			,	m_data{ std::move(mbox_data) }
			{}

	public:
		~actual_mbox_t() override = default;

		mbox_id_t
		id() const override
			{
				return this->m_data.m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & msg_type,
			const so_5::message_limit::control_block_t * limit,
			agent_t & subscriber ) override
			{
				const auto opt_sink = try_find_sink_for_msg_type( msg_type );
				if( opt_sink )
					{
						(*opt_sink)->m_dest->subscribe_event_handler(
								msg_type,
								limit,
								subscriber );
					}
				else
					{
						std::visit(
								unknown_msg_type_handlers::subscribe_event_t{
										msg_type,
										limit,
										subscriber },
								this->m_data.m_unknown_type_reaction );
					}
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & msg_type,
			agent_t & subscriber ) override
			{
				const auto opt_sink = try_find_sink_for_msg_type( msg_type );
				if( opt_sink )
					{
						(*opt_sink)->m_dest->unsubscribe_event_handlers(
								msg_type,
								subscriber );
					}
				else
					{
						std::visit(
								unknown_msg_type_handlers::unsubscribe_event_t{
										msg_type,
										subscriber },
								this->m_data.m_unknown_type_reaction );
					}
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=COMPOSITE";

				switch( this->m_data.m_mbox_type )
					{
					case mbox_type_t::multi_producer_multi_consumer:
						s << "(MPMC)";
					break;

					case mbox_type_t::multi_producer_single_consumer:
						s << "(MPSC)";
					break;
					}

				s << ":id=" << this->m_data.m_id << ">";

				return s.str();
			}

		mbox_type_t
		type() const override
			{
				return this->m_data.m_mbox_type;
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep ) override
			{
				ensure_immutable_message( msg_type, message );

				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_base
						*this, // as abstract_message_box_t
						"deliver_message",
						msg_type, message, overlimit_reaction_deep };

				const auto opt_sink = try_find_sink_for_msg_type( msg_type );
				if( opt_sink )
					{
						using namespace ::so_5::impl::msg_tracing_helpers::details;

						tracer.make_trace(
								"redirect_to_destination",
								mbox_as_msg_destination{ *( (*opt_sink)->m_dest ) } );

						(*opt_sink)->m_dest->do_deliver_message(
								msg_type,
								message,
								overlimit_reaction_deep );
					}
				else
					{
						using handler_t = unknown_msg_type_handlers::deliver_message_t<
								typename Tracing_Base::deliver_op_tracer >;

						std::visit(
								handler_t{
										tracer,
										msg_type,
										message,
										overlimit_reaction_deep },
								this->m_data.m_unknown_type_reaction );
					}
			}

		void
		set_delivery_filter(
			const std::type_index & msg_type,
			const delivery_filter_t & filter,
			agent_t & subscriber ) override
			{
				const auto opt_sink = try_find_sink_for_msg_type( msg_type );
				if( opt_sink )
					{
						(*opt_sink)->m_dest->set_delivery_filter(
								msg_type,
								filter,
								subscriber );
					}
				else
					{
						std::visit(
								unknown_msg_type_handlers::set_delivery_filter_t{
										msg_type,
										filter,
										subscriber },
								this->m_data.m_unknown_type_reaction );
					}
			}

		void
		drop_delivery_filter(
			const std::type_index & msg_type,
			agent_t & subscriber ) noexcept override
			{
				const auto opt_sink = try_find_sink_for_msg_type( msg_type );
				if( opt_sink )
					{
						(*opt_sink)->m_dest->drop_delivery_filter(
								msg_type,
								subscriber );
					}
				else
					{
						std::visit(
								unknown_msg_type_handlers::drop_delivery_filter_t{
										msg_type,
										subscriber },
								this->m_data.m_unknown_type_reaction );
					}
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return *(this->m_data.m_env_ptr);
			}

	private:
		//! Mbox's data.
		const mbox_data_t m_data;

		/*!
		 * \brief Attempt to find a sink for specified message type.
		 *
		 * \return empty std::optional if \a msg_type is unknown.
		 */
		[[nodiscard]]
		std::optional< const sink_t * >
		try_find_sink_for_msg_type( const std::type_index & msg_type ) const
			{
				const auto last = end( m_data.m_sinks );
				const auto it = std::lower_bound(
						begin( m_data.m_sinks ), last,
						msg_type,
						sink_compare );

				if( !( it == last ) && msg_type == it->m_msg_type )
					return { std::addressof( *it ) };
				else
					return std::nullopt;
			}

		/*!
		 * \brief Ensures that message is an immutable message.
		 *
		 * Checks mutability flag and throws an exception if message is
		 * a mutable one.
		 */
		void
		ensure_immutable_message(
			const std::type_index & msg_type,
			const message_ref_t & what ) const
			{
				if( (mbox_type_t::multi_producer_multi_consumer ==
						this->m_data.m_mbox_type) &&
						(message_mutability_t::immutable_message !=
								message_mutability( what )) )
					SO_5_THROW_EXCEPTION(
							so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
							"an attempt to deliver mutable message via MPMC mbox"
							", msg_type=" + std::string(msg_type.name()) );
			}
	};

} /* namespace impl */

/*!
 * \brief Factory class for building an instance of composite mbox.
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto mbox = single_consumer_builder(throw_if_not_found())
 * 	.add<msg_first>(first_mbox)
 * 	.add< so_5::mutable_msg<msg_second> >(second_mbox)
 * 	.make(env);
 * \endcode
 *
 * \note
 * This class is intended to be used in just one chain of add()..make() methods.
 * It means that code like that:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * // Simplest case without storing mbox_builder_t instance.
 * auto mbox = single_consumer_builder(throw_if_not_found())
 * 	.add<msg_first>(first_mbox)
 * 	.add< so_5::mutable_msg<msg_second> >(second_mbox)
 * 	.make(env);
 *
 * // More complex case with holding temporary mbox_builder_t instance.
 * auto my_builder = multi_consumer_builder(drop_if_not_found());
 * my_builder.add<msg_first>(first_mbox);
 * if(some_condition)
 * 	my_builder.add<msg_second>(second_mbox);
 * if(third_mbox_present)
 * 	my_builder.add<msg_third>(third_mbox);
 * auto mbox = my_builder.make(env);
 * \endcode
 * Will work in all versions of so5extra. But multiple calls to make() for
 * the same builder object are not guaranteed to be working. It's depend
 * on the current implementation and the implementation can change in
 * future versions of so5extra. It means that you have to avoid code
 * like that:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * // DO NOT DO THIS!
 * // The behaviour can change in future versions of so5extra without prior notify.
 * auto my_builder = multi_consumer_builder(redirect_to_if_not_found(default_dest));
 *
 * my_builder.add<msg_first>(first_mbox);
 * auto one = my_builder.make(env); // (1)
 *
 * my_builder.add<msg_second>(second_mbox);
 * auto two = my_builder.make(env);
 * \endcode
 * It's not guaranteed thet my_builder will be in a valid state after point (1).
 *
 * \attention
 * An instance of mbox_builder_t isn't thread safe.
 *
 * \note
 * This class has a private constructor and instance of builder can be
 * obtained only with help from builder(), single_consumer_builder(), and
 * multi_consumer_builder() functions.
 *
 * \since v.1.5.2
 */
class mbox_builder_t
	{
		friend mbox_builder_t
		builder(
			mbox_type_t mbox_type,
			type_not_found_reaction_t unknown_type_reaction );

		//! Initializing constructor.
		mbox_builder_t(
			//! Type of mbox to be created.
			mbox_type_t mbox_type,
			//! Reaction to a unknown message type.
			type_not_found_reaction_t unknown_type_reaction )
			:	m_mbox_type{ mbox_type }
			,	m_unknown_type_reaction{ std::move(unknown_type_reaction) }
			{}

	public:
		~mbox_builder_t() noexcept = default;

		/*!
		 * \brief Add destination mbox for a message type.
		 *
		 * Usage example:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * // A case with holding temporary mbox_builder_t instance.
		 * auto my_builder = multi_consumer_builder(drop_if_not_found());
		 * my_builder.add<msg_first>(first_mbox);
		 * if(some_condition)
		 * 	my_builder.add<msg_second>(second_mbox);
		 * if(third_mbox_present)
		 * 	my_builder.add<msg_third>(third_mbox);
		 * auto result_mbox = my_builder.make(env);
		 * \endcode
		 *
		 * If a type for mutable message has to be specified then
		 * so_5::mutable_msg marker should be used:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * auto my_builder = single_consumer_builder(drop_if_not_found());
		 * my_builder.add< so_5::mutable_msg<msg_first> >(first_mbox);
		 * \endcode
		 *
		 * Type of mutable message can't be used if:
		 *
		 * - composite mbox is MPMC mbox;
		 * - the destination mbox is MPMC mbox;
		 *
		 * \note
		 * If builder is created to produce a MPSC composite mbox then a MPMC
		 * mbox can be added as the destination mbox, but for immutable message
		 * only. For example:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * auto mpmc_dest = env.create_mbox(); // It's MPMC mbox.
		 *
		 * auto result_mbox = single_consumer_builder(throw_if_not_found())
		 * 	// This call is allowed because my_msg is immutable message.
		 * 	.add< my_msg >( mpmc_dest )
		 * 	...
		 * \endcode
		 *
		 * \attention
		 * An exception will be thrown if destination mbox is already registered
		 * for Msg_Type.
		 *
		 * \tparam Msg_Type type of message to be redirected to specified mbox.
		 */
		template< typename Msg_Type >
		mbox_builder_t &
		add( mbox_t dest_mbox ) &
			{
				// Use of mutable message type for MPMC mbox should be prohibited.
				if constexpr( is_mutable_message< Msg_Type >::value )
					{
						switch( m_mbox_type )
							{
							case mbox_type_t::multi_producer_multi_consumer:
								SO_5_THROW_EXCEPTION(
										so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
										"mutable message can't handled with MPMC composite, "
										"msg_type=" + std::string(typeid(Msg_Type).name()) );
							break;

							case mbox_type_t::multi_producer_single_consumer:
							break;
							}

						if( mbox_type_t::multi_producer_multi_consumer ==
							  dest_mbox->type() )
							{
								SO_5_THROW_EXCEPTION(
										errors::rc_mpmc_sink_can_be_used_with_mpsc_composite,
										"MPMC mbox can't be added as a sink to MPSC "
										"composite and mutable message, "
										"msg_type=" + std::string(typeid(Msg_Type).name()) );
							}
					}

				const auto [it, is_inserted] = m_sinks.emplace(
						message_payload_type< Msg_Type >::subscription_type_index(),
						std::move(dest_mbox) );
				if( !is_inserted )
					SO_5_THROW_EXCEPTION(
							errors::rc_message_type_already_has_sink,
							"message type already has a destination mbox, "
							"msg_type=" + std::string(typeid(Msg_Type).name()) );

				return *this;
			}

		/*!
		 * \brief Add destination mbox for a message type.
		 *
		 * Usage example:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * // Simplest case without storing mbox_builder_t instance.
		 * auto result_mbox = single_consumer_builder(throw_if_not_found())
		 * 	.add<msg_first>(first_mbox)
		 * 	.add< so_5::mutable_msg<msg_second> >(second_mbox)
		 * 	.make(env);
		 * \endcode
		 *
		 * If a type for mutable message has to be specified then
		 * so_5::mutable_msg marker should be used:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * auto result_mbox = single_consumer_builder(throw_if_not_found())
		 * 	.add< so_5::mutable_msg<message> >(dest_mbox)
		 * 	...
		 * \endcode
		 *
		 * Type of mutable message can't be used if:
		 *
		 * - composite mbox is MPMC mbox;
		 * - the destination mbox is MPMC mbox;
		 *
		 * \note
		 * If builder is created to produce a MPSC composite mbox then a MPMC
		 * mbox can be added as the destination mbox, but for immutable message
		 * only. For example:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * auto mpmc_dest = env.create_mbox(); // It's MPMC mbox.
		 *
		 * auto result_mbox = single_consumer_builder(throw_if_not_found())
		 * 	// This call is allowed because my_msg is immutable message.
		 * 	.add< my_msg >( mpmc_dest )
		 * 	...
		 * \endcode
		 *
		 * \attention
		 * An exception will be thrown if destination mbox is already registered
		 * for Msg_Type.
		 *
		 * \tparam Msg_Type type of message to be redirected to specified mbox.
		 */
		template< typename Msg_Type >
		[[nodiscard]]
		mbox_builder_t &&
		add( mbox_t dest_mbox ) &&
			{
				return std::move( add<Msg_Type>( std::move(dest_mbox) ) );
			}

		/*!
		 * \brief Make a composite mbox.
		 *
		 * The created mbox will be based on information added to builder
		 * before calling make() method.
		 *
		 * Usage example:
		 * \code
		 * using namespace so_5::extra::mboxes::composite;
		 *
		 * // Simplest case without storing mbox_builder_t instance.
		 * auto result_mbox = single_consumer_builder(throw_if_not_found())
		 * 	.add<msg_first>(first_mbox)
		 * 	.add< so_5::mutable_msg<msg_second> >(second_mbox)
		 * 	.make(env);
		 * \endcode
		 *
		 * It's guaranteed that the builder object will be in some correct
		 * state after make() returns. It means that builder can be safely
		 * deleted or can obtain a new value as the result of assignement.
		 * But it isn't guaranteed ther the builder will hold values previously
		 * stored to it by add() methods.
		 */
		[[nodiscard]]
		mbox_t
		make( environment_t & env )
			{
				return env.make_custom_mbox(
						[this]( const mbox_creation_data_t & data )
						{
							impl::mbox_data_t mbox_data{
									data.m_env.get(),
									data.m_id,
									m_mbox_type,
									std::move(m_unknown_type_reaction),
									sinks_to_vector()
								};
							mbox_t result;

							if( data.m_tracer.get().is_msg_tracing_enabled() )
								{
									using ::so_5::impl::msg_tracing_helpers::
											tracing_enabled_base;
									using T = impl::actual_mbox_t< tracing_enabled_base >;

									result = mbox_t{ new T{
											std::move(mbox_data),
											data.m_tracer.get()
										} };
								}
							else
								{
									using ::so_5::impl::msg_tracing_helpers::
											tracing_disabled_base;
									using T = impl::actual_mbox_t< tracing_disabled_base >;

									result = mbox_t{ new T{ std::move(mbox_data) } };
								}

							return result;
						} );
			}

	private:
		/*!
		 * \brief Type of container for holding sinks.
		 *
		 * \note
		 * std::map is used to simplify the implementation.
		 */
		using sink_map_t = std::map< std::type_index, mbox_t >;

		//! Type of mbox to be created.
		mbox_type_t m_mbox_type;

		//! Reaction to unknown type of a message.
		type_not_found_reaction_t m_unknown_type_reaction;

		//! Container for registered sinks.
		sink_map_t m_sinks;

		/*!
		 * \return A vector of sinks that should be passed to impl::actual_mbox_t
		 * constructor. That vector is guaranteed to be sorted (it means that
		 * binary search can be used for searching message types).
		 */
		[[nodiscard]]
		impl::sink_container_t
		sinks_to_vector() const
			{
				impl::sink_container_t result;
				result.reserve( m_sinks.size() );

				// Use the fact that items in std::map are ordered by keys.
				for( const auto & [k, v] : m_sinks )
					result.emplace_back( k, v );

				return result;
			}
	};

/*!
 * \brief Factory function for making mbox_builder.
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto result_mbox = builder(
 * 		so_5::mbox_type_t::multi_producer_multi_consumer,
 * 		redirect_to_if_not_found(default_mbox))
 * 	.add<msg_first>(first_mbox)
 * 	.add<msg_second>(second_mbox)
 * 	.add<msg_third>(third_mbox)
 * 	.make(env);
 * \endcode
 *
 * \since v.1.5.2
 */
[[nodiscard]]
inline mbox_builder_t
builder(
	//! Type of new mbox: MPMC or MPSC.
	mbox_type_t mbox_type,
	//! What to do if message type is unknown.
	type_not_found_reaction_t unknown_type_reaction )
	{
		return { mbox_type, std::move(unknown_type_reaction) };
	}

/*!
 * \brief Factory function for making mbox_builder that produces MPMC composite
 * mbox.
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto result_mbox = multi_consumer_builder(
 * 		redirect_to_if_not_found(default_mbox))
 * 	.add<msg_first>(first_mbox)
 * 	.add<msg_second>(second_mbox)
 * 	.add<msg_third>(third_mbox)
 * 	.make(env);
 * \endcode
 *
 * \since v.1.5.2
 */
[[nodiscard]]
inline mbox_builder_t
multi_consumer_builder(
	//! What to do if message type is unknown.
	type_not_found_reaction_t unknown_type_reaction )
	{
		return builder(
				mbox_type_t::multi_producer_multi_consumer,
				std::move(unknown_type_reaction) );
	}

/*!
 * \brief Factory function for making mbox_builder that produces MPSC composite
 * mbox.
 *
 * Usage example:
 * \code
 * using namespace so_5::extra::mboxes::composite;
 *
 * auto result_mbox = single_consumer_builder(
 * 		redirect_to_if_not_found(default_mbox))
 * 	.add<msg_first>(first_mbox)
 * 	.add< so_5::mutable_msg<msg_second> >(second_mbox)
 * 	.add<msg_third>(third_mbox)
 * 	.make(env);
 * \endcode
 *
 * \since v.1.5.2
 */
[[nodiscard]]
inline mbox_builder_t
single_consumer_builder(
	type_not_found_reaction_t unknown_type_reaction )
	{
		return builder(
				mbox_type_t::multi_producer_single_consumer,
				std::move(unknown_type_reaction) );
	}

} /* namespace composite */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

