/*!
 * \file Implementation of broadcasting mbox.
 *
 * \since
 * v.1.3.1
 */

#pragma once

#include <so_5/mbox.hpp>
#include <so_5/custom_mbox.hpp>
#include <so_5/environment.hpp>

namespace so_5
{

namespace extra
{

namespace mboxes
{

namespace broadcast
{

//FIXME: document this!
template< typename Container = std::vector<mbox_t> >
class fixed_mbox_template_t final : public abstract_message_box_t
{
	outliving_reference_t< environment_t > m_env;
	const mbox_id_t m_id;
	const Container m_destinations;

public :
	fixed_mbox_template_t(
		outliving_reference_t< environment_t > env,
		mbox_id_t id,
		const Container & destinations )
		:	m_env{ env }
		,	m_id{ id }
		,	m_destinations{ destinations }
		{}

	fixed_mbox_template_t(
		outliving_reference_t< environment_t > env,
		mbox_id_t id,
		Container && destinations )
		:	m_env{ env }
		,	m_id{ id }
		,	m_destinations{ std::move(destinations) }
		{}

	template< typename Input_It >
	fixed_mbox_template_t(
		outliving_reference_t< environment_t > env,
		mbox_id_t id,
		Input_It first, Input_It last )
		:	m_env{ env }
		,	m_id{ id }
		,	m_destinations{ first, last }
		{}

	mbox_id_t
	id() const override { return m_id; }

	void
	subscribe_event_handler(
		const std::type_index & /*type_index*/,
		const message_limit::control_block_t * /*limit*/,
		agent_t & /*subscriber*/ ) override
		{
			SO_5_THROW_EXCEPTION( rc_not_implemented,
					"subscribe_event_handler can't be used for broadcast mbox" );
		}

	void
	unsubscribe_event_handlers(
		const std::type_index & /*type_index*/,
		agent_t & /*subscriber*/ ) override
		{
			SO_5_THROW_EXCEPTION( rc_not_implemented,
					"unsubscribe_event_handler can't be used for broadcast mbox" );
		}

	std::string
	query_name() const override
		{
			std::ostringstream s;
			s << "<mbox:type=BROADCAST:id=" << this->m_id << ">";

			return s.str();
		}

	mbox_type_t
	type() const override
		{
			return mbox_type_t::multi_producer_multi_consumer;
		}

	void
	do_deliver_message(
		const std::type_index & msg_type,
		const message_ref_t & message,
		unsigned int overlimit_reaction_deep ) override
		{
			if( message_mutability_t::mutable_message == message_mutability(message) )
				SO_5_THROW_EXCEPTION(
						rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
						"a mutable message can't be sent via broadcast mbox" );

			for( auto & m : m_destinations )
				m->do_deliver_message( msg_type, message, overlimit_reaction_deep );
		}

	void
	set_delivery_filter(
		const std::type_index & /*msg_type*/,
		const delivery_filter_t & /*filter*/,
		agent_t & /*subscriber*/ ) override
		{
			SO_5_THROW_EXCEPTION( rc_not_implemented,
					"set_delivery_filter can't be used for broadcast mbox" );
		}

	void
	drop_delivery_filter(
		const std::type_index & /*msg_type*/,
		agent_t & /*subscriber*/ ) noexcept override
		{}

	environment_t &
	environment() const noexcept override
		{
			return m_env.get();
		}

	static mbox_t
	make(
		environment_t & env,
		const Container & destinations )
		{
			return env.make_custom_mbox(
					[&destinations]( const mbox_creation_data_t & data ) {
						return std::make_unique< fixed_mbox_template_t<Container> >(
								data.m_env,
								data.m_id,
								destinations );
					} );
		}

	static mbox_t
	make(
		environment_t & env,
		Container && destinations )
		{
			return env.make_custom_mbox(
					[&destinations]( const mbox_creation_data_t & data ) {
						return std::make_unique< fixed_mbox_template_t<Container> >(
								data.m_env,
								data.m_id,
								std::move(destinations) );
					} );
		}

	template< typename Input_It >
	static mbox_t
	make(
		environment_t & env,
		Input_It first,
		Input_It last )
		{
			return env.make_custom_mbox(
					[&first, &last]( const mbox_creation_data_t & data ) {
						return std::make_unique< fixed_mbox_template_t<Container> >(
								data.m_env,
								data.m_id,
								first, last );
					} );
		}
};

} /* namespace broadcast */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

