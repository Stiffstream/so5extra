/*!
 * \file
 * \brief Implementation of composite mbox.
 *
 * \since v.1.5.2
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/mbox.hpp>

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
	};

//FIXME: document this!
/*!
 * \since v.1.5.2
 */
using sink_container_t = std::vector< sink_t >;

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
class actual_mbox_t final : public abstract_message_box_t
	{
		//FIXME: friends have to be declated here!

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
			const std::type_index & /*msg_type*/,
			const so_5::message_limit::control_block_t * /*limit*/,
			agent_t & /*subscriber*/ ) override
			{
//FIXME: implement this!
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) override
			{
//FIXME: implement this!
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
			const std::type_index & /*msg_type*/,
			const message_ref_t & /*message*/,
			unsigned int /*overlimit_reaction_deep*/ ) override
			{
//FIXME: implement this!
#if 0
				//FIXME: mutability of message has to be checked!
				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_base
						*this, // as abstract_message_box_t
						"deliver_message",
						msg_type, message, overlimit_reaction_deep };

				do_deliver_message_impl(
						tracer,
						msg_type,
						message,
						overlimit_reaction_deep );
#endif
			}

		void
		set_delivery_filter(
			const std::type_index & /*msg_type*/,
			const delivery_filter_t & /*filter*/,
			agent_t & /*subscriber*/ ) override
			{
//FIXME: implement this!
			}

		void
		drop_delivery_filter(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) noexcept override
			{
//FIXME: implement this!
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return *(this->m_data.m_env_ptr);
			}

	private:
		//! Mbox's data.
		const mbox_data_t m_data;
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
		[[nodiscard]]
		mbox_builder_t &
		add( mbox_t dest_mbox ) &
			{
				if constexpr( is_mutable_message< Msg_Type >::value )
					{
						switch( m_mbox_type )
							{
							case mbox_type_t::multi_producer_multi_consumer:
								SO_5_THROW_EXCEPTION(
										so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
										"an attempt to make MPMC mbox for mutable message, "
										"msg_type=" + std::string(typeid(Msg_Type).name()) );
							break;

							case mbox_type_t::multi_producer_single_consumer:
							break;
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

} /* namespace composite */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */
