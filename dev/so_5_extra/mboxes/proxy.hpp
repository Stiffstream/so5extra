/*!
 * \file
 * \brief Implementation of simple mbox proxy.
 *
 * \since
 * v.1.2.0
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/mbox.hpp>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace proxy {

namespace errors {

/*!
 * \brief Null pointer to underlying mbox.
 *
 * A proxy-mbox uses underlying mbox and delegates all actions to that mbox.
 * Because of that underlying mbox can't be nullptr.
 *
 * \since
 * v.1.2.0
 */
const int rc_nullptr_as_underlying_mbox =
		::so_5::extra::errors::mboxes_proxy_errors + 1;

} /* namespace errors */

//
// simple_t
//
/*!
 * \brief A simple proxy that delegates all calls to underlying actual mbox.
 *
 * Sometimes it is necessary to create an own mbox that does
 * some specific task. For example, counts the number of message
 * of some specific type. Something like:
 * \code
 * class my_mbox final : public so_5::abstract_message_box_t
 * {
 * 	bool is_appropriate_message_type(
 * 		const std::type_index & msg_type ) const { ... }
 *
 * 	std::size_t counter_{};
 * ...
 * 	void do_deliver_message(
 * 			const std::type_index & msg_type,
 * 			const so_5::message_ref_t & message,
 * 			unsigned int overlimit_reaction_deep ) override {
 * 		if(is_appropriate_message_type(msg_type))
 * 			++counter_;
 * 		... // Actual message delivery.
 * 	}
 * };
 * \endcode
 * But it is hard to create a full implementation of
 * so_5::abstract_message_box_t from the ground up. An existing mbox can be
 * used for doing actual work:
 * \code
 * class my_mbox final : public so_5::abstract_message_box_t
 * {
 * 	bool is_appropriate_message_type(
 * 		const std::type_index & msg_type ) const { ... }
 *
 * 	const so_5::mbox_t mbox_;
 * 	std::size_t counter_{};
 * ...
 * public:
 * 	my_mbox(so_5::mbox_t mbox) : mbox_{std::move(mbox)} {}
 * 	...
 * 	void do_deliver_message(
 * 			const std::type_index & msg_type,
 * 			const so_5::message_ref_t & message,
 * 			unsigned int overlimit_reaction_deep ) override {
 * 		if(is_appropriate_message_type(msg_type))
 * 			++counter_;
 * 		// Use actual mbox for message delivery.
 * 		mbox_->do_deliver_message(
 * 				msg_type, message, overlimit_reaction_deep);
 * 	}
 * };
 * \endcode
 * But there is a small problem with this approach: so_5::abstract_message_box_t
 * has a rich interface with a lot of pure virtual methods. It is a boring
 * task to reimplement all of them.
 *
 * In such cases simple_t can be used to reduce amount of developer's work:
 * \code
 * class my_mbox final : public so_5::extra::mboxes::proxy::simple_t
 * {
 * 	using base_type = so_5::extra::mboxes::proxy::simple_t;
 *
 * 	bool is_appropriate_message_type(
 * 		const std::type_index & msg_type ) const { ... }
 *
 * 	std::size_t counter_{};
 *
 * public:
 * 	my_mbox(so_5::mbox_t mbox) : base_type{std::move(mbox)} {}
 *
 * 	void do_deliver_message(
 * 			const std::type_index & msg_type,
 * 			const so_5::message_ref_t & message,
 * 			unsigned int overlimit_reaction_deep ) override {
 * 		if(is_appropriate_message_type(msg_type))
 * 			++counter_;
 * 		// Use actual mbox for message delivery.
 * 		base_type::do_deliver_message(
 * 				msg_type, message, overlimit_reaction_deep);
 * 	}
 * }
 * \endcode
 * And that's all.
 *
 * \since
 * v.5.5.23
 */
class simple_t : public ::so_5::abstract_message_box_t
	{
		//! Actual underlying mbox to be used for all calls.
		/*!
		 * \attention Should not be nullptr.
		 */
		const ::so_5::mbox_t m_underlying_mbox;

		//! Ensure that underlying mbox is not nullptr.
		/*!
		 * \throw so_5::exception_t if \a mbox is nullptr.
		 */
		::so_5::mbox_t
		ensure_underlying_mbox_not_null(
			::so_5::mbox_t mbox )
			{
				if( !mbox )
					SO_5_THROW_EXCEPTION(
							errors::rc_nullptr_as_underlying_mbox,
							"nullptr is used as underlying mbox" );

				return mbox;
			}

	protected :
		//! An accessor to actual mbox.
		/*!
		 * This method is intended to be used in derived classes.
		 * For example:
		 * \code
		 * class my_mbox : public so_5::extra::mboxes::proxy::simple_t {
		 * public:
		 * 	void do_deliver_message(
		 * 			const std::type_index & msg_type,
		 * 			const so_5::message_ref_t & message,
		 * 			unsigned int overlimit_reaction_deep ) override {
		 * 		... // Do some specific stuff.
		 * 		// Use actual mbox for message delivery.
		 * 		underlying_mbox().do_deliver_message(
		 * 				msg_type, message, overlimit_reaction_deep);
		 * 	}
		 * 	...
		 * };
		 * \endcode
		 */
		::so_5::abstract_message_box_t &
		underlying_mbox() const noexcept
			{
				return *m_underlying_mbox;
			}

	public :
		//! Initializing constructor.
		simple_t(
			//! Actual underlying mbox to be used for all operations.
			//! Must not be nullptr.
			::so_5::mbox_t underlying_mbox )
			:	m_underlying_mbox{
					ensure_underlying_mbox_not_null( std::move(underlying_mbox) ) }
			{}

		/*!
		 * \name Simple implementation of inherited methods.
		 * \{
		 */
		mbox_id_t
		id() const override
			{
				return underlying_mbox().id();
			}

		void
		subscribe_event_handler(
			const std::type_index & type_index,
			const ::so_5::message_limit::control_block_t * limit,
			::so_5::agent_t & subscriber ) override
			{
				underlying_mbox().subscribe_event_handler(
						type_index,
						limit,
						subscriber );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & type_index,
			::so_5::agent_t & subscriber ) override
			{
				underlying_mbox().unsubscribe_event_handlers(
						type_index,
						subscriber );
			}

		std::string
		query_name() const override
			{
				return underlying_mbox().query_name();
			}

		mbox_type_t
		type() const override
			{
				return underlying_mbox().type();
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const ::so_5::message_ref_t & message,
			unsigned int overlimit_reaction_deep ) override
			{
				underlying_mbox().do_deliver_message(
						msg_type,
						message,
						overlimit_reaction_deep );
			}

		void
		set_delivery_filter(
			const std::type_index & msg_type,
			const ::so_5::delivery_filter_t & filter,
			::so_5::agent_t & subscriber ) override
			{
				underlying_mbox().set_delivery_filter(
						msg_type,
						filter,
						subscriber );
			}

		void
		drop_delivery_filter(
			const std::type_index & msg_type,
			::so_5::agent_t & subscriber ) noexcept override
			{
				underlying_mbox().drop_delivery_filter(
						msg_type,
						subscriber );
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return underlying_mbox().environment();
			}

	protected :
		void
		do_deliver_message_from_timer(
			const std::type_index & msg_type,
			const ::so_5::message_ref_t & message ) override
			{
				delegate_deliver_message_from_timer(
						underlying_mbox(),
						msg_type,
						message );
			}
		/*!
		 * \}
		 */
	};

} /* namespace proxy */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

