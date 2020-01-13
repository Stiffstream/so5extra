/*!
 * \file
 * \brief Implementation of Asio-based simple thread safe
 * environment infrastructure.
 */

#pragma once

#include <so_5_extra/env_infrastructures/asio/impl/common.hpp>

#include <so_5/impl/st_env_infrastructure_reuse.hpp>
#include <so_5/impl/internal_env_iface.hpp>
#include <so_5/details/sync_helpers.hpp>
#include <so_5/details/at_scope_exit.hpp>
#include <so_5/details/invoke_noexcept_code.hpp>

#include <asio.hpp>

#include <string_view>

namespace so_5 {

namespace extra {

namespace env_infrastructures {

namespace asio {

namespace simple_mtsafe {

namespace impl {

//! A short name for namespace with reusable stuff.
namespace reusable = ::so_5::env_infrastructures::st_reusable_stuff;

//! A short name for namespace with common stuff.
namespace asio_common = ::so_5::extra::env_infrastructures::asio::common;

//
// shutdown_status_t
//
using shutdown_status_t = reusable::shutdown_status_t;

//
// coop_repo_t
//
/*!
 * \brief Implementation of coop_repository for
 * simple thread-safe single-threaded environment infrastructure.
 */
using coop_repo_t = reusable::coop_repo_t;

//
// stats_controller_t
//
/*!
 * \brief Implementation of stats_controller for that type of
 * single-threaded environment.
 */
using stats_controller_t =
	reusable::stats_controller_t< so_5::details::actual_lock_holder_t<> >;

//
// event_queue_impl_t
//
/*!
 * \brief Implementation of event_queue interface for the default dispatcher.
 *
 * \tparam Activity_Tracker A type for tracking work thread activity.
 */
template< typename Activity_Tracker >
class event_queue_impl_t final : public event_queue_t
	{
		using pending_demands_type = std::atomic< std::size_t >;

	public :
		//! Type for representation of statistical data for this event queue.
		struct stats_t
			{
				//! The current size of the demands queue.
				std::size_t m_demands_count;
			};

		//! Initializing constructor.
		event_queue_impl_t(
			//! Asio's io_context to be used for dispatching.
			outliving_reference_t<::asio::io_context> io_svc,
			//! Actual activity tracker.
			outliving_reference_t<Activity_Tracker> activity_tracker )
			:	m_io_svc(io_svc)
			,	m_activity_tracker(activity_tracker)
			,	m_pending_demands{}
			{
			}

		virtual void
		push( execution_demand_t demand ) override
			{
				// Statistics must be updated.
				++m_pending_demands;

				// Now we can schedule processing of the demand.
				// It ::asio::post fails then statistics must be reverted.
				::so_5::details::do_with_rollback_on_exception(
					[&] {
						::asio::post(
								m_io_svc.get(),
								[this, d = std::move(demand)]() mutable {
									// Statistics must be updated.
									--m_pending_demands;

									// Update wait statistics.
									m_activity_tracker.get().wait_stopped();
									const auto wait_starter = ::so_5::details::at_scope_exit(
											[this]{ m_activity_tracker.get().wait_started(); } );

									// The demand can be handled now.
									// With working time tracking.
									m_activity_tracker.get().work_started();
									{
										// For the case if call_handler will throw.
										const auto stopper = ::so_5::details::at_scope_exit(
												[this]{ m_activity_tracker.get().work_stopped(); });

										d.call_handler( m_thread_id );
									}
								} );
					},
					[this] {
						--m_pending_demands;
					} );
			}

		//! Notification that event queue work is started.
		void
		start(
			//! ID of the main working thread.
			current_thread_id_t thread_id )
			{
				m_thread_id = std::move(thread_id);

				// There is no any pending demand now. We can start counting
				// the waiting time.
				m_activity_tracker.get().wait_started();
			}

		//! Get the current statistics.
		stats_t
		query_stats() const noexcept
			{
				return { m_pending_demands.load( std::memory_order_acquire ) };
			}

	private :
		outliving_reference_t<::asio::io_context> m_io_svc;
		outliving_reference_t<Activity_Tracker> m_activity_tracker;

		current_thread_id_t m_thread_id;

		std::atomic<std::size_t> m_pending_demands;
	};

//
// disp_ds_name_parts_t
//
/*!
 * \brief A class with major part of dispatcher name.
 */
struct disp_ds_name_parts_t final
	{
		static constexpr std::string_view
		disp_type_part() noexcept { return { "asio_mtsafe" }; }
	};

//
// default_dispatcher_t
//
/*!
 * \brief An implementation of dispatcher to be used in
 * places where default dispatcher is needed.
 *
 * \tparam Activity_Tracker a type of activity tracker to be used
 * for run-time statistics.
 *
 * \since
 * v.1.3.0
 */
template< typename Activity_Tracker >
class default_dispatcher_t final
	: public reusable::default_dispatcher_t<
			event_queue_impl_t< Activity_Tracker >,
			Activity_Tracker,
			disp_ds_name_parts_t >
	{
		using base_type = reusable::default_dispatcher_t<
				event_queue_impl_t< Activity_Tracker >,
				Activity_Tracker,
				disp_ds_name_parts_t >;

	public :
		default_dispatcher_t(
			outliving_reference_t< environment_t > env,
			outliving_reference_t< event_queue_impl_t<Activity_Tracker> > event_queue,
			outliving_reference_t< Activity_Tracker > activity_tracker )
			:	base_type{ env, event_queue, activity_tracker }
			{
				// Event queue should be started manually.
				// We known that the default dispatcher is created on a thread
				// that will be used for events dispatching.
				event_queue.get().start( this->thread_id() );
			}
	};

//
// env_infrastructure_t
//
/*!
 * \brief Default implementation of not-thread safe single-threaded environment
 * infrastructure.
 *
 * \attention
 * This object doesn't have any mutexes. All synchronization is done via
 * delegation mutating operations to Asio's context (asio::post and
 * asio::dispatch are used).
 *
 * \tparam Activity_Tracker A type of activity tracker to be used.
 */
template< typename Activity_Tracker >
class env_infrastructure_t
	: public environment_infrastructure_t
	{
	public :
		env_infrastructure_t(
			//! Asio's io_context to be used.
			outliving_reference_t<::asio::io_context> io_svc,
			//! Environment to work in.
			environment_t & env,
			//! Cooperation action listener.
			coop_listener_unique_ptr_t coop_listener,
			//! Mbox for distribution of run-time stats.
			mbox_t stats_distribution_mbox );

		void
		launch( env_init_t init_fn ) override;

		void
		stop() override;

		[[nodiscard]]
		coop_unique_holder_t
		make_coop(
			coop_handle_t parent,
			disp_binder_shptr_t default_binder ) override;

		coop_handle_t
		register_coop(
			coop_unique_holder_t coop ) override;

		void
		ready_to_deregister_notify(
			coop_shptr_t coop ) noexcept override;

		bool
		final_deregister_coop(
			coop_shptr_t coop_name ) override;

		so_5::timer_id_t
		schedule_timer(
			const std::type_index & type_wrapper,
			const message_ref_t & msg,
			const mbox_t & mbox,
			std::chrono::steady_clock::duration pause,
			std::chrono::steady_clock::duration period ) override;

		void
		single_timer(
			const std::type_index & type_wrapper,
			const message_ref_t & msg,
			const mbox_t & mbox,
			std::chrono::steady_clock::duration pause ) override;

		stats::controller_t &
		stats_controller() noexcept override;

		stats::repository_t &
		stats_repository() noexcept override;

		so_5::environment_infrastructure_t::coop_repository_stats_t
		query_coop_repository_stats() override;

		timer_thread_stats_t
		query_timer_thread_stats() override;

		disp_binder_shptr_t
		make_default_disp_binder() override;

	private :
		//! Asio's io_context to be used.
		outliving_reference_t< ::asio::io_context > m_io_svc;

		//! Actual SObjectizer Environment.
		environment_t & m_env;

		//! Status of shutdown procedure.
		std::atomic< shutdown_status_t > m_shutdown_status;

		//! Repository of registered coops.
		coop_repo_t m_coop_repo;

		//! Actual activity tracker.
		Activity_Tracker m_activity_tracker;

		//! Event queue which is necessary for the default dispatcher.
		event_queue_impl_t< Activity_Tracker > m_event_queue;

		//! Dispatcher to be used as default dispatcher.
		/*!
		 * \note
		 * Has an actual value only inside launch() method.
		 */
		std::shared_ptr< default_dispatcher_t< Activity_Tracker > > m_default_disp;

		//! Stats controller for this environment.
		stats_controller_t m_stats_controller;

		//! Counter of cooperations which are waiting for final deregistration
		//! step.
		/*!
		 * It is necessary for building correct run-time stats.
		 */
		std::atomic< std::size_t > m_final_dereg_coop_count;

		//! The pointer to an exception that was thrown during init phase.
		/*!
		 * This exception is stored inside a callback posted to Asio.
		 * An then this exception will be rethrown from launch() method
		 * after the shutdown of SObjectizer.
		 */
		std::exception_ptr m_exception_from_init;

		void
		run_default_dispatcher_and_go_further( env_init_t init_fn );

		/*!
		 * \note Calls m_io_svc.stop() and m_default_disp.shutdown() if necessary.
		 *
		 * \attention Must be called only for locked object!
		 */
		void
		check_shutdown_completeness();
	};

template< typename Activity_Tracker >
env_infrastructure_t<Activity_Tracker>::env_infrastructure_t(
	outliving_reference_t<::asio::io_context> io_svc,
	environment_t & env,
	coop_listener_unique_ptr_t coop_listener,
	mbox_t stats_distribution_mbox )
	:	m_io_svc( io_svc )
	,	m_env( env )
	,	m_shutdown_status( shutdown_status_t::not_started )
	,	m_coop_repo( outliving_mutable(env), std::move(coop_listener) )
	,	m_event_queue( io_svc, outliving_mutable(m_activity_tracker) )
	,	m_stats_controller(
			stats_distribution_mbox,
			stats::impl::st_env_stuff::next_turn_mbox_t::make(m_env) )
	,	m_final_dereg_coop_count( 0 )
	{}

template< typename Activity_Tracker >
void
env_infrastructure_t<Activity_Tracker>::launch( env_init_t init_fn )
	{
		// Post initial operation to Asio event loop.
		::asio::post( m_io_svc.get(), [this, init = std::move(init_fn)] {
			run_default_dispatcher_and_go_further( std::move(init) );
		} );

		// Default dispatcher should be destroyed on exit from this function.
		auto default_disp_destroyer = so_5::details::at_scope_exit( [this] {
				m_default_disp.reset();
			} );

		// Tell that there is a work to do.
		auto work = ::asio::make_work_guard( m_io_svc.get() );

		// Launch Asio event loop.
		m_io_svc.get().run();

		if( m_exception_from_init )
			// Some exception was thrown during initialization.
			// It should be rethrown.
			std::rethrow_exception( m_exception_from_init );
	}

template< typename Activity_Tracker >
void
env_infrastructure_t<Activity_Tracker>::stop()
	{
		// NOTE: if the code below throws then we don't know the actual
		// state of env_infrastructure. Because of that we just terminate
		// the whole application the the case of an exception.
		::so_5::details::invoke_noexcept_code( [&] {
			auto expected_status = shutdown_status_t::not_started;
			if( m_shutdown_status.compare_exchange_strong(
					expected_status,
					shutdown_status_t::must_be_started ) )
				{
					// All registered cooperations must be deregistered now.
					::asio::dispatch( m_io_svc.get(),
						[this] {
							m_shutdown_status = shutdown_status_t::in_progress;

							m_coop_repo.deregister_all_coop();
							
							check_shutdown_completeness();
						} );
				}
			else
				// Check for shutdown completeness must be performed only
				// on the main Asio's thread.
				::asio::dispatch( m_io_svc.get(), [this] {
					check_shutdown_completeness();
				} );
		} );
	}

template< typename Activity_Tracker >
coop_unique_holder_t
env_infrastructure_t< Activity_Tracker >::make_coop(
	coop_handle_t parent,
	disp_binder_shptr_t default_binder ) 
	{
		return m_coop_repo.make_coop(
				std::move(parent),
				std::move(default_binder) );
	}

template< typename Activity_Tracker >
coop_handle_t
env_infrastructure_t< Activity_Tracker >::register_coop(
	coop_unique_holder_t coop )
	{
		return m_coop_repo.register_coop( std::move(coop) );
	}

template< typename Activity_Tracker >
void
env_infrastructure_t<Activity_Tracker>::ready_to_deregister_notify(
	coop_shptr_t coop_to_dereg ) noexcept
	{
		++m_final_dereg_coop_count;

		::asio::post( m_io_svc.get(), [this, coop = std::move(coop_to_dereg)] {
			--m_final_dereg_coop_count;
			so_5::impl::internal_env_iface_t{ m_env }
					.final_deregister_coop( std::move(coop) );
		} );
	}

template< typename Activity_Tracker >
bool
env_infrastructure_t<Activity_Tracker>::final_deregister_coop(
	coop_shptr_t coop )
	{
		return m_coop_repo.final_deregister_coop( std::move(coop) )
				.m_has_live_coop;
	}

template< typename Activity_Tracker >
so_5::timer_id_t
env_infrastructure_t<Activity_Tracker>::schedule_timer(
	const std::type_index & type_index,
	const message_ref_t & msg,
	const mbox_t & mbox,
	std::chrono::steady_clock::duration pause,
	std::chrono::steady_clock::duration period )
	{
		using namespace asio_common;

		// We do not control shutdown_status_t here. Because it seems
		// to be safe to call schedule_timer after call to stop().
		// New timer will simply ignored during shutdown process.
		timer_id_t result;
		if( period != std::chrono::steady_clock::duration::zero() )
			{
				intrusive_ptr_t< periodic_timer_holder_t > timer{
						new periodic_timer_holder_t(
								m_io_svc.get(),
								type_index,
								msg,
								mbox,
								period ) };

				result = timer_id_t{
						new actual_timer_t< periodic_timer_holder_t >( timer ) };

				timer->schedule_from_now( pause );
			}
		else
			{
				intrusive_ptr_t< singleshot_timer_holder_t > timer{
					new singleshot_timer_holder_t(
							m_io_svc.get(),
							type_index,
							msg,
							mbox ) };

				result = timer_id_t{
						new actual_timer_t< singleshot_timer_holder_t >( timer ) };

				timer->schedule_from_now( pause );
			}

		return result;
	}

template< typename Activity_Tracker >
void
env_infrastructure_t<Activity_Tracker>::single_timer(
	const std::type_index & type_index,
	const message_ref_t & msg,
	const mbox_t & mbox,
	std::chrono::steady_clock::duration pause )
	{
		using namespace asio_common;

		// We do not control shutdown_status_t here. Because it seems
		// to be safe to call schedule_timer after call to stop().
		// New timer will simply ignored during shutdown process.

		intrusive_ptr_t< singleshot_timer_holder_t > timer{
			new singleshot_timer_holder_t(
					m_io_svc.get(),
					type_index,
					msg,
					mbox ) };

		timer->schedule_from_now( pause );
	}

template< typename Activity_Tracker >
stats::controller_t &
env_infrastructure_t<Activity_Tracker>::stats_controller() noexcept
	{
		return m_stats_controller;
	}

template< typename Activity_Tracker >
stats::repository_t &
env_infrastructure_t<Activity_Tracker>::stats_repository() noexcept
	{
		return m_stats_controller;
	}

template< typename Activity_Tracker >
so_5::environment_infrastructure_t::coop_repository_stats_t
env_infrastructure_t<Activity_Tracker>::query_coop_repository_stats()
	{
		const auto stats = m_coop_repo.query_stats();

		return environment_infrastructure_t::coop_repository_stats_t{
				stats.m_total_coop_count,
				stats.m_total_agent_count,
				m_final_dereg_coop_count.load( std::memory_order_acquire )
		};
	}

template< typename Activity_Tracker >
timer_thread_stats_t
env_infrastructure_t<Activity_Tracker>::query_timer_thread_stats()
	{
		// NOTE: this type of environment_infrastructure doesn't support
		// statistics for timers.
		return { 0, 0 };
	}

template< typename Activity_Tracker >
disp_binder_shptr_t
env_infrastructure_t< Activity_Tracker >::make_default_disp_binder()
	{
		return { m_default_disp };
	}

template< typename Activity_Tracker >
void
env_infrastructure_t<Activity_Tracker>::run_default_dispatcher_and_go_further(
	env_init_t init_fn )
	{
		try
			{
				m_default_disp = std::make_shared<
						default_dispatcher_t< Activity_Tracker > >(
								outliving_mutable(m_env),
								outliving_mutable(m_event_queue),
								outliving_mutable(m_activity_tracker) );

				// User-supplied init can be called now.
				init_fn();
			}
		catch(...)
			{
				// We can't restore if the following fragment throws and exception.
				so_5::details::invoke_noexcept_code( [&] {
						// The current exception should be stored to be
						// rethrown later.
						m_exception_from_init = std::current_exception();

						// SObjectizer's shutdown should be initiated.
						stop();

						// NOTE: pointer to the default dispatcher will be dropped
						// in launch() method.
					} );
			}
	}

template< typename Activity_Tracker >
void
env_infrastructure_t<Activity_Tracker>::check_shutdown_completeness()
	{
		if( m_shutdown_status == shutdown_status_t::in_progress )
			{
				// If there is no more live coops then shutdown must be
				// completed.
				if( !m_coop_repo.has_live_coop() )
					{
						m_shutdown_status = shutdown_status_t::completed;
						// Asio's event loop must be broken here!
						m_io_svc.get().stop();
					}
			}
	}

} /* namespace impl */

//
// factory
//
/*!
 * \brief A factory for creation of environment infrastructure based on
 * Asio's event loop.
 *
 * \attention
 * This environment infrastructure is not a thread safe.
 *
 * Usage example:
 * \code
int main()
{
	asio::io_context io_svc;

	so_5::launch( [](so_5::environment_t & env) {
				... // Some initialization stuff.
			},
			[&io_svc](so_5::environment_params_t & params) {
				using asio_env = so_5::extra::env_infrastructures::asio::simple_mtsafe;

				params.infrastructure_factory( asio_env::factory(io_svc) );
			} );

	return 0;
}
 * \endcode
 */
inline environment_infrastructure_factory_t
factory( ::asio::io_context & io_svc )
	{
		using namespace impl;

		return [&io_svc](
				environment_t & env,
				environment_params_t & env_params,
				mbox_t stats_distribution_mbox )
		{
			environment_infrastructure_t * obj = nullptr;

			// Create environment infrastructure object in dependence of
			// work thread activity tracking flag.
			const auto tracking = env_params.work_thread_activity_tracking();
			if( work_thread_activity_tracking_t::on == tracking )
				obj = new env_infrastructure_t< reusable::real_activity_tracker_t >(
					outliving_mutable(io_svc),
					env,
					env_params.so5__giveout_coop_listener(),
					std::move(stats_distribution_mbox) );
			else
				obj = new env_infrastructure_t< reusable::fake_activity_tracker_t >(
					outliving_mutable(io_svc),
					env,
					env_params.so5__giveout_coop_listener(),
					std::move(stats_distribution_mbox) );

			return environment_infrastructure_unique_ptr_t(
					obj,
					environment_infrastructure_t::default_deleter() );
		};
	}

} /* namespace simple_mtsafe */

} /* namespace asio */

} /* namespace env_infrastructures */

} /* namespace extra */

} /* namespace so_5 */

