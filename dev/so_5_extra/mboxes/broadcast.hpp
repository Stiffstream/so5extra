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

#include <iterator>

namespace so_5
{

namespace extra
{

namespace mboxes
{

namespace broadcast
{

/*!
 * \brief A template for broadcasting mbox with fixed set of destinations.
 *
 * \note
 * A set of destination is fixed at the creation time and can't be changed
 * later. It can be not flexible enough for some senarios, but allows to
 * avoid any additional locks during the delivery of a message.
 *
 * \note
 * This type has no public constructors. To create an instance of that
 * type public static `make` methods should be used.
 *
 * \attention
 * This type of mbox prohibits the delivery of mutable messages. It is
 * because this is MPMC mbox.
 *
 * \attention
 * This type of mbox prohibits subscriptions and usage of delivery filters.
 * An attempt to create a subscription or an attempt to set a delivery
 * filter will lead to an exception.
 *
 * \tparam Container Type of a container for holding list of destination
 * mboxes. By default it is `std::vector<mbox_t>` but a user can set
 * any type of sequential container. For example:
 * \code
 * using my_broadcast_mbox = so_5::extra::mboxes::broadcast::fixed_mbox_template_t< std::array<so_5::mbox_t, 10> >;
 * \endcode
 *
 * \since
 * v.1.3.1
 */
template< typename Container = std::vector<mbox_t> >
class fixed_mbox_template_t final : public abstract_message_box_t
{
	outliving_reference_t< environment_t > m_env;
	const mbox_id_t m_id;
	const Container m_destinations;

protected :
	//! Initializing constructor.
	/*!
	 * Intended for the case when a set of destinations should be taken
	 * from a const reference to the container of the same type.
	 */
	fixed_mbox_template_t(
		//! SObjectizer Environment to work in.
		outliving_reference_t< environment_t > env,
		//! A unique ID of that
		mbox_id_t id,
		//! Source container with a set of destination mboxes.
		const Container & destinations )
		:	m_env{ env }
		,	m_id{ id }
		,	m_destinations{ destinations }
		{}

	//! Initializing constructor.
	/*!
	 * Intended for the case when a set of destinations should be borrowed
	 * (moved) from a temporary container of the same type.
	 */
	fixed_mbox_template_t(
		//! SObjectizer Environment to work in.
		outliving_reference_t< environment_t > env,
		//! A unique ID of that
		mbox_id_t id,
		//! Source container with a set of destination mboxes.
		//! The content of this container will be borrowed.
		Container && destinations )
		:	m_env{ env }
		,	m_id{ id }
		,	m_destinations{ std::move(destinations) }
		{}

	//! Initializing constructor.
	/*!
	 * Intended for the case when a set of destination mboxes is specified
	 * by a pair of iterators.
	 */
	template< typename Input_It >
	fixed_mbox_template_t(
		//! SObjectizer Environment to work in.
		outliving_reference_t< environment_t > env,
		//! A unique ID of that
		mbox_id_t id,
		//! The left border of a range (inclusive).
		Input_It first,
		//! The right border of a range (exclusive).
		Input_It last )
		:	m_env{ env }
		,	m_id{ id }
		,	m_destinations{ first, last }
		{}

public :
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

	/*!
	 * \brief Factory method for the creation of new instance of a mbox.
	 *
	 * Copies the whole content from \a destinations container.
	 *
	 * Usage example:
	 * \code
	 * using broadcasting_mbox = so_5::extra::mboxes::broadcast::fixed_mbox_template_t<>;
	 *
	 * std::vector< so_5::mbox_t > destinations;
	 * destinations.push_back( some_agent->so_direct_mbox() );
	 * destinations.push_back( another_agent->so_direct_mbox() );
	 * ...
	 * auto first_broadcaster = broadcasting_mbox::make( env, destinations );
	 * auto second_broadcaster = broadcasting_mbox::make( env, destinations );
	 * ...
	 * \endcode
	 */
	static mbox_t
	make(
		//! SObjectizer Environment to work in.
		environment_t & env,
		//! A set of destinations for a new mbox.
		const Container & destinations )
		{
			return env.make_custom_mbox(
					[&destinations]( const mbox_creation_data_t & data ) {
						return std::unique_ptr< fixed_mbox_template_t >{
								new fixed_mbox_template_t(
										data.m_env,
										data.m_id,
										destinations )
							};
					} );
		}

	/*!
	 * \brief Factory method for the creation of new instance of a mbox.
	 *
	 * Borrows (moves from) the whole content from \a destinations container.
	 *
	 * Usage example:
	 * \code
	 * using broadcasting_mbox = so_5::extra::mboxes::broadcast::fixed_mbox_template_t<>;
	 *
	 * std::vector< so_5::mbox_t > make_destinations() {
	 * 	std::vector< so_5::mbox_t > result;
	 * 	result.push_back( some_agent->so_direct_mbox() );
	 * 	result.push_back( another_agent->so_direct_mbox() );
	 * 	...
	 * 	return result;
	 * }
	 *
	 * auto broadcaster = broadcasting_mbox::make( env, make_destinations() );
	 * ...
	 * \endcode
	 */
	static mbox_t
	make(
		//! SObjectizer Environment to work in.
		environment_t & env,
		//! A temporary container with a set of destination mboxes.
		//! The content of that container will be moved into a new mbox.
		Container && destinations )
		{
			return env.make_custom_mbox(
					[&destinations]( const mbox_creation_data_t & data ) {
						return std::unique_ptr< fixed_mbox_template_t >{
								new fixed_mbox_template_t(
										data.m_env,
										data.m_id,
										std::move(destinations) )
							};
					} );
		}

	/*!
	 * \brief Factory method for the creation of new instance of a mbox.
	 *
	 * Uses values from a range [\a first, \a last) for initialization of
	 * destinations container.
	 *
	 * Usage example:
	 * \code
	 * using broadcasting_mbox = so_5::extra::mboxes::broadcast::fixed_mbox_template_t<>;
	 *
	 * so_5::mbox_t destinations[] = {
	 * 	some_agent->so_direct_mbox(),
	 * 	another_agent->so_direct_mbox(),
	 * 	...
	 * };
	 *
	 * auto broadcaster = broadcasting_mbox::make( env,
	 * 		std::begin(destinations), std::end(destinations) );
	 * ...
	 * \endcode
	 */
	template< typename Input_It >
	static mbox_t
	make(
		//! SObjectizer Environment to work in.
		environment_t & env,
		//! The left border of a range (inclusive).
		Input_It first,
		//! The right border of a range (exclusive).
		Input_It last )
		{
			return env.make_custom_mbox(
					[&first, &last]( const mbox_creation_data_t & data ) {
						return std::unique_ptr< fixed_mbox_template_t >{
								new fixed_mbox_template_t(
										data.m_env,
										data.m_id,
										first, last )
							};
					} );
		}

	/*!
	 * \brief Factory method for the creation of new instance of a mbox.
	 *
	 * Uses the whole content of a container of other type.
	 *
	 * Usage example:
	 * \code
	 * using broadcasting_mbox = so_5::extra::mboxes::broadcast::fixed_mbox_template_t<>;
	 *
	 * std::array<so_5::mbox_t, 5> destinations{
	 * 	some_agent->so_direct_mbox(),
	 * 	another_agent->so_direct_mbox(),
	 * 	...
	 * };
	 *
	 * auto broadcaster = broadcasting_mbox::make( env, destinations );
	 * ...
	 * \endcode
	 */
	template< typename Another_Container >
	static mbox_t
	make(
		//! SObjectizer Environment to work in.
		environment_t & env,
		//! The container (or range object) with a set of destinations to a new mbox.
		const Another_Container & destinations )
		{
			using std::begin;
			using std::end;

			// Ensure that destinations if a container or range-like object
			// with mbox_t inside.
			static_assert(
				std::is_convertible_v<
					decltype(
						++std::declval<
							std::add_lvalue_reference_t<
								decltype(begin(std::declval<const Another_Container &>()))>
							>()
						== end(std::declval<const Another_Container &>()) ),
					bool>,
				"destinations should be a container or range-like object" );

			static_assert(
				std::is_same_v<
					std::decay_t<
							decltype(*begin(std::declval<const Another_Container &>())) >,
					mbox_t >,
				"mbox_t should be accessible via iterator for destinations container (or "
				"range-like object" );

			return make( env, begin(destinations), end(destinations) );
		}

};

} /* namespace broadcast */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

