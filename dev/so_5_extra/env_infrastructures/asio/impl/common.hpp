/*!
 * \file
 * \brief Common parts for Asio's env_infrastructures.
 */

#pragma once

#include <so_5/impl/st_env_infrastructure_reuse.hpp>

#include <asio.hpp>

namespace so_5 {

namespace extra {

namespace env_infrastructures {

namespace asio {

namespace common {

namespace helpers {

//
// ensure_we_can_handle_this_timer_error_code
//
/*!
 * \brief Helper function which checke error_code value.
 *
 * It returns normally if there is no an error or if the error 
 * is ::asio::error::operation_aborted.
 */
void
ensure_we_can_handle_this_timer_error_code(
	const ::asio::error_code & ec )
	{
		if( ec && ::asio::error::operation_aborted != ec )
			SO_5_THROW_EXCEPTION( rc_unexpected_error,
					"Only asio::operation_aborted error code can be handled "
					"by timer handler" );
	}

} /* namespace helpers */

//
// singleshot_timer_holder_t
//
/*!
 * \brief A main part of implementation of single-shot timer.
 */
class singleshot_timer_holder_t : public atomic_refcounted_t
	{
		friend intrusive_ptr_t< singleshot_timer_holder_t >;

		//! Actual Asio's timer.
		::asio::steady_timer m_timer;
		//! Type of message/signal to be delivered.
		const std::type_index m_type_index;
		//! Instance of message to be delivered.
		const message_ref_t m_msg;
		//! A destination mbox.
		const mbox_t m_mbox;

	public :
		//! Initializing constructor.
		singleshot_timer_holder_t(
			//! Asio's io_context to be used for timer.
			::asio::io_context & io_svc,
			//! Type of message/signal to be delivered.
			const std::type_index & type_index,
			//! Message instance to be delivered.
			const message_ref_t & msg,
			//! A destination mbox.
			const mbox_t & mbox )
			:	m_timer( io_svc )
			,	m_type_index( type_index )
			,	m_msg( msg )
			,	m_mbox( mbox )
			{}

		//! Cancelation of the timer.
		void
		cancel()
			{
				m_timer.cancel();
			}

		//! Scheduling of timer.
		void
		schedule_from_now(
			//! A pause in message delivery.
			std::chrono::steady_clock::duration pause )
			{
				intrusive_ptr_t< singleshot_timer_holder_t > self{ this };
				m_timer.expires_after( pause );
				m_timer.async_wait(
					// Timer action shouldn't throw because we don't know
					// how to repair it.
					[self]( const ::asio::error_code & ec ) noexcept {
						helpers::ensure_we_can_handle_this_timer_error_code( ec );
						if( ::asio::error::operation_aborted == ec )
							return;

						::so_5::impl::mbox_iface_for_timers_t{ self->m_mbox }
								.deliver_message_from_timer(
										self->m_type_index,
										self->m_msg );
					} );
			}
	};

//
// periodic_timer_holder_t
//
/*!
 * \brief A main part of implementation of periodic timer.
 *
 * \note This class is very similar to singleshot_timer_holder_t, but
 * these classes are not related to simplify implemenation of them.
 */
class periodic_timer_holder_t : public atomic_refcounted_t
	{
		friend intrusive_ptr_t< periodic_timer_holder_t >;

		//! Actual Asio's timer.
		::asio::steady_timer m_timer;
		//! Type of message/signal to be delivered.
		const std::type_index m_type_index;
		//! Instance of message to be delivered.
		const message_ref_t m_msg;
		//! A destination mbox.
		const mbox_t m_mbox;
		//! A repetition period for periodic message delivery.
		const std::chrono::steady_clock::duration m_period;

	public :
		periodic_timer_holder_t(
			//! Asio's io_context to be used for timer.
			::asio::io_context & io_svc,
			//! Type of message/signal to be delivered.
			const std::type_index & type_index,
			//! Message instance to be delivered.
			const message_ref_t & msg,
			//! A destination mbox.
			const mbox_t & mbox,
			//! A repetition period for periodic message delivery.
			std::chrono::steady_clock::duration period )
			:	m_timer( io_svc )
			,	m_type_index( type_index )
			,	m_msg( msg )
			,	m_mbox( mbox )
			,	m_period( period )
			{}

		//! Cancelation of the timer.
		void
		cancel()
			{
				m_timer.cancel();
			}

		//! Scheduling of timer.
		void
		schedule_from_now(
			//! A pause in message delivery.
			std::chrono::steady_clock::duration pause )
			{
				intrusive_ptr_t< periodic_timer_holder_t > self{ this };
				m_timer.expires_after( pause );
				m_timer.async_wait(
					// Timer action shouldn't throw because we don't know
					// how to repair it.
					[self]( const ::asio::error_code & ec ) noexcept {
						helpers::ensure_we_can_handle_this_timer_error_code( ec );
						if( ::asio::error::operation_aborted == ec )
							return;

						::so_5::impl::mbox_iface_for_timers_t{ self->m_mbox }
								.deliver_message_from_timer(
										self->m_type_index,
										self->m_msg );
						self->schedule_from_now( self->m_period );
					} );
			}
	};

//
// actual_timer_t
//
/*!
 * \brief A template for implementation of actual timer.
 *
 * \tparam Holder Type of actual timer data. Expected to be
 * periodic_timer_holder_t or singleshot_timer_holder_t.
 */
template< typename Holder >
class actual_timer_t : public timer_t
	{
	public :
		using holder_t = Holder;
		using holder_smart_ptr_t = intrusive_ptr_t< holder_t >;

		actual_timer_t(
			holder_smart_ptr_t holder )
			:	m_holder( std::move(holder) )
			{}
		virtual ~actual_timer_t() override
			{
				release();
			}

		virtual bool
		is_active() const noexcept override
			{
				return m_holder;
			}

		virtual void
		release() noexcept override
			{
				if( m_holder )
					{
						m_holder->cancel();
						m_holder.reset();
					}
			}

	private :
		holder_smart_ptr_t m_holder;
	};

} /* namespace common */

} /* namespace asio */

} /* namespace env_infrastructures */

} /* namespace extra */

} /* namespace so_5 */

