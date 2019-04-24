/*!
 * \file
 * \brief Implementation of synchronous operations on top of SObjectizer.
 *
 * \since
 * v.1.3.0
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/send_functions.hpp>

namespace so_5 {

namespace extra {

namespace sync {

namespace errors {

/*!
 * \brief An attempt to send a new reply when the reply is already sent.
 *
 * Only one reply can be sent as a result of request_reply-interaction.
 * An attempt to send another reply is an error.
 */
const int rc_reply_was_sent =
	so_5::extra::errors::sync_errors;

/*!
 * \brief No reply.
 *
 * The reply has not been received after waiting for the specified time.
 */
const int rc_no_reply =
	so_5::extra::errors::sync_errors + 1;

} /* namespace errors */

//
// request_reply_t
//
//FIXME: document this!
template<typename Request, typename Reply>
class request_reply_t final : public so_5::message_t
	{
	public :
		using request_type = Request;
		using reply_type = Reply;

	private :
		so_5::mchain_t m_reply_ch;

		request_type m_request;

		bool m_reply_sent{ false };

		template< typename... Args >
		request_reply_t(
			so_5::mchain_t reply_ch,
			Args && ...args )
			:	m_reply_ch{ std::move(reply_ch) }
			,	m_request( std::forward<Args>(args)... )
			{}

	public :
		~request_reply_t() override
			{
				close_retain_content( m_reply_ch );
			}

		const request_type &
		request() const noexcept { return m_request; }

		request_type &
		request() noexcept { return m_request; }

		so_5::mchain_t
		reply_ch() const noexcept { return m_reply_ch; }

		template< typename Target, typename... Args >
		SO_5_NODISCARD
		static so_5::mchain_t
		initiate( const Target & target, Args && ...args )
			{
				auto mchain = create_mchain(
						so_5::send_functions_details::arg_to_env( target ),
						1u, // Only one message should be stored in reply_ch.
						so_5::mchain_props::memory_usage_t::preallocated,
						so_5::mchain_props::overflow_reaction_t::throw_exception );

				message_holder_t< mutable_msg< request_reply_t > > msg{
					// Calling 'new' directly because request_reply_t has
					// private constructor.
					new request_reply_t{ mchain, std::forward<Args>(args)... }
				};

				send( target, std::move(msg) );

				return mchain;
			}

		template< typename... Args >
		void
		make_reply( Args && ...args )
			{
				if( m_reply_sent )
					SO_5_THROW_EXCEPTION( errors::rc_reply_was_sent,
							std::string{ "reply has already been sent, "
									"request_reply type: " } +
							typeid(request_reply_t).name() );

				so_5::send< reply_type >( m_reply_ch, std::forward<Args>(args)... );
				m_reply_sent = true;
			}
	};

template<
	typename Request,
	typename Reply,
	typename Target,
	typename Duration,
	typename... Args >
auto
request_value(
	Target && target,
	Duration duration,
	Args && ...args )
	{
		using request_type = request_reply_t<Request, Reply>;

		auto reply_ch = request_type::initiate(
				std::forward<Target>(target),
				std::forward<Args>(args)... );

		optional<typename request_type::reply_type> result;
		receive(
				from(reply_ch).handle_n(1).empty_timeout(duration),
				[&result]( mhood_t<Reply> cmd ) {
					// For the simplicity.
					result = *cmd;
				} );

		if( !result )
			SO_5_THROW_EXCEPTION( errors::rc_no_reply,
					std::string{ "no reply received, request_reply type: " } +
					typeid(request_type).name() );

		return *result;
	}


} /* namespace sync */

} /* namespace extra */

} /* namespace so_5 */

