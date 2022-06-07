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

} /* namespace errors */

//FIXME: document this!
/*!
 *
 * \since v.1.5.2
 */
class delegate_to_if_not_found_case_t
	{
		//! Destination for message of unknown type.
		mbox_t m_dest;

	public:
		//! Initializing constructor.
		delegate_to_if_not_found_case_t( mbox_t dest )
			//FIXME: should there be check that dest isn't null?
			:	m_dest{ std::move(dest) }
			{}

		//! Getter.
		[[nodiscard]]
		const mbox_t &
		dest() const noexcept { return m_dest; }
	};

//FIXME: document this!
/*!
 *
 * \since v.1.5.2
 */
struct throw_if_not_found_case_t
	{};

//FIXME: document this!
/*!
 *
 * \since v.1.5.2
 */
struct drop_if_not_found_case_t
	{};

//FIXME: document this!
/*!
 *
 * \since v.1.5.2
 */
using type_not_found_reaction_t = std::variant<
		delegate_to_if_not_found_case_t,
		throw_if_not_found_case_t,
		drop_if_not_found_case_t
	>;

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
[[nodiscard]]
inline type_not_found_reaction_t
delegate_to_if_not_found( const mbox_t & dest_mbox )
	{
		return { delegate_to_if_not_found_case_t{ dest_mbox } };
	}

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
[[nodiscard]]
inline type_not_found_reaction_t
throw_if_not_found()
	{
		return { throw_if_not_found_case_t{} };
	}

//FIXME: document this!
/*!
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

//FIXME: document this!
/*!
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

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
using sink_container_t = std::vector< sink_t >;

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
inline const auto sink_compare =
		[]( const sink_t & sink, const std::type_index & msg_type ) -> bool {
			return sink.m_msg_type < msg_type;
		};

namespace unknown_msg_type_handlers
{

//FIXME: document this!
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
		operator()( const delegate_to_if_not_found_case_t & c ) const
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

//FIXME: document this!
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
		operator()( const delegate_to_if_not_found_case_t & c ) const
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

//FIXME: document this!
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
		operator()( const delegate_to_if_not_found_case_t & c ) const
			{
				using namespace ::so_5::impl::msg_tracing_helpers::details;

				m_tracer.make_trace(
						"delegate_to_default_destination",
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

//FIXME: document this!
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
		operator()( const delegate_to_if_not_found_case_t & c ) const
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

//FIXME: document this!
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
		operator()( const delegate_to_if_not_found_case_t & c ) const noexcept
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

//FIXME: document this!
/*!
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
								"delegate_to_destination",
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

		//FIXME: document this!
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

//FIXME: document this!
class mbox_builder_t
	{
		//FIXME: friends have to be declared here!
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

		//FIXME: document this!
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

		//FIXME: document this!
		template< typename Msg_Type >
		[[nodiscard]]
		mbox_builder_t &&
		add( mbox_t dest_mbox ) &&
			{
				return std::move( add<Msg_Type>( std::move(dest_mbox) ) );
			}

		//FIXME: document this!
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

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
[[nodiscard]]
inline mbox_builder_t
builder(
	mbox_type_t mbox_type,
	type_not_found_reaction_t unknown_type_reaction )
	{
		return { mbox_type, std::move(unknown_type_reaction) };
	}

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
[[nodiscard]]
inline mbox_builder_t
multi_consumer_builder(
	type_not_found_reaction_t unknown_type_reaction )
	{
		return builder(
				mbox_type_t::multi_producer_multi_consumer,
				std::move(unknown_type_reaction) );
	}

//FIXME: document this!
/*!
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

