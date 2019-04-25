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

#include <so_5/details/always_false.hpp>

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

namespace details
{

//
// ensure_no_mutability_modificators
//
template< typename T >
struct ensure_no_mutability_modificators
	{
		using type = T;
	};

template< typename T >
struct ensure_no_mutability_modificators< immutable_msg<T> >
	{
		static_assert( so_5::details::always_false<T>::value,
				"so_5::immutable_msg<T> modificator can't be used with "
				"so_5::extra::sync::request_reply_t" );

		using type = T;
	};

template< typename T >
struct ensure_no_mutability_modificators< mutable_msg<T> >
	{
		static_assert( so_5::details::always_false<T>::value,
				"so_5::mutable_msg<T> modificator can't be used with "
				"so_5::extra::sync::request_reply_t" );

		using type = T;
	};

template< typename T >
using ensure_no_mutability_modificators_t =
	typename ensure_no_mutability_modificators<T>::type;

//
// basic_request_reply_part_t
//
template< typename Request, typename Reply >
class basic_request_reply_part_t : public so_5::message_t
	{
	public :
		using request_type = ensure_no_mutability_modificators_t<Request>;
		using reply_type = ensure_no_mutability_modificators_t<Reply>;

		static_assert(
				(std::is_move_assignable_v<reply_type> &&
						std::is_move_constructible_v<reply_type>) ||
				(std::is_copy_assignable_v<reply_type> &&
						std::is_copy_constructible_v<reply_type>),
				"Reply type should be MoveAssignable or CopyAssignable" );

	protected :
		so_5::mchain_t m_reply_ch;
		bool m_reply_sent{ false };

		basic_request_reply_part_t( so_5::mchain_t reply_ch )
			:	m_reply_ch{ std::move(reply_ch) }
			{}

	public :
		~basic_request_reply_part_t() override
			{
				close_retain_content( m_reply_ch );
			}

		const so_5::mchain_t &
		reply_ch() const noexcept { return m_reply_ch; }
	};

//
// request_holder_part_t
//
template< typename Base, bool is_signal >
class request_holder_part_t;

template< typename Base >
class request_holder_part_t<Base, false> : public Base
	{
	protected :
		typename Base::request_type m_request;

		template< typename... Args >
		request_holder_part_t(
			so_5::mchain_t reply_ch,
			Args && ...args )
			:	Base{ std::move(reply_ch) }
			,	m_request{ std::forward<Args>(args)... }
			{}

	public :
		const auto &
		request() const noexcept { return m_request; }

		auto &
		request() noexcept { return m_request; }
	};

template< typename Base >
class request_holder_part_t<Base, true> : public Base
	{
	protected :
		using Base::Base;
	};

} /* namespace details */

//
// request_reply_t
//
//FIXME: document this!
template<typename Request, typename Reply>
class request_reply_t final
	:	public details::request_holder_part_t<
				details::basic_request_reply_part_t<Request, Reply>,
				is_signal<Request>::value >
	{
		using base_type = details::request_holder_part_t<
				details::basic_request_reply_part_t<Request, Reply>,
				is_signal<Request>::value >;

	public :
		using request_type = typename base_type::request_type;
		using reply_type = typename base_type::reply_type;

	private :
		using base_type::base_type;

	public :
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
				if( this->m_reply_sent )
					SO_5_THROW_EXCEPTION( errors::rc_reply_was_sent,
							std::string{ "reply has already been sent, "
									"request_reply type: " } +
							typeid(request_reply_t).name() );

				so_5::send< so_5::mutable_msg<reply_type> >(
						this->m_reply_ch, std::forward<Args>(args)... );

				this->m_reply_sent = true;
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
		using requester_type = request_reply_t<Request, Reply>;

		using reply_type = typename requester_type::reply_type;

		auto reply_ch = requester_type::initiate(
				std::forward<Target>(target),
				std::forward<Args>(args)... );

		optional<reply_type> result;
		receive(
				from(reply_ch).handle_n(1).empty_timeout(duration),
				[&result]( so_5::mutable_mhood_t<reply_type> cmd ) {
					if constexpr( std::is_move_assignable_v<reply_type> &&
							std::is_move_constructible_v<reply_type> )
						result = std::move(*cmd);
					else
						result = *cmd;
				} );

		if( !result )
			SO_5_THROW_EXCEPTION( errors::rc_no_reply,
					std::string{ "no reply received, request_reply type: " } +
					typeid(requester_type).name() );

		return *result;
	}

} /* namespace sync */

} /* namespace extra */

} /* namespace so_5 */

