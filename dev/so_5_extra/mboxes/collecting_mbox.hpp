/*!
 * \file
 * \brief Implementation of collecting mbox.
 *
 * \since
 * v.1.0.1
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/impl/msg_tracing_helpers.hpp>

#include <so_5/details/sync_helpers.hpp>

#include <so_5/mbox.hpp>
#include <so_5/enveloped_msg.hpp>

#include <so_5/optional.hpp>

#include <memory>
#include <tuple>
#include <utility>

namespace so_5 {

namespace extra {

namespace mboxes {

namespace collecting_mbox {

namespace errors {

/*!
 * \brief An attempt to make subscription to collecting_mbox.
 *
 * \since
 * v.1.0.1
 */
const int rc_subscribe_event_handler_be_used_on_collecting_mbox =
		so_5::extra::errors::collecting_mbox_errors;

/*!
 * \brief An attempt to set delivery filter to collecting_mbox.
 *
 * \since
 * v.1.0.1
 */
const int rc_delivery_filter_cannot_be_used_on_collecting_mbox =
		so_5::extra::errors::collecting_mbox_errors + 1;

/*!
 * \brief An attempt to send a message or signal of different type.
 *
 * \since
 * v.1.0.1
 */
const int rc_different_message_type =
		so_5::extra::errors::collecting_mbox_errors + 2;

} /* namespace errors */

namespace details {

/*!
 * \brief A helper type which is a collection of type parameters.
 *
 * This type is used to simplify code of collecting_mbox internals.
 * Instead of writting something like:
 * \code
 * template< typename Collecting_Msg, typename Traits >
 * class ... {...};
 *
 * template< typename Collecting_Msg, typename Traits, typename Lock_Type >
 * class ... {...};
 * \endcode
 * this config_type allows to write like that:
 * \code
 * template< typename Config_Type >
 * class ... {...};
 *
 * template< typename Config_Type >
 * class ... {...};
 * \endcode
 *
 * \tparam Collecting_Msg type of collecting messages or signals. Note: if
 * mutable messages is collecting then it should be so_5::mutable_msg<M>.
 *
 * \tparam Traits type of size-dependent traits (like
 * so_5::extra::mboxes::collecting_mbox::constexpr_size_traits_t or
 * so_5::extra::mboxes::collecting_mbox::runtime_size_traits_t).
 *
 * \tparam Lock_Type type of object to be used for thread-safety (like
 * std::mutex or so_5::null_mutex_t).
 */
template<
	typename Collecting_Msg,
	typename Traits,
	typename Lock_Type >
struct config_type
	{
		using collecting_msg_type = Collecting_Msg;
		using traits_type = Traits;
		using lock_type = Lock_Type;
	};

/*!
 * \name Type extractors for config_type
 * \{
 */
template< typename Config_Type >
using collecting_msg_t = typename Config_Type::collecting_msg_type;

template< typename Config_Type >
using traits_t = typename Config_Type::traits_type;

template< typename Config_Type >
using lock_t = typename Config_Type::lock_type;
/*!
 * \}
 */

/*!
 * \brief Helper method for checking message mutability and type of
 * the target mbox.
 *
 * \throw so_5::exception_t if message is mutable but \a target is not
 * MPSC-mbox.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
void
check_mutability_validity_for_target_mbox(
	//! A target mbox for messages_collected message.
	const so_5::mbox_t & target )
	{
		if( is_mutable_message< collecting_msg_t<Config_Type> >::value &&
				mbox_type_t::multi_producer_single_consumer != target->type() )
			SO_5_THROW_EXCEPTION(
					::so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox,
					"a target for collecting_mbox must be MPSC mbox in case "
					"of a mutable messge" );
	}

//
// collected_messages_bunch_t
//
/*!
 * \brief Type of message to be sent when all collecting messages are received.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
class collected_messages_bunch_t final
	: public so_5::message_t
	, traits_t<Config_Type>::messages_collected_mixin_type
	{
		template<typename> friend class collected_messages_bunch_builder_t;

		using mixin_base_type =
				typename traits_t<Config_Type>::messages_collected_mixin_type;

		//! A container for collected messages.
		typename traits_t<Config_Type>::container_type m_collected_messages;

		//! Store another collected message at the specified index.
		void
		store_collected_messages(
			//! Index at which message should be stored.
			size_t index,
			//! Message to be stored.
			message_ref_t msg )
			{
				this->storage()[ index ] = std::move(msg);
			}

		//! Initializing constructor.
		collected_messages_bunch_t( std::size_t size )
			:	mixin_base_type( size )
			{}

	public :
		using mixin_base_type::size;

		//! Do some action with Nth collected message.
		/*!
		 * \note This method can be used for immutable and for mutable messages.
		 *
		 * \attention
		 * \a index should be less than size(). Value of \a index is not
		 * checked at the run-time.
		 *
		 * \return value of f(mhood_t<Config_Type::collecting_msg_t>(...)).
		 *
		 * \tparam F type of functor/lambda which accepts mhood_t.
		 *
		 * Usage example:
		 * \code
		 * struct my_msg final : public so_5::message_t {
		 * 	std::string value_;
		 * 	...
		 * };
		 * using my_msg_collector = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
		 * 	my_msg, so_5::extra::mboxes::collecting_msg::runtime_size_traits_t >;
		 * ...
		 * void my_actor::on_my_msg_collected(mhood_t<typename my_msg_collector::messages_collected_t> cmd) {
		 * 	std::string v = cmd->with_nth( 0, [](auto m) { return m->value_; } );
		 * }
		 * \endcode
		 */
		template< typename F >
		decltype(auto)
		with_nth(
			std::size_t index,
			F && f ) const
			{
				message_ref_t ref{ this->storage()[ index ] };
				return f(mhood_t< collecting_msg_t<Config_Type> >{ref});
			}

		//! Do some action for all collected message.
		/*!
		 * \note This method can be used for immutable and for mutable messages.
		 *
		 * \return value of f(mhood_t<Config_Type::collecting_msg_t>(...)).
		 *
		 * \tparam F type of functor/lambda which accepts mhood_t.
		 *
		 * Usage example:
		 * \code
		 * struct my_msg final : public so_5::message_t {
		 * 	std::string value_;
		 * 	...
		 * };
		 * using my_msg_collector = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
		 * 	my_msg, so_5::extra::mboxes::collecting_msg::runtime_size_traits_t >;
		 * ...
		 * void my_actor::on_my_msg_collected(mhood_t<typename my_msg_collector::messages_collected_t> cmd) {
		 * 	cmd->for_all( [](auto m) { std::cout << m->value_; } );
		 * }
		 * \endcode
		 */
		template< typename F >
		void
		for_each( F && f ) const
			{
				for( message_ref_t ref : this->storage() )
					f(mhood_t< collecting_msg_t<Config_Type> >{ref});
			}

		//! Do some action for all collected message.
		/*!
		 * \note This method can be used for immutable and for mutable messages.
		 *
		 * \return value of f(index, mhood_t<Config_Type::collecting_msg_t>(...)).
		 *
		 * \tparam F type of functor/lambda which accepts two parameters:
		 * \a index of std::size_t and \a cmd of mhood_t.
		 *
		 * Usage example:
		 * \code
		 * struct my_msg final : public so_5::message_t {
		 * 	std::string value_;
		 * 	...
		 * };
		 * using my_msg_collector = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
		 * 	my_msg, so_5::extra::mboxes::collecting_msg::runtime_size_traits_t >;
		 * ...
		 * void my_actor::on_my_msg_collected(mhood_t<typename my_msg_collector::messages_collected_t> cmd) {
		 * 	cmd->for_all_with_index( [](auto i, auto m) { std::cout << i << ":" m->value_; } );
		 * }
		 * \endcode
		 */
		template< typename F >
		void
		for_each_with_index( F && f ) const
			{
				const auto total = this->size();
				for( std::size_t index = 0; index < total; ++index )
					{
						message_ref_t ref{ this->storage()[ index ] };
						f(index, mhood_t< collecting_msg_t<Config_Type> >{ref});
					}
			}
	};

//
// detect_message_to_store
//
/*!
 * \brief Detect the actual message to be collected (if it is present).
 *
 * SO-5.5.23 introduced enveloped messages. In the case of enveloped message
 * the payload must be extrected and stored inside collected_mbox.
 * This function checks the kind of a message and extract payload if
 * message is an enveloped.
 *
 * Original value of \a what is returned in \a what is not an envelope.
 *
 * \since
 * v.1.2.0
 */
inline optional< message_ref_t >
detect_message_to_store( message_ref_t what )
	{
		if( message_t::kind_t::enveloped_msg == message_kind(what) )
			{
				// Envelope's payload must be extracted.
				auto opt_payload_info = ::so_5::enveloped_msg::
						extract_payload_for_message_transformation( what );
				if( opt_payload_info )
					return { opt_payload_info->message() };
				else
					return {};
			}
		else
			return { std::move(what) };
	}

//
// collected_messages_bunch_builder_t
//
/*!
 * \brief A builder for case when collecting_mbox collects messages.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
class collected_messages_bunch_builder_t
	{
	public :
		//! Actual message type to be used.
		using message_type = collected_messages_bunch_t< Config_Type >;

	private :
		//! The current instance of messages_collected to store 
		//! messages to be delivered.
		/*!
		 * Can be nullptr if there is no new messages.
		 */
		std::unique_ptr< message_type > m_current_msg;

		//! Count of collected messages.
		/*!
		 * If m_collected_messages != 0 then m_current_msg must not be nullptr.
		 */
		std::size_t m_collected_messages = 0;

	public :
		//! Store another instance of collecting messages.
		void
		store(
			//! Message to be stored.
			message_ref_t message,
			//! Total count of message to be collected.
			//! This parameter is necessary because a new instance of
			//! messages_collected can be created inside this method.
			std::size_t messages_to_collect )
			{
				// Since SO-5.5.23 it is necessary to check a type of message.
				// If it is an envelope then the content of the envelope should
				// be extracted.
				auto opt_msg_to_store = detect_message_to_store(
						std::move(message) );
				// There can be a case when payload is missing.
				// In that case nothing will be stored.
				if( opt_msg_to_store )
					{
						message_type * storage = m_current_msg.get();
						if( !storage )
							{
								m_current_msg.reset(
										new message_type( messages_to_collect ) );
								storage = m_current_msg.get();
							}

						storage->store_collected_messages(
								m_collected_messages,
								std::move( *opt_msg_to_store ) );
						++m_collected_messages;
					}
			}

		bool
		is_ready_to_be_sent( std::size_t messages_to_collect ) const noexcept
			{
				return m_collected_messages >= messages_to_collect;
			}

		std::unique_ptr< message_type >
		extract_message()
			{
				m_collected_messages = 0u;
				return std::move( m_current_msg );
			}
	};

//
// collected_signals_bunch_t
//
/*!
 * \brief A type of message to be sent when all collected signals are received.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
class collected_signals_bunch_t final
	: public so_5::message_t
	, traits_t<Config_Type>::signals_collected_mixin_type
	{
		template<typename> friend class collected_signals_bunch_builder_t;

		using mixin_base_type =
				typename traits_t<Config_Type>::signals_collected_mixin_type;

		collected_signals_bunch_t( std::size_t size )
			:	mixin_base_type( size )
			{}

	public :
		using mixin_base_type::size;
	};

//
// collected_signals_bunch_builder_t
//
/*!
 * \brief A builder for case when collecting_mbox collects signals.
 *
 * In this case only count of collected signals must be maintained.
 *
 * A message to be sent can be created directly in extract_message().
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
class collected_signals_bunch_builder_t
	{
	public :
		// Actual message type to be used.
		using message_type = collected_signals_bunch_t<Config_Type>;

	private :
		//! Count of collected signals.
		std::size_t m_collected_messages = 0;

	public :
		void
		store(
			message_ref_t /*message*/,
			std::size_t /*messages_to_collect*/ )
			{
				++m_collected_messages;
			}

		bool
		is_ready_to_be_sent( std::size_t messages_to_collect ) const
			{
				return m_collected_messages >= messages_to_collect;
			}

		std::unique_ptr< message_type >
		extract_message()
			{
				const auto constructor_arg = m_collected_messages;
				m_collected_messages = 0u;
				return std::unique_ptr< message_type >{
						new message_type{ constructor_arg } };
			}
	};

//
// collected_bunch_type_selector
//
/*!
 * \brief A helper type for selection of actual message type and
 * type of message builder.
 *
 * It defines two typedefs:
 *
 * * message_type. This will be a type for message to be sent when
 *   all collecting messages/signal are received;
 * * builder_type. This will be a type of object to collect received
 *   messages or signals and to build a new message to be sent.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
struct collected_bunch_type_selector
	{
		static constexpr bool is_signal =
				::so_5::is_signal< collecting_msg_t<Config_Type> >::value;

		using message_type = typename std::conditional<
					is_signal,
					collected_signals_bunch_t< Config_Type >,
					collected_messages_bunch_t< Config_Type > >
				::type;

		using builder_type = typename std::conditional<
					is_signal,
					collected_signals_bunch_builder_t< Config_Type >,
					collected_messages_bunch_builder_t< Config_Type > >
				::type;
	};

//
// messages_collected_t
//
/*!
 * \brief Type of message to be sent as messages_collected instance.
 *
 * It will be collected_messages_bunch_t if Config_Type::collecting_msg_type
 * is a type of a message. Or it will be collected_signals_bunch_t if
 * Config_Type::collecting_msg_type is a type of a signal.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 */
template< typename Config_Type >
using messages_collected_t = typename 
		collected_bunch_type_selector<Config_Type>::message_type;

//
// actual_mbox_t
//
/*!
 * \brief Actual implementation of collecting mbox.
 *
 * \tparam Config_Type a type with enumeration of all necessary type traits.
 * It is expected to be config_type with appropriate type parameters.
 *
 * \tparam Tracing_Base base class with implementation of message
 * delivery tracing methods. Expected to be tracing_enabled_base or
 * tracing_disabled_base from so_5::impl::msg_tracing_helpers namespace.
 */
template<
	typename Config_Type,
	typename Tracing_Base >
class actual_mbox_t final
	: public ::so_5::abstract_message_box_t
	, protected traits_t<Config_Type>::size_specific_base_type
	, protected ::so_5::details::lock_holder_detector< lock_t<Config_Type> >::type
	, protected Tracing_Base
	{
		//! Short alias for base type which is depended on consexpr or runtime
		//! size.
		using size_specific_base_type = typename
				traits_t<Config_Type>::size_specific_base_type;

		//! Short alias for base type which is depended on msg_tracing
		//! facilities.
		using tracing_base_type = Tracing_Base;

		//! Alias for actual message which will be sent when all messages
		//! or signals are collected.
		using messages_collected_t = ::so_5::extra::mboxes::collecting_mbox::details::
				messages_collected_t<Config_Type>;

		//! Alias for builder of message_collected.
		using messages_collected_builder_t = typename
				collected_bunch_type_selector<Config_Type>::builder_type;

		//! Alias for type which should be used for subscription to
		//! collecting messages.
		using collecting_message_subscription_type = typename
				message_payload_type<
						collecting_msg_t<Config_Type>>::subscription_type;

		//! Alias for type which should be used for subscription to
		//! message_collected message.
		using messages_collected_subscription_type = typename
			std::conditional<
					is_mutable_message< collecting_msg_t<Config_Type> >::value,
					mutable_msg< messages_collected_t >,
					messages_collected_t >
				::type;

		// Actual constructor which does calls of constructors of base classes.
		template<
			typename Specific_Base_Type_Tuple,
			std::size_t... Specific_Base_Type_Indexes,
			typename Tracing_Base_Type_Tuple,
			std::size_t... Tracing_Base_Type_Indexes >
		actual_mbox_t(
			mbox_id_t mbox_id,
			Specific_Base_Type_Tuple && specific_base_type_args,
			std::index_sequence<Specific_Base_Type_Indexes...> /*unused*/,
			Tracing_Base_Type_Tuple && tracing_base_type_args,
			std::index_sequence<Tracing_Base_Type_Indexes...> /*unused*/ )
			:	size_specific_base_type{
					mbox_id,
					std::get<Specific_Base_Type_Indexes>(
							std::forward<Specific_Base_Type_Tuple>(
									specific_base_type_args))... }
			,	tracing_base_type{
					std::get<Tracing_Base_Type_Indexes>(
							std::forward<Tracing_Base_Type_Tuple>(
									tracing_base_type_args))... }
			{
				check_mutability_validity_for_target_mbox<Config_Type>(
						this->m_target );
			}

	public :
		//! A public constructor.
		/*!
		 * Receives two tuples: one for parameters for size_specific_base_type's
		 * constructor and another for parameters for tracing_base_type's
		 * constructor.
		 *
		 * \tparam Size_Specific_Base_Args list of types for parameters for
		 * size_specific_base_type's constructor.
		 *
		 * \tparam Tracing_Base_Args list of types for parameters for
		 * tracing_base_type's constructor. Note: this can be an empty list.
		 */
		template<
			typename... Size_Specific_Base_Args,
			typename... Tracing_Base_Args >
		actual_mbox_t(
			//! Unique ID for that mbox.
			mbox_id_t mbox_id,
			//! Parameters related to constexpr or runtime size.
			std::tuple<Size_Specific_Base_Args...> && size_specific_base_args,
			//! Parameters related to msg_tracing facilities.
			//! Note: this can be an empty tuple.
			std::tuple<Tracing_Base_Args...> && tracing_base_args )
			:	actual_mbox_t{
					mbox_id,
					std::move(size_specific_base_args),
					std::make_index_sequence<sizeof...(Size_Specific_Base_Args)>{},
					std::move(tracing_base_args),
					std::make_index_sequence<sizeof...(Tracing_Base_Args)>{} }
			{}

		mbox_id_t
		id() const override
			{
				return this->m_id;
			}

		void
		subscribe_event_handler(
			const std::type_index & /*type_wrapper*/,
			const so_5::message_limit::control_block_t * /*limit*/,
			agent_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION(
						errors::rc_subscribe_event_handler_be_used_on_collecting_mbox,
						"subscribe_event_handler is called for collecting-mbox" );
			}

		void
		unsubscribe_event_handlers(
			const std::type_index & /*type_wrapper*/,
			agent_t & /*subscriber*/ ) override
			{
			}

		std::string
		query_name() const override
			{
				std::ostringstream s;
				s << "<mbox:type=COLLECTINGMBOX:id=" << this->m_id << ">";

				return s.str();
			}

		mbox_type_t
		type() const override
			{
				return this->m_target->type();
			}

		void
		do_deliver_message(
			const std::type_index & msg_type,
			const message_ref_t & message,
			unsigned int overlimit_reaction_deep ) override
			{
				ensure_valid_message_type( msg_type );

				typename Tracing_Base::deliver_op_tracer tracer{
						*this, // as Tracing_Base
						*this, // as abstract_message_box_t
						"collect_message",
						msg_type, message, overlimit_reaction_deep };

				collect_new_message( tracer, message );
			}

		void
		set_delivery_filter(
			const std::type_index & /*msg_type*/,
			const delivery_filter_t & /*filter*/,
			agent_t & /*subscriber*/ ) override
			{
				SO_5_THROW_EXCEPTION(
						errors::rc_delivery_filter_cannot_be_used_on_collecting_mbox,
						"set_delivery_filter is called for collecting-mbox" );
			}

		void
		drop_delivery_filter(
			const std::type_index & /*msg_type*/,
			agent_t & /*subscriber*/ ) noexcept override
			{
				// Nothing to do.
			}

		so_5::environment_t &
		environment() const noexcept override
			{
				return this->m_target->environment();
			}

	private :
		//! The current instance of messages_collected to store 
		//! messages to be delivered.
		messages_collected_builder_t m_msg_builder;

		static void
		ensure_valid_message_type( const std::type_index & msg_type_id )
			{
				static const std::type_index expected_type_id =
					typeid(collecting_message_subscription_type);

				if( expected_type_id != msg_type_id )
					SO_5_THROW_EXCEPTION(
							errors::rc_different_message_type,
							std::string( "an attempt to send message or signal of "
									"different type. expected type: " )
							+ expected_type_id.name() + ", actual type: "
							+ msg_type_id.name() );
			}

		void
		collect_new_message(
			typename Tracing_Base::deliver_op_tracer const & tracer,
			const message_ref_t & message )
			{
				this->lock_and_perform( [&] {
					// A new message must be stored to the current messages_collected.
					m_msg_builder.store( message, this->messages_to_collect() );
					tracer.make_trace( "collected" );

					// Can we send messages_collected?
					if( m_msg_builder.is_ready_to_be_sent(
								this->messages_to_collect() ) )
						{
							using namespace ::so_5::impl::msg_tracing_helpers::details;

							auto msg_to_send = m_msg_builder.extract_message();

							tracer.make_trace( "deliver_collected_bunch",
									text_separator{ "->" },
									mbox_as_msg_destination{ *(this->m_target) } );

							this->m_target->do_deliver_message(
									typeid(messages_collected_subscription_type),
									std::move(msg_to_send),
									1u );
						}
				} );
			}
	};

} /* namespace details */

/*!
 * \brief A trait for mbox_template_t to be used when count of
 * messages to collected is known at the compile time.
 *
 * Usage example:
 * \code
 * using my_msg_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
 * 		my_msg,
 * 		so_5::extra::mboxes::collecting_mbox::constexpr_size_traits_t<10> >;
 * auto my_msg_mbox = my_msg_mbox_type::make( so_environment(), target_mbox );
 * \endcode
 */
template< std::size_t S >
struct constexpr_size_traits_t
	{
		/*!
		 * \brief Type of container to be used in messages_collected message.
		 *
		 * \note
		 * Because count of collected messages is known at compile time
		 * a very simple and efficient std::array is used.
		 */
		using container_type = std::array< message_ref_t, S >;

		/*!
		 * \brief A special mixin which must be used in actual type of
		 * messages_collected message for cases when signals are collected.
		 *
		 * \since
		 * v.1.0.2
		 */
		class signals_collected_mixin_type
			{
			public :
				signals_collected_mixin_type( std::size_t /*size*/ ) {}

				constexpr std::size_t
				size() const noexcept { return S; }
			};

		/*!
		 * \brief A special mixin which must be used in actual type of
		 * messages_collected message for cases when messages are collected.
		 */
		class messages_collected_mixin_type
			{
				container_type m_messages;
			public :
				messages_collected_mixin_type( std::size_t /*size*/ ) {}

				container_type &
				storage() noexcept { return m_messages; }

				const container_type &
				storage() const noexcept { return m_messages; }

				constexpr std::size_t
				size() const noexcept { return S; }
			};

		/*!
		 * \brief A special mixin which must be used in actual type of
		 * collecting mbox.
		 */
		struct size_specific_base_type
			{
				//! Unique ID of mbox.
				const mbox_id_t m_id;
				//! A target for messages_collected.
				const mbox_t m_target;

				//! Constructor.
				size_specific_base_type(
					mbox_id_t mbox_id,
					mbox_t target )
					:	m_id{ mbox_id }
					,	m_target{ std::move(target) }
					{}

				/*!
				 * \brief Total count of messages to be collected before
				 * messages_collected will be sent.
				 */
				constexpr std::size_t messages_to_collect() const noexcept { return S; }
			};
	};

/*!
 * \brief A trait for mbox_template_t to be used when count of
 * messages to collected is known only at runtime.
 *
 * Usage example:
 * \code
 * using my_msg_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
 * 		my_msg,
 * 		so_5::extra::mboxes::collecting_mbox::runtime_size_traits_t >;
 * auto my_msg_mbox = my_msg_mbox_type::make( so_environment(), target_mbox, collected_msg_count );
 * \endcode
 */
struct runtime_size_traits_t
	{
		/*!
		 * \brief Type of container to be used for collected messages.
		 */
		using container_type = std::vector< message_ref_t >;

		/*!
		 * \brief A special mixin which must be used in actual type of
		 * messages_collected message for cases when signals are collected.
		 */
		class signals_collected_mixin_type
			{
				std::size_t m_size;
			public :
				signals_collected_mixin_type( std::size_t size ) : m_size{size} {}

				std::size_t
				size() const noexcept { return m_size; }
			};

		/*!
		 * \brief A special mixin which must be used in actual type of
		 * messages_collected message.
		 */
		class messages_collected_mixin_type
			{
				container_type m_messages;
			public :
				messages_collected_mixin_type( std::size_t size )
					: m_messages( size, message_ref_t{} )
					{}

				container_type &
				storage() noexcept { return m_messages; }

				const container_type &
				storage() const noexcept { return m_messages; }

				std::size_t
				size() const noexcept { return m_messages.size(); }
			};

		/*!
		 * \brief A special mixin which must be used in actual type of
		 * collecting mbox.
		 */
		struct size_specific_base_type
			{
				//! Unique ID of mbox.
				const mbox_id_t m_id;
				//! A target for messages_collected.
				const mbox_t m_target;
				//! Count of messages/signals to be collected.
				const std::size_t m_size;

				//! Constructor.
				size_specific_base_type(
					mbox_id_t mbox_id,
					mbox_t target,
					std::size_t size )
					:	m_id{ mbox_id }
					,	m_target{ std::move(target) }
					,	m_size{ size }
					{}

				//! Total count of messages to be collected before
				//! messages_collected will be sent.
				std::size_t messages_to_collect() const noexcept { return m_size; }
			};
	};

//
// mbox_template_t
//
/*!
 * \brief A template which defines properties for a collecting mbox.
 *
 * Usage examples:
 *
 * 1. Collecting mbox for immutable messages of type my_msg. Count of
 * messages to be collected is known only at runtime.
 * \code
 * using my_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
 * 		my_msg >;
 * auto my_mbox = my_mbox_type::make(
 * 		// A target mbox for messages_collected_t.
 * 		target_mbox,
 * 		// Count of messages to be collected.
 * 		messages_to_collect );
 *
 * // To receve messages_collected_t from my_mbox:
 * void my_agent::on_messages_collected(mhood_t<my_mbox_type::messages_collected_t> cmd) {
 * 	...
 * }
 * \endcode
 *
 * 2. Collecting mbox for immutable messages of type my_msg. Count of
 * messages to be collected is known at the compile time.
 * \code
 * using my_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
 * 		my_msg,
 * 		so_5::extra::mboxes::collecting_mbox::constexpr_size_traits_t<10> >;
 * // Note: there is no need to specify message count because it is already known.
 * auto my_mbox = my_mbox_type::make(
 * 		// A target mbox for messages_collected_t.
 * 		target_mbox );
 *
 * // To receve messages_collected_t from my_mbox:
 * void my_agent::on_messages_collected(mhood_t<my_mbox_type::messages_collected_t> cmd) {
 * 	...
 * }
 * \endcode
 *
 * 3. Collecting mbox for mutable messages of type my_msg. Count of
 * messages to be collected is known only at runtime.
 * Please note that message_collected_t is also delivered as mutable message!
 * \code
 * using my_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
 * 		so_5::mutable_msg<my_msg> >;
 * auto my_mbox = my_mbox_type::make(
 * 		// A target mbox for messages_collected_t.
 * 		target_mbox,
 * 		// Count of messages to be collected.
 * 		messages_to_collect );
 *
 * // To receve messages_collected_t from my_mbox:
 * void my_agent::on_messages_collected(mutable_mhood_t<my_mbox_type::messages_collected_t> cmd) {
 * 	...
 * }
 * \endcode
 *
 * 4. Collecting mbox for mutable messages of type my_msg. Count of
 * messages to be collected is known at the compile time.
 * Please note that message_collected_t is also delivered as mutable message!
 * \code
 * using my_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
 * 		so_5::mutable_msg<my_msg>,
 * 		so_5::extra::mboxes::collecting_mbox::constexpr_size_traits_t<10> >;
 * // Note: there is no need to specify message count because it is already known.
 * auto my_mbox = my_mbox_type::make(
 * 		// A target mbox for messages_collected_t.
 * 		target_mbox );
 *
 * // To receve messages_collected_t from my_mbox:
 * void my_agent::on_messages_collected(mutable_mhood_t<my_mbox_type::messages_collected_t> cmd) {
 * 	...
 * }
 * \endcode
 *
 * A type of message with collected messages is specified by inner type
 * mbox_template_t::messages_collected_t. Please note that actual message type
 * for messages_collected_t will depend on \a Collecting_Msg template parameter.
 * If \a Collecting_Msg is message type then messages_collected_t will be
 * message which holds collected messages inside. Such message type will have
 * the following interface:
 * \code
// Interface of messages_collected_t for the case
// when Collecting_Msg is a message type.
class message_collected_t {
	... // Some private stuff.
public :
	// Count of collected messages.
	std::size_t size() const;

	// Perform some action on collected message with the specified index.
	template<typename F>
	decltype(auto) with_nth(std::size_t index, F && f) const;

	// Perform some action on every collected message.
	template<typename F>
	void for_each(F && f) const;

	// Perform some action on every collected message.
	// Index of message is also passed to functor f.
	template<typename F>
	void for_each_with_index(F && f) const;
};
\endcode
 * A functor for methods `with_nth` and `for_each` must have the following
 * format:
 * \code
 * return_type f(mhood_t<Collecting_Msg> m);
 * \endcode
 * A functor for method `for_each_with_index` must have the following
 * format:
 * \code
 * return_type f(std::size_t index, mhood_t<Collecting_Msg> m);
 * \endcode
 * For example, handling of collected immutable messages can looks like:
\code
using my_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
		my_msg >;
...
void my_agent::on_my_messages(mhood_t<my_mbox_type::messages_collected_t> cmd) {
	cmd->for_each( [](mhood_t<my_msg> m) { ... } );
}
\endcode
 * And handling of collected mutable messages can looks like:
\code
using my_mbox_type = so_5::extra::mboxes::collecting_mbox::mbox_template_t<
		so_5::mutable_msg<my_msg> >;
...
void my_agent::on_my_messages(mutable_mhood_t<my_mbox_type::messages_collected_t> cmd) {
	cmd->for_each( [](mutable_mhood_t<my_msg> m) { ... } );
}
\endcode
 *
 * If \a Collecting_Msg is a type of signal, then
 * mbox_template_t::messages_collected_t will have the following format:
\code
// Interface of messages_collected_t for the case
// when Collecting_Msg is a signal type.
class message_collected_t {
	... // Some private stuff.
public :
	// Count of collected messages.
	std::size_t size() const;
};
\endcode
 * It means that if \a Collecting_Msg is a signal type then there is no
 * any collected signals instances.
 *
 * \note
 * Collecting mbox can be used for collecting mutable messages. But there are
 * some limitations:
 * - mutable messages can be collected only if \a target_mbox is
 *   multi-producer/single-consumer mbox. It is because messages_collected_t
 *   will be sent also as a mutable message. And sending of mutable messages
 *   it allowed only to MPSC mboxes;
 * - messages_collected_t will be sent as mutable message;
 * - it is impossible to collect mutable signals (this is prohibited by
 *   SObjectizer);
 * - it is impossible to collect mutable and immutable messages of the same
 *   type.
 *
 * \tparam Collecting_Msg type of message to be collected. It can be simple
 * type like `my_msg` (in this case only immutable messages of type `my_msg`
 * will be collected). Or it can be `so_5::mutable_msg<my_msg>` (in this
 * case only mutable messages of type `my_msg` will be collected).
 *
 * \tparam Traits type of size-specific traits. It is expected to be
 * constexpr_size_traits_t or runtime_size_traits_t (or any other type like
 * these two).
 *
 * \tparam Lock_Type type of lock to be used for thread safety. It can be
 * std::mutex or so_5::null_mutex_t (or any other type which can be used
 * with std::lock_quard).
 */
template<
	typename Collecting_Msg,
	typename Traits = runtime_size_traits_t,
	typename Lock_Type = std::mutex >
class mbox_template_t final
	{
		//! A configuration to be used for that mbox type.
		using config_type = details::config_type< Collecting_Msg, Traits, Lock_Type >;
	public :
		//! Actual type of message_collected instance.
		using messages_collected_t = typename 
				details::messages_collected_t<config_type>;

		/*!
		 * \brief Create an instance of collecting mbox.
		 *
		 * Please note that actual list of parameters depends on
		 * \a Traits type.
		 * If \a Traits is constexpr_size_traits_t then `make` will have the
		 * following format:
		 * \code
		 * mbox_t make(const mbox_t & target);
		 * \endcode
		 * If \a Traits is runtime_size_traits_t then `make` will have the
		 * following format:
		 * \code
		 * mbox_t make(const mbox_t & target, size_t messages_to_collect);
		 * \endcode
		 */
		template< typename... Args >
		static mbox_t
		make( const mbox_t & target, Args &&... args )
			{
				ensure_not_mutable_signal< Collecting_Msg >();

				return target->environment().make_custom_mbox(
						[&]( const mbox_creation_data_t & data ) {
							mbox_t result;

							if( data.m_tracer.get().is_msg_tracing_enabled() )
								{
									using T = details::actual_mbox_t<
											config_type,
											::so_5::impl::msg_tracing_helpers::tracing_enabled_base >;

									result = mbox_t{ new T{
											data.m_id,
											std::make_tuple( target, std::forward<Args>(args)... ),
											std::make_tuple( std::ref(data.m_tracer.get()) )
									} };
								}
							else
								{
									using T = details::actual_mbox_t<
											config_type,
											::so_5::impl::msg_tracing_helpers::tracing_disabled_base >;
									result = mbox_t{ new T{
											data.m_id,
											std::make_tuple( target, std::forward<Args>(args)... ),
											std::make_tuple()
									} };
								}

							return result;
						} );
			}
	};

} /* namespace collecting_mbox */

} /* namespace mboxes */

} /* namespace extra */

} /* namespace so_5 */

