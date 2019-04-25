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
/*!
 * \brief Helper class to ensure that immutable_msg/mutable_msg
 * modificators are not used.
 */
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
	};

template< typename T >
struct ensure_no_mutability_modificators< mutable_msg<T> >
	{
		static_assert( so_5::details::always_false<T>::value,
				"so_5::mutable_msg<T> modificator can't be used with "
				"so_5::extra::sync::request_reply_t" );
	};

/*!
 * \brief A short form of ensure_no_mutability_modificators metafunction.
 */
template< typename T >
using ensure_no_mutability_modificators_t =
	typename ensure_no_mutability_modificators<T>::type;

//
// basic_request_reply_part_t
//
/*!
 * \brief The basic part of implementation of request_reply type.
 */
template< typename Request, typename Reply >
class basic_request_reply_part_t : public so_5::message_t
	{
	public :
		using request_t = ensure_no_mutability_modificators_t<Request>;
		using reply_t = ensure_no_mutability_modificators_t<Reply>;

		static_assert(
				(std::is_move_assignable_v<reply_t> &&
						std::is_move_constructible_v<reply_t>) ||
				(std::is_copy_assignable_v<reply_t> &&
						std::is_copy_constructible_v<reply_t>),
				"Reply type should be MoveAssignable or CopyAssignable" );

	protected :
		//! The chain to be used for reply message.
		so_5::mchain_t m_reply_ch;
		//! The flag for detection of repeated replies.
		/*!
		 * Recives `true` when the first reply is sent.
		 */
		bool m_reply_sent{ false };

		//! Initializing constructor.
		basic_request_reply_part_t( so_5::mchain_t reply_ch )
			:	m_reply_ch{ std::move(reply_ch) }
			{}

		//! Get access to the reply chain.
		const so_5::mchain_t &
		reply_ch() const noexcept { return m_reply_ch; }

	public :
		~basic_request_reply_part_t() override
			{
				// Close the reply chain.
				// If there is no reply but someone is waiting
				// on that chain it will be awakened.
				close_retain_content( m_reply_ch );
			}
	};

//
// request_holder_part_t
//
/*!
 * \brief The type to be used as holder for request type instance.
 *
 * Has two specialization for every variant of \a is_signal parameter.
 */
template< typename Base, bool is_signal >
class request_holder_part_t;

/*!
 * \brief A specialization for the case when request type is not a signal.
 *
 * This specialization holds an instance of request type.
 * This instance is constructed in request_holder_part_t's constructor
 * and is accessible via getters.
 */
template< typename Base >
class request_holder_part_t<Base, false> : public Base
	{
	protected :
		typename Base::request_t m_request;

		//! Initializing constructor.
		template< typename... Args >
		request_holder_part_t(
			so_5::mchain_t reply_ch,
			Args && ...args )
			:	Base{ std::move(reply_ch) }
			,	m_request{ std::forward<Args>(args)... }
			{}

	public :
		//! Getter for the case of const object.
		const auto &
		request() const noexcept { return m_request; }

		//! Getter for the case of non-const object.
		auto &
		request() noexcept { return m_request; }
	};

/*!
 * \brief A specialization for the case when request type is a signal.
 *
 * There is no need to hold anything. Becase of that this type is empty.
 */
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
/*!
 * \brief A special class for performing interactions between
 * agents in request-reply maner.

Some older versions of SObjectizer-5 supported synchronous interactions between
agents. But since SObjectizer-5.6 this functionality has been removed from
SObjectizer core. Some form of synchronous interaction is now supported via
so_5::extra::sync.

The request_reply_t template is the main building block for the synchronous
interaction between agents in the form of request-reply. The basic usage
example is looked like the following:

\code
struct my_request {
	int a_;
	std::string b_;
};

struct my_reply {
	std::string c_;
	std::pair<int, int> d_;
};

namespace sync_ns = so_5::extra::sync;

// The agent that processes requests.
class service final : public so_5::agent_t {
	...
	void on_request(
			sync_ns::request_reply_t<my_request, my_reply>::request_mhood_t cmd) {
		...; // Some processing.
		// Now the reply can be sent. Instance of my_reply will be created
		// automatically.
		cmd->make_reply(
			// Value for my_reply::c_ field.
			"Reply", 
			// Value for my_reply::d_ field.
			std::make_pair(0, 1) );
	}
}
...
// Mbox of service agent.
const so_5::mbox_t svc_mbox = ...;

// Issue the request and wait reply for at most 15s.
// An exception of type so_5::exception_t will be sent if reply
// is not received in 15 seconds.
my_reply reply = sync_ns::request_value<my_request, my_reply>(
	// Destination of the request.
	svc_mbox,
	// Max waiting time.
	15s,
	// Parameters for initialization of my_request instance.
	// Value for my_request::a_ field.
	42,
	// Value for my_request::b_ field.
	"Request");

// Or, if we don't want to get an exception.
optional<my_reply> opt_reply = sync_ns::request_opt_value<my_request, my_reply>(
	svc_mbox,
	15s,
	4242,
	"Request #2");
\endcode

 */
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
		using request_t = typename base_type::request_t;
		using reply_t = typename base_type::reply_t;

		using request_mhood_t = mutable_mhood_t< request_reply_t >;
		using reply_mhood_t = mutable_mhood_t< reply_t >;

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

				so_5::send< so_5::mutable_msg<reply_t> >(
						this->m_reply_ch, std::forward<Args>(args)... );

				this->m_reply_sent = true;
			}
	};

//
// request_opt_value
//
template<
	typename Request,
	typename Reply,
	typename Target,
	typename Duration,
	typename... Args >
SO_5_NODISCARD
auto
request_opt_value(
	Target && target,
	Duration duration,
	Args && ...args )
	{
		using requester_type = request_reply_t<Request, Reply>;

		using reply_t = typename requester_type::reply_t;

		auto reply_ch = requester_type::initiate(
				std::forward<Target>(target),
				std::forward<Args>(args)... );

		optional<reply_t> result;
		receive(
				from(reply_ch).handle_n(1).empty_timeout(duration),
				[&result]( typename requester_type::reply_mhood_t cmd ) {
					if constexpr( std::is_move_assignable_v<reply_t> &&
							std::is_move_constructible_v<reply_t> )
						result = std::move(*cmd);
					else
						result = *cmd;
				} );

		return result;
	}

//
// request_value
//
template<
	typename Request,
	typename Reply,
	typename Target,
	typename Duration,
	typename... Args >
SO_5_NODISCARD
auto
request_value(
	Target && target,
	Duration duration,
	Args && ...args )
	{
		auto result = request_opt_value<Request, Reply>(
				std::forward<Target>(target),
				duration,
				std::forward<Args>(args)... );

		if( !result )
			{
				using requester_type = request_reply_t<Request, Reply>;
				SO_5_THROW_EXCEPTION( errors::rc_no_reply,
						std::string{ "no reply received, request_reply type: " } +
						typeid(requester_type).name() );
			}

		return *result;
	}

} /* namespace sync */

} /* namespace extra */

} /* namespace so_5 */

