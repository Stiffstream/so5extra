/*!
 * \file
 * \brief Implementation of synchronous operations on top of SObjectizer.
 *
 * \since
 * v.1.3.0
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5_extra/mchains/fixed_size.hpp>

#include <so_5/send_functions.hpp>

#include <so_5/details/always_false.hpp>

#include <variant>

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
// reply_target_t
//
/*!
 * \brief Type of storage for reply's target.
 *
 * A reply can be sent to a mchain or to a mbox. If a mchain is used as
 * a target then it should be closed.
 *
 * This type allow to distinguish between mchain and mbox cases.
 */
using reply_target_t = std::variant< mchain_t, mbox_t >;

//
// reply_target_holder_t
//
/*!
 * \brief A special holder for reply_target instance.
 *
 * This class performs the proper cleanup in the destructor: if
 * reply_target instance holds a mchain that mchain will be closed
 * automatically.
 *
 * \note
 * This class is not Moveable nor Copyable.
 */
class reply_target_holder_t final
	{
		//! The target for the reply message.
		reply_target_t m_target;

	public :
		reply_target_holder_t( reply_target_t target ) noexcept
			:	m_target{ std::move(target) }
			{}

		~reply_target_holder_t() noexcept
			{
				struct closer_t final {
					void operator()( const mchain_t & ch ) const noexcept {
						// Close the reply chain.
						// If there is no reply but someone is waiting
						// on that chain it will be awakened.
						close_retain_content( ch );
					}
					void operator()( const mbox_t & ) const noexcept {
						// Nothing to do.
					}
				};

				std::visit( closer_t{}, m_target );
			}

		reply_target_holder_t( const reply_target_holder_t & ) = delete;
		reply_target_holder_t( reply_target_holder_t && ) = delete;

		//! Getter.
		const auto &
		target() const noexcept { return m_target; }
	};

//
// query_actual_reply_target
//
/*!
 * \brief Helper function for extraction of actual reply target
 * from reply_target instance.
 *
 * \note
 * Reply target is returned as mbox_t.
 */
inline mbox_t
query_actual_reply_target( const reply_target_t & rt ) noexcept
	{
		struct extractor_t {
			mbox_t operator()( const mchain_t & ch ) const noexcept {
				return ch->as_mbox();
			}
			mbox_t operator()( const mbox_t & mbox ) const noexcept {
				return mbox;
			}
		};

		return std::visit( extractor_t{}, rt );
	}

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
		//! An actual type of request.
		using request_t = ensure_no_mutability_modificators_t<Request>;
		//! An actual type of reply.
		using reply_t = ensure_no_mutability_modificators_t<Reply>;

	protected :
		//! Is reply moveable type?
		static constexpr const bool is_reply_moveable =
				std::is_move_assignable_v<reply_t> &&
						std::is_move_constructible_v<reply_t>;

		//! Is reply copyable type?
		static constexpr const bool is_reply_copyable =
				std::is_copy_assignable_v<reply_t> &&
						std::is_copy_constructible_v<reply_t>;

		static_assert( is_reply_moveable || is_reply_copyable,
				"Reply type should be MoveAssignable or CopyAssignable" );

		//! The target for the reply.
		reply_target_holder_t m_reply_target;
		//! The flag for detection of repeated replies.
		/*!
		 * Recives `true` when the first reply is sent.
		 */
		bool m_reply_sent{ false };

		//! Initializing constructor.
		basic_request_reply_part_t( reply_target_t reply_target ) noexcept
			:	m_reply_target{ std::move(reply_target) }
			{}

	public :
		~basic_request_reply_part_t() override = default;
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
		//! An actual request object.
		typename Base::request_t m_request;

		//! Initializing constructor.
		template< typename... Args >
		request_holder_part_t(
			reply_target_t reply_target,
			Args && ...args )
			:	Base{ std::move(reply_target) }
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
// close_reply_chain_flag_t
//
/*!
 * \brief A flag to specify should the reply chain be closed automatically.
 */
enum class close_reply_chain_flag_t
	{
		//! The reply chain should be automatically closed when
		//! the corresponding request_reply_t instance is being destroyed.
		close,
		//! The reply chain shouldn't be closed even if the corresponding
		//! request_reply_t instance is destroyed.
		//! A user should close the reply chain manually.
		do_not_close
	};

//! The indicator that the reply chain should be closed automatically.
/*!
 * If this flag is used then the reply chain will be automatically closed when
 * the corresponding request_reply_t instance is being destroyed.
 *
 * Usage example:
 * \code
 * using my_ask_reply = so_5::extra::sync::request_reply_t<my_request, my_reply>;
 * // Create the reply chain manually.
 * auto reply_ch = create_mchain(env);
 * // Issue a request.
 * my_ask_reply::initiate_with_custom_reply_to(
 * 	target,
 * 	reply_ch,
 * 	so_5::extra::sync::close_reply_chain,
 * 	...);
 * ... // Do something.
 * // Now we can read the reply.
 * // The reply chain will be automatically closed after dispatching of the request.
 * receive(from(reply_ch).handle_n(1),
 * 	[](typename my_ask_reply::reply_mhood_t cmd) {...});
 * \endcode
 */
constexpr const close_reply_chain_flag_t close_reply_chain =
		close_reply_chain_flag_t::close;

//! The indicator that the reply chain shouldn't be closed automatically.
/*!
 * If this flag is used then the reply chain won't be automatically closed when
 * the corresponding request_reply_t instance is being destroyed. It means that
 * one reply chain can be used for receiving of different replies:
 * \code
 * using one_ask_reply = so_5::extra::sync::request_reply_t<one_request, one_reply>;
 * using another_ask_reply = so_5::extra::sync::request_reply_t<another_request, another_reply>;
 *
 * // Create the reply chain manually.
 * auto reply_ch = create_mchain(env);
 * // Issue the first request.
 * one_ask_reply::initiate_with_custom_reply_to(
 * 	one_target,
 * 	reply_ch,
 * 	so_5::extra::sync::do_not_close_reply_chain,
 * 	...);
 * // Issue the second request.
 * another_ask_reply::initiate_with_custom_reply_to(
 * 	another_target,
 * 	reply_ch,
 * 	so_5::extra::sync::do_not_close_reply_chain,
 * 	...);
 * ... // Do something.
 * // Now we can read the replies.
 * receive(from(reply_ch).handle_n(2),
 * 	[](typename one_ask_reply::reply_mhood_t cmd) {...},
 * 	[](typename another_ask_reply::reply_mhood_t cmd) {...});
 * \endcode
 */
constexpr const close_reply_chain_flag_t do_not_close_reply_chain =
		close_reply_chain_flag_t::do_not_close;

//
// request_reply_t
//
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
			sync_ns::request_mhood_t<my_request, my_reply> cmd) {
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
my_reply reply = sync_ns::request_reply<my_request, my_reply>(
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
optional<my_reply> opt_reply = sync_ns::request_opt_reply<my_request, my_reply>(
	svc_mbox,
	15s,
	4242,
	"Request #2");
\endcode

When a user calls request_reply() or request_opt_reply() helper function an
instance of request_reply_t class is created and sent to the destination.
This instance is sent as a mutable message, because of that it should be
received via mutable_mhood_t. Usage of so_5::extra::sync::request_mhood_t
template is the most convenient way to receive instances of request_reply_t
class.

If Request is not a type of a signal then request_reply_t holds an instance
of that type inside. This instance is accessible via request_reply_t::request()
getter:

\code
struct my_request {
	int a_;
	std::string b_;
};

struct my_reply {...};

namespace sync_ns = so_5::extra::sync;

// The agent that processes requests.
class service final : public so_5::agent_t {
	...
	void on_request(
			sync_ns::request_mhood_t<my_request, my_reply> cmd) {
		std::cout << "request received: " << cmd->request().a_
				<< ", " << cmd->request().b_ << std::endl;
		...
	}
};
\endcode

If Request is a type of a signal then there is no request_reply_t::request()
getter.

To make a reply it is necessary to call request_reply_t::make_reply() method:

\code
void some_agent::on_request(so_5::extra::sync::request_mhood_t<Request, Reply> cmd) {
	...;
	cmd->make_reply(
		// This arguments will be passed to Reply's constructor.
		... );
}
\endcode

\note
The make_reply() method can be called only once. An attempt to do the second
call to make_reply() will lead to raising exception of type
so_5::exception_t.

Please note that repeating of Request and Reply types, again and again, is not
a good idea. For example, the following code can be hard to maintain:
\code
void some_agent::on_some_request(
		sync_ns::request_mhood_t<some_request, some_reply> cmd)
{...}

void some_agent::on_another_request(
		sync_ns::request_mhood_t<another_request, another_reply> cmd)
{...}

...
auto some_result = sync_ns::request_reply<some_request, some_reply>(...);
...
auto another_result = sync_ns::request_reply<another_request, another_reply>(...);
\endcode

It is better to define a separate name for Request and Reply pair and
use this name:
\code
using some_request_reply = sync_ns::request_reply_t<some_request, some_reply>;
using another_request_reply = sync_ns::request_reply_t<another_request, another_reply>;

void some_agent::on_some_request(
		typename some_request_reply::request_mhood_t cmd)
{...}

void some_agent::on_another_request(
		typename another_request_reply::request_mhood_t cmd)
{...}

...
auto some_result = some_request_reply::ask_value(...);
...
auto another_result = another_request_reply::ask_value(...);
\endcode

\attention The request_reply_t is not thread safe class.

\attention Message mutability indicators like so_5::mutable_msg and
so_5::immutable_msg can't be used for Request and Reply parameters. It means
that the following code will lead to compilation errors:
\code
// NOTE: so_5::immutable_msg can't be used here!
auto v1 = so_5::extra::sync::request_value<so_5::immutable_msg<Q>, A>(...);

// NOTE: so_5::mutable_msg can't be used here!
auto v2 = so_5::extra::sync::request_value<Q, so_5::mutable_msg<A>>(...);
\endcode

\par A custom destination for the reply message

By default the reply message is sent to a separate mchain created inside
request_value() and request_opt_value() functions (strictly speacking
this mchain is created inside request_reply_t::initiate() function and
then used by request_value() and request_opt_value() functions).

But sometimes it can be necessary to have an ability to specify a custom
destination for the reply message. It can be done via
request_reply_t::initiate_with_custom_reply_to() methods. For example,
the following code sends two requests to different targets but all replies
are going to the same mchain:
\code
using first_ask_reply = so_5::extra::sync::request_reply_t<first_request, first_reply>;
using second_ask_reply = so_5::extra::sync::request_reply_t<second_request, second_reply>;

auto reply_ch = create_mchain(env);

// Sending of requests.
first_ask_reply::initiate_with_custom_reply_to(
		first_target,
		// do_not_close_reply_chain is mandatory in that scenario.
		reply_ch, so_5::extra::sync::do_not_close_reply_chain,
		... );
second_ask_reply::initiate_with_custom_reply_to(
		second_target,
		// do_not_close_reply_chain is mandatory in that scenario.
		reply_ch, so_5::extra::sync::do_not_close_reply_chain,
		... );

... // Do something.

// Now we want to receive and handle replies.
receive(from(reply_ch).handle_n(2).empty_timeout(15s),
		[](typename first_ask_reply::reply_mhood_t cmd) {...},
		[](typename second_ask_reply::reply_mhood_t cmd) {...});
\endcode

\tparam Request a type of a request. It can be type of a signal.

\tparam Reply a type of a reply. This type should be
MoveAssignable+MoveConstructible or CopyAssignable+CopyConstructible.

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
		//! An actual type of request.
		using request_t = typename base_type::request_t;
		//! An actual type of reply.
		using reply_t = typename base_type::reply_t;

		//! A shorthand for mhood for receiving request object.
		/*!
		 * An instance of requst is sent as mutabile message of
		 * type request_reply_t<Q,A>. It means that this message
		 * can be received if a request handler has the following
		 * prototype:
		 * \code
		 * void handler(so_5::mhood_t<so_5::mutable_msg<so_5::extra::sync::request_reply_t<Q,A>>>);
		 * \endcode
		 * or:
		 * \code
		 * void handler(so_5::mutable_mhood_t<so_5::extra::sync::request_reply_t<Q,A>>);
		 * \endcode
		 * But it is better to use request_mhood_t typedef:
		 * \code
		 * void handler(typename so_5::extra::sync::request_reply_t<Q,A>::request_mhood_t);
		 * \endcode
		 * And yet better to use request_mhood_t typedef that way:
		 * \code
		 * using my_request_reply = so_5::extra::sync::request_reply_t<Q,A>;
		 * void handler(typename my_request_reply::request_mhood_t);
		 * \endcode
		 */
		using request_mhood_t = mutable_mhood_t< request_reply_t >;

		//! A shorthand for mhood for receiving reply object.
		/*!
		 * An instance of requst is sent as mutabile message of
		 * type reply_t. It means that this message
		 * can be received if a message handler has the following
		 * prototype:
		 * \code
		 * void handler(so_5::mhood_t<so_5::mutable_msg<typename so_5::extra::sync::request_reply_t<Q,A>::reply_t>>);
		 * \endcode
		 * or:
		 * \code
		 * void handler(so_5::mutable_mhood_t<typename so_5::extra::sync::request_reply_t<Q,A>::reply_t>);
		 * \endcode
		 * But it is better to use reply_mhood_t typedef:
		 * \code
		 * void handler(typename so_5::extra::sync::request_reply_t<Q,A>::reply_mhood_t);
		 * \endcode
		 * And yet better to use reply_mhood_t typedef that way:
		 * \code
		 * using my_request_reply = so_5::extra::sync::request_reply_t<Q,A>;
		 * void handler(typename my_request_reply::reply_mhood_t);
		 * \endcode
		 *
		 * \note
		 * Usage of that typedef can be necessary only if you implement
		 * you own handling of reply-message from the reply chain.
		 */
		using reply_mhood_t = mutable_mhood_t< reply_t >;

		//! A shorthand for message_holder that can hold request_reply instance.
		/*!
		 * Usage example:
		 * \code
		 * using my_request_reply = so_5::extra::sync::request_reply_t<my_request, my_reply>;
		 *
		 * class requests_collector final : public so_5::agent_t {
		 * 	// Container for holding received requests.
		 * 	std::vector<typename my_request_reply::holder_t> requests_;
		 *
		 * 	...
		 * 	void on_request(typename my_request_reply::request_mhood_t cmd) {
		 * 		// Store the request to process it later.
		 * 		requests_.push_back(cmd.make_holder());
		 * 	}
		 * };
		 * \endcode
		 */
		using holder_t = message_holder_t< mutable_msg<request_reply_t> >;

	private :
		using base_type::base_type;

		using base_type::is_reply_moveable;
		using base_type::is_reply_copyable;

		using msg_holder_t = message_holder_t< mutable_msg< request_reply_t > >;

		//! Helper method for getting the result value from reply_mhood
		//! with respect to moveability of reply object.
		template< typename Reply_Receiver >
		static void
		borrow_from_reply_mhood(
			reply_mhood_t & cmd,
			Reply_Receiver & result )
			{
				if constexpr( is_reply_moveable )
					result = std::move(*cmd);
				else
					result = *cmd;
			}

		//! An actual implementation of ask_value for the case when
		//! reply object is DefaultConstructible.
		template<typename Target, typename Duration, typename... Args>
		[[nodiscard]]
		static auto
		ask_default_constructible_value(
			Target && target,
			Duration duration,
			Args && ...args )
			{
				// Because reply object is default constructible we can create it
				// on the stack. And this can be the subject of NRVO.
				reply_t result;

				auto reply_ch = initiate(
						std::forward<Target>(target),
						std::forward<Args>(args)... );

				bool result_received{false};
				receive(
						from(reply_ch).handle_n(1).empty_timeout(duration),
						[&result, &result_received]( reply_mhood_t cmd ) {
							result_received = true;
							borrow_from_reply_mhood( cmd, result );
						} );

				if( !result_received )
					{
						SO_5_THROW_EXCEPTION( errors::rc_no_reply,
								std::string{ "no reply received, request_reply type: " } +
								typeid(request_reply_t).name() );
					}

				return result;
			}

		//! An actual implementation of request_value for the case when
		//! reply object is not DefaultConstructible.
		template<typename Target, typename Duration, typename... Args>
		[[nodiscard]]
		static auto
		ask_not_default_constructible_value(
			Target && target,
			Duration duration,
			Args && ...args )
			{
				// For the case when Reply is not default constructible we
				// will use request_opt_value that can handle then case.
				auto result = ask_opt_value(
						std::forward<Target>(target),
						duration,
						std::forward<Args>(args)... );

				if( !result )
					{
						SO_5_THROW_EXCEPTION( errors::rc_no_reply,
								std::string{ "no reply received, request_reply type: " } +
								typeid(request_reply_t).name() );
					}

				return *result;
			}

	public :
		/*!
		 * \brief Initiate a request by sending request_reply_t message instance.
		 *
		 * This method creates a mchain for reply, then instantiates and
		 * sends an instance of request_reply_t<Request, Reply> type.
		 *
		 * Returns the reply mchain.
		 *
		 * Note that this method is used in implementation of ask_value()
		 * and ask_opt_value() methods. Call it only if you want to receive
		 * reply message by itself. For example usage of initiate() can
		 * be useful in a such case:
		 * \code
		 * // Issue the first request.
		 * auto ch1 = so_5::extra::sync::request_reply_t<Ask1, Reply1>::initiate(target1, ..);
		 * // Issue the second request.
		 * auto ch2 = so_5::extra::sync::request_reply_t<Ask2, Reply2>::initiate(target2, ...);
		 * // Issue the third request.
		 * auto ch3 = so_5::extra::sync::request_reply_t<Ask3, Reply3>::initiate(target3, ...);
		 * // Wait and handle requests in any order of appearance.
		 * so_5::select( so_5::from_all().handle_all(),
		 * 	case_(ch1, [](so_5::extra::sync::reply_mhood_t<Ask1, Reply1> cmd) {...}),
		 * 	case_(ch2, [](so_5::extra::sync::reply_mhood_t<Ask2, Reply2> cmd) {...}),
		 * 	case_(ch3, [](so_5::extra::sync::reply_mhood_t<Ask3, Reply3> cmd) {...}));
		 * \endcode
		 */
		template< typename Target, typename... Args >
		[[nodiscard]]
		static mchain_t
		initiate( const Target & target, Args && ...args )
			{
				// Only one message should be stored in reply_ch.
				auto mchain = so_5::extra::mchains::fixed_size::create_mchain<1>(
						so_5::send_functions_details::arg_to_env( target ),
						so_5::mchain_props::overflow_reaction_t::throw_exception );

				msg_holder_t msg{
					// Calling 'new' directly because request_reply_t has
					// private constructor.
					new request_reply_t{ mchain, std::forward<Args>(args)... }
				};

				send( target, std::move(msg) );

				return mchain;
			}

		/*!
		 * \brief Initiate a request by sending request_reply_t message instance
		 * with sending the reply to the specified mbox.
		 *
		 * A new instance of request_reply_t message is created and sent to
		 * \a target. The reply will be sent to \a reply_to mbox.
		 *
		 * Usage example:
		 * \code
		 * using my_ask_reply = so_5::extra::sync::request_reply_t<my_request, my_reply>;
		 * class consumer final : public so_5::agent_t {
		 * 	void on_some_event(mhood_t<some_event> cmd) {
		 * 		// Prepare for issuing a request.
		 * 		// New mbox for receiving the reply.
		 * 		auto reply_mbox = so_make_new_direct_mbox();
		 * 		// Make subscription for handling the reply.
		 * 		so_subscribe(reply_mbox).event(
		 * 			[this](typename my_ask_reply::reply_mhood_t cmd) {
		 * 				... // Some handling.
		 * 			});
		 * 		// Issuing a request with redirection of the reply to
		 * 		// the new reply mbox.
		 * 		my_ask_reply::initiate_with_custom_reply_to(target, reply_mbox, ...);
		 * 	}
		 * };
		 * \endcode
		 */
		template< typename Target, typename... Args >
		static void
		initiate_with_custom_reply_to(
			const Target & target,
			const mbox_t & reply_to,
			Args && ...args )
			{
				msg_holder_t msg{
					// Calling 'new' directly because request_reply_t has
					// private constructor.
					new request_reply_t{ reply_to, std::forward<Args>(args)... }
				};

				send( target, std::move(msg) );
			}

		/*!
		 * \brief Initiate a request by sending request_reply_t message instance
		 * with sending the reply to the direct mbox of the specified agent.
		 *
		 * A new instance of request_reply_t message is created and sent to
		 * \a target. The reply will be sent to the direct mbox \a reply_to agent.
		 *
		 * Usage example:
		 * \code
		 * using my_ask_reply = so_5::extra::sync::request_reply_t<my_request, my_reply>;
		 * class consumer final : public so_5::agent_t {
		 * 	void on_some_event(mhood_t<some_event> cmd) {
		 * 		// Prepare for issuing a request.
		 * 		// Make subscription for handling the reply.
		 * 		so_subscribe_self().event(
		 * 			[this](typename my_ask_reply::reply_mhood_t cmd) {
		 * 				... // Some handling.
		 * 			});
		 * 		// Issuing a request with redirection of the reply to
		 * 		// the direct mbox of the agent.
		 * 		my_ask_reply::initiate_with_custom_reply_to(target, *this, ...);
		 * 	}
		 * };
		 * \endcode
		 */
		template< typename Target, typename... Args >
		static void
		initiate_with_custom_reply_to(
			const Target & target,
			const agent_t & reply_to,
			Args && ...args )
			{
				msg_holder_t msg{
					// Calling 'new' directly because request_reply_t has
					// private constructor.
					new request_reply_t{
							reply_to.so_direct_mbox(), std::forward<Args>(args)... }
				};

				send( target, std::move(msg) );
			}

		/*!
		 * \brief Initiate a request by sending request_reply_t message instance
		 * with sending the reply to the specified mchain.
		 *
		 * A new instance of request_reply_t message is created and sent to
		 * \a target. The reply will be sent to \a reply_ch mchain.
		 *
		 * Argument \a close_flag specifies what should be done with \a reply_ch
		 * after destroying of request_reply_t message instance:
		 * - if \a close_flag is so_5::extra::sync::close_reply_chain then \a
		 *   reply_ch will be automatically closed;
		 * - if \a close_flag is so_5::extra::sync::do_not_close_reply_chain then
		 *   \a reply_ch won't be closed. In that case \a reply_ch mchain can be
		 *   used for handling replies from several requests. See description of
		 *   so_5::extra::sync::do_not_close_reply_chain for an example.
		 */
		template< typename Target, typename... Args >
		static void
		initiate_with_custom_reply_to(
			const Target & target,
			const mchain_t & reply_ch,
			close_reply_chain_flag_t close_flag,
			Args && ...args )
			{
				msg_holder_t msg;

				switch( close_flag )
					{
					case close_reply_chain:
						msg = msg_holder_t{ new request_reply_t{
								reply_ch,
								std::forward<Args>(args)... } };
					break;

					case do_not_close_reply_chain:
						msg = msg_holder_t{ new request_reply_t{
								reply_ch->as_mbox(),
								std::forward<Args>(args)... } };
					break;
					}

				send( target, std::move(msg) );
			}

		/*!
		 * \brief Make the reply and send it back.
		 *
		 * Instantiates an instance of type Reply and send it back to
		 * the reply chain.
		 *
		 * \attention This method should be called at most once.
		 * An attempt to call it twice will lead to an exception.
		 *
		 * Usage example:
		 * \code
		 * void some_agent::on_request(
		 * 	so_5::extra::sync::request_reply_t<Request, Reply> cmd)
		 * {
		 * 	... // Some processing of the request.
		 * 	// Sending the reply back.
		 * 	cmd->make_reply(...);
		 * }
		 * \endcode
		 *
		 * \tparam Args types of arguments for Reply's constructor.
		 */
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
						details::query_actual_reply_target(
								this->m_reply_target.target() ),
						std::forward<Args>(args)... );

				this->m_reply_sent = true;
			}

		/*!
		 * \brief Send a request and wait for the reply.
		 *
		 * This method instantiates request_reply_t object using 
		 * \a args arguments for initializing instance of Request type.
		 * Then the request_reply_t is sent as a mutable message to
		 * \a target. Then this method waits the reply for no more
		 * than \a duration. If there is no reply then an empty
		 * std::optional is returned.
		 *
		 * Returns an instance of std::optional<Reply>.
		 *
		 * Usage example:
		 * \code
		 * struct check_user_request final {...};
		 * struct check_user_result final {...};
		 * using check_user = so_5::extra::sync::request_reply_t<check_user_request, check_user_result>;
		 * ...
		 * std::optional<check_user_result> result = check_user::ask_opt_value(
		 * 		target,
		 * 		10s,
		 * 		... );
		 * if(result) {
		 * 	... // Some handling of *result.
		 * }
		 * \endcode
		 *
		 * \note
		 * If Request is a type of a signal then \a args should be an empty
		 * sequence:
		 * \code
		 * struct statistics_request final : public so_5::signal_t {};
		 * class current_stats final {...};
		 *
		 * std::optional<current_stats> stats =
		 * 	so_5::extra::sync::request_reply_t<statistics_request, current_stats>::ask_opt_value(target, 10s);
		 * \endcode
		 */
		template<typename Target, typename Duration, typename... Args>
		[[nodiscard]]
		static auto
		ask_opt_value(
			Target && target,
			Duration duration,
			Args && ...args )
			{
				auto reply_ch = initiate(
						std::forward<Target>(target),
						std::forward<Args>(args)... );

				optional<reply_t> result;
				receive(
						from(reply_ch).handle_n(1).empty_timeout(duration),
						[&result]( reply_mhood_t cmd ) {
							borrow_from_reply_mhood( cmd, result );
						} );

				return result;
			}

		/*!
		 * \brief Send a request and wait for the reply.
		 *
		 * This method instantiates request_reply_t object using 
		 * \a args arguments for initializing instance of Request type.
		 * Then the request_reply_t is sent as a mutable message to
		 * \a target. Then this method waits the reply for no more
		 * than \a duration. If there is no reply then an exception
		 * of type so_5::exception_t is thrown.
		 *
		 * Returns an instance of Reply.
		 *
		 * Usage example:
		 * \code
		 * struct check_user_request final {...};
		 * struct check_user_result final {...};
		 * using check_user = so_5::extra::sync::request_reply_t<check_user_request, check_user_result>;
		 * ...
		 * check_user_result result = check_user::ask_value(
		 * 		target,
		 * 		10s,
		 * 		... );
		 * \endcode
		 *
		 * \note
		 * If Request is a type of a signal then \a args should be an empty
		 * sequence:
		 * \code
		 * struct statistics_request final : public so_5::signal_t {};
		 * class current_stats final {...};
		 *
		 * std::optional<current_stats> stats =
		 * 	so_5::extra::sync::request_reply_t<statistics_request, current_stats>::ask_value(target, 10s);
		 * \endcode
		 */
		template<typename Target, typename Duration, typename... Args>
		[[nodiscard]]
		static auto
		ask_value(
			Target && target,
			Duration duration,
			Args && ...args )
			{
				if constexpr( std::is_default_constructible_v<reply_t> )
					return ask_default_constructible_value(
							std::forward<Target>(target),
							duration,
							std::forward<Args>(args)... );
				else
					return ask_not_default_constructible_value(
							std::forward<Target>(target),
							duration,
							std::forward<Args>(args)... );
			}
	};

//
// request_mhood_t
//
/*!
 * \brief A short form of request_reply_t<Q,A>::request_mhood_t.
 *
 * Usage example:
 * \code
 * namespace sync_ns = so_5::extra::sync;
 * class service final : public so_5::agent_t {
 * 	...
 * 	void on_request(sync_ns::request_mhood_t<my_request, my_reply> cmd) {
 * 		...
 * 		cmd->make_reply(...);
 * 	}
 * };
 * \endcode
 */
template< typename Request, typename Reply >
using request_mhood_t = typename request_reply_t<Request, Reply>::request_mhood_t;

//
// reply_mhood_t
//
/*!
 * \brief A short form of request_reply_t<Q,A>::reply_mhood_t.
 */
template< typename Request, typename Reply >
using reply_mhood_t = typename request_reply_t<Request, Reply>::reply_mhood_t;

//
// request_reply
//
/*!
 * \brief A helper function for performing request_reply-iteraction.
 *
 * Sends a so_5::extra::sync::request_reply_t <Request,Reply> to the specified
 * \a target and waits the reply for no more that \a duration. If there is no
 * reply then an exception will be thrown.
 *
 * Usage example:
 * \code
 * auto r = so_5::extra::sync::request_reply<my_request, my_reply>(
 * 		some_mchain,
 * 		10s,
 * 		...);
 * \endcode
 *
 * Returns an instance of Reply object.
 */
template<
	typename Request, typename Reply,
	typename Target, typename Duration, typename... Args>
[[nodiscard]]
auto
request_reply(
	Target && target,
	Duration duration,
	Args && ...args )
	{
		return request_reply_t<Request, Reply>::ask_value(
				std::forward<Target>(target),
				duration,
				std::forward<Args>(args)... );
	}

//
// request_opt_reply
//
/*!
 * \brief A helper function for performing request_reply-iteraction.
 *
 * Sends a so_5::extra::sync::request_reply_t <Request,Reply> to the specified
 * \a target and waits the reply for no more that \a duration. If there is no
 * reply then an empty optional object will be returned.
 *
 * Usage example:
 * \code
 * auto r = so_5::extra::sync::request_opt_reply<my_request, my_reply>(
 * 		some_mchain,
 * 		10s,
 * 		...);
 * if(r) {
 * 	... // Do something with *r.
 * }
 * \endcode
 *
 * Returns an instance of std::optional<Reply> object.
 */
template<
	typename Request, typename Reply,
	typename Target, typename Duration, typename... Args>
[[nodiscard]]
auto
request_opt_reply(
	Target && target,
	Duration duration,
	Args && ...args )
	{
		return request_reply_t<Request, Reply>::ask_opt_value(
				std::forward<Target>(target),
				duration,
				std::forward<Args>(args)... );
	}

} /* namespace sync */

} /* namespace extra */

} /* namespace so_5 */

