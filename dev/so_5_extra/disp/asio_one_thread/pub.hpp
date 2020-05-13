/*!
 * \file
 * \brief Implementation of Asio's One Thread dispatcher.
 *
 * \since
 * v.1.4.1
 */

#pragma once

#include <so_5_extra/error_ranges.hpp>

#include <so_5/disp_binder.hpp>
#include <so_5/send_functions.hpp>

#include <so_5/disp/reuse/work_thread_activity_tracking.hpp>
#include <so_5/disp/reuse/data_source_prefix_helpers.hpp>

#include <so_5/stats/repository.hpp>
#include <so_5/stats/messages.hpp>
#include <so_5/stats/std_names.hpp>
#include <so_5/stats/impl/activity_tracking.hpp>

#include <so_5/details/invoke_noexcept_code.hpp>
#include <so_5/details/rollback_on_exception.hpp>
#include <so_5/details/abort_on_fatal_error.hpp>

#include <so_5/impl/thread_join_stuff.hpp>

#include <so_5/outliving.hpp>

#include <asio/io_context.hpp>
#include <asio/post.hpp>

namespace so_5 {

namespace extra {

namespace disp {

namespace asio_one_thread {

namespace errors {

//! Asio IoService is not set for asio_thread_pool dispatcher.
const int rc_io_context_is_not_set =
		so_5::extra::errors::asio_one_thread_errors;

} /* namespace errors */

//FIXME: document this!
using io_context_shptr_t = std::shared_ptr< asio::io_context >;

//
// disp_params_t
//
/*!
 * \brief Parameters for %asio_one_thread dispatcher.
 */
class disp_params_t
	:	public ::so_5::disp::reuse::work_thread_activity_tracking_flag_mixin_t< disp_params_t >
	{
		using activity_tracking_mixin_t = ::so_5::disp::reuse::
				work_thread_activity_tracking_flag_mixin_t< disp_params_t >;

	public :
		//! Default constructor.
		disp_params_t() = default;

		friend inline void
		swap(
			disp_params_t & a, disp_params_t & b ) noexcept
			{
				using std::swap;

				swap(
						static_cast< activity_tracking_mixin_t & >(a),
						static_cast< activity_tracking_mixin_t & >(b) );

				swap( a.m_io_context, b.m_io_context );
			}

		//! Use external Asio io_context object with dispatcher.
		/*!
		 * Usage example:
		 * \code
		 * int main() {
		 * 	asio::io_context svc;
		 * 	so_5::launch( [&](so_5::environment_t & env) {
		 * 		namespace asio_ot = so_5::extra::disp::asio_one_thread;
		 * 		auto disp = asio_ot::create_private_disp(
		 * 				env, "asio_ot",
		 * 				asio_tp::disp_params_t{}.use_external_io_context(svc) );
		 * 		...
		 * 	} );
		 * }
		 * \endcode
		 */
		disp_params_t &
		use_external_io_context(
			::asio::io_context & service )
			{
				m_io_context = std::shared_ptr< ::asio::io_context >(
						std::addressof( service ),
						// Empty deleter.
						[](::asio::io_context *) {} );
				return *this;
			}

		//! Use external Asio io_context object with dispatcher.
		/*!
		 * \note
		 * Ownership of this io_context object must be shared with
		 * others.
		 */
		disp_params_t &
		use_external_io_context(
			std::shared_ptr< ::asio::io_context > service )
			{
				m_io_context = std::move(service);
				return *this;
			}

		//! Use own Asio io_context object.
		/*!
		 * Note this object will be dynamically created at the start
		 * of the dispatcher. And will be destroyed with the dispatcher object.
		 *
		 * A created io_context can be accessed later via io_context() method.
		 */
		disp_params_t &
		use_own_io_context()
			{
				m_io_context = std::make_shared< ::asio::io_context >();
				return *this;
			}

		//! Get the io_context.
		std::shared_ptr< ::asio::io_context >
		io_context() const noexcept
			{
				return m_io_context;
			}

	private :
		//! Asio's io_context which must be used with this dispatcher.
		std::shared_ptr< ::asio::io_context > m_io_context;
	};

namespace impl {

//
// actual_disp_binder_t
//
//FIXME: document this!
class actual_disp_binder_t
	:	public disp_binder_t
	{
	public :
		[[nodiscard]]
		virtual asio::io_context &
		io_context() noexcept = 0;
	};

//
// actual_disp_binder_shptr_t
//
using actual_disp_binder_shptr_t =
		std::shared_ptr< actual_disp_binder_t >;

class dispatcher_handle_maker_t;

} /* namespace impl */

//
// dispatcher_handle_t
//

/*!
 * \brief A handle for %asio_one_thread dispatcher.
 *
 * \since
 * v.1.4.1
 */
class [[nodiscard]] dispatcher_handle_t
	{
		friend class impl::dispatcher_handle_maker_t;

		//! A reference to actual implementation of a dispatcher.
		impl::actual_disp_binder_shptr_t m_binder;

		dispatcher_handle_t(
			impl::actual_disp_binder_shptr_t binder ) noexcept
			:	m_binder{ std::move(binder) }
			{}

		//! Is this handle empty?
		bool
		empty() const noexcept { return !m_binder; }

	public :
		dispatcher_handle_t() noexcept = default;

		//! Get a binder for that dispatcher.
		/*!
		 * Usage example:
		 * \code
		 * using namespace so_5::extra::disp::asio_one_thread;
		 *
		 * asio::io_context io_ctx;
		 *
		 * so_5::environment_t & env = ...;
		 * auto disp = make_dispatcher( env, "my_disp", io_ctx );
		 *
		 * env.introduce_coop( [&]( so_5::coop_t & coop ) {
		 * 	coop.make_agent_with_binder< some_agent_type >(disp.binder(), ...);
		 * 	...
		 * } );
		 * \endcode
		 *
		 * \attention
		 * An attempt to call this method on empty handle is UB.
		 */
		[[nodiscard]]
		disp_binder_shptr_t
		binder() const
			{
				return m_binder;
			}

		//! Get reference to io_context from that dispatcher.
		/*!
		 * \attention
		 * An attempt to call this method on empty handle is UB.
		 */
		[[nodiscard]]
		::asio::io_context &
		io_context() noexcept 
			{
				return m_binder->io_context();
			}

		//! Is this handle empty?
		operator bool() const noexcept { return empty(); }

		//! Does this handle contain a reference to dispatcher?
		bool
		operator!() const noexcept { return !empty(); }

		//! Drop the content of handle.
		void
		reset() noexcept { m_binder.reset(); }
	};

namespace impl {

//
// demands_counter_t
//
/*!
 * \brief Type of atomic counter for counting waiting demands.
 *
 * \since
 * v.1.4.1
 */
using demands_counter_t = std::atomic< std::size_t >;

namespace work_thread_details {

//FIXME: maybe some Traits type should be used here?
template< typename Thread_Type >
struct common_data_t
	{
		//! Asio's context to be used.
		io_context_shptr_t m_io_context;

		//! Thread object.
		Thread_Type m_thread;

		//! ID of the work thread.
		/*!
		 * \note Receives actual value only after successful start
		 * of the thread.
		 */
		so_5::current_thread_id_t m_thread_id;

		//! Counter of waiting demands.
		demands_counter_t m_demands_counter{ 0u };

		common_data_t( io_context_shptr_t io_context )
			: m_io_context( std::move(io_context) ) {}
	};

template< typename Thread_Type >
class no_activity_tracking_impl_t : protected common_data_t< Thread_Type >
	{
		using base_type_t = common_data_t< Thread_Type >;

	public :
		no_activity_tracking_impl_t( io_context_shptr_t io_context )
			:	base_type_t( std::move(io_context) )
			{}

		//FIXME: is it really needed?
		[[nodiscard]]
		asio::io_context &
		io_context() const noexcept { return *(this->m_io_context); }

		[[nodiscard]]
		demands_counter_t &
		demands_counter() noexcept { return this->m_demands_counter; }

	protected :
		void
		work_started() { /* Nothing to do. */ }

		void
		work_finished() { /* Nothing to do. */ }
	};

template< typename Thread_Type >
class with_activity_tracking_impl_t : protected common_data_t< Thread_Type >
	{
		using base_type_t = common_data_t< Thread_Type >;

	public :
		with_activity_tracking_impl_t( io_context_shptr_t io_context )
			:	base_type_t( std::move(io_context) )
			{}

//FIXME: the repetition of methods io_context and demands_counter is not good.

		//FIXME: is it really needed?
		[[nodiscard]]
		asio::io_context &
		io_context() const noexcept { return *(this->m_io_context); }

		[[nodiscard]]
		demands_counter_t &
		demands_counter() noexcept { return this->m_demands_counter; }

		[[nodiscard]]
		so_5::stats::work_thread_activity_stats_t
		take_activity_stats()
			{
				so_5::stats::work_thread_activity_stats_t result;

				result.m_working_stats = m_working_stats.take_stats();

				return result;
			}

	protected :
		//! Statictics for work activity.
		so_5::stats::activity_tracking_stuff::stats_collector_t<
				so_5::stats::activity_tracking_stuff::internal_lock >
			m_working_stats;

		void
		work_started() { m_working_stats.start(); }

		void
		work_finished() { m_working_stats.stop(); }
	};

} /* namespace work_thread_details */

//
// work_thread_template_t
//
template<
	typename Thread_Type,
	template<class> class Work_Thread >
class work_thread_template_t
	:	public Work_Thread< Thread_Type >
	,	public event_queue_t
	{
		using base_type_t = Work_Thread< Thread_Type >;

		//! SObjectizer Environment to work in.
		/*!
		 * We have to store that reference to have an ability to
		 * log error messages in thread's body.
		 */
		environment_t & m_env;

	public :
		//! Initializing constructor.
		work_thread_template_t(
			environment_t & env,
			io_context_shptr_t io_context )
			:	base_type_t( std::move(io_context) )
			,	m_env( env )
			{}

		void
		start()
			{
				//FIXME: is it a good requirement for custom thread types?
				//FIXME: has asio_thread_pool dispatcher the same requirement?
				this->m_thread = Thread_Type( [this]() { body(); } );
			}

		void
		stop()
			{
				this->io_context().stop();
			}

		void
		join()
			{
				so_5::impl::ensure_join_from_different_thread( this->m_thread_id );
				this->m_thread.join();
			}

		//FIXME: is this method really needed?
		so_5::current_thread_id_t
		thread_id() const
			{
				return this->m_thread_id;
			}

		void
		push( execution_demand_t demand ) override
			{
				++(this->demands_counter());

				// If posting a demand fails the count of demands
				// should be decremented.
				::so_5::details::do_with_rollback_on_exception(
						[&] {
							asio::post( this->io_context(),
								[d = std::move(demand), this]() mutable {
									this->handle_demand( std::move(d) );
								} );
						},
						[this] {
							--this->demands_counter();
						} );
			}

	private :
		void
		body()
			{
				this->m_thread_id = so_5::query_current_thread_id();

				// We don't expect any errors here.
				// But if something happens then there is no way to
				// recover and the whole application should be aborted.
				try
					{
						// Prevent return from io_context::run() if there is no
						// more Asio's events.
						auto work = ::asio::make_work_guard( this->io_context() );
						this->io_context().run();
					}
				catch( const std::exception & x )
					{
						::so_5::details::abort_on_fatal_error( [&] {
							SO_5_LOG_ERROR( this->m_env, log_stream ) {
								log_stream << "An exception caught in work thread "
									"of so_5::extra::disp::asio_one_thread dispatcher."
									" Exception: "
									<< x.what() << std::endl;
							}
						} );
					}
				catch( ... )
					{
						::so_5::details::abort_on_fatal_error( [&] {
							SO_5_LOG_ERROR( this->m_env, log_stream ) {
								log_stream << "An unknown exception caught in work thread "
									"of so_5::extra::disp::asio_one_thread dispatcher."
									<< std::endl;
							}
						} );
					}
			}

		void
		handle_demand( execution_demand_t demand ) noexcept
			{
				--(this->demands_counter());

				this->work_started();
				auto work_meter_stopper = so_5::details::at_scope_exit(
						[this] { this->work_finished(); } );

				demand.call_handler( this->m_thread_id );
			}
	};

//
// work_thread_no_activity_tracking_t
//
template< typename Thread_Type >
using work_thread_no_activity_tracking_t =
	work_thread_template_t<
			Thread_Type,
			work_thread_details::no_activity_tracking_impl_t >;

template< typename Thread_Type >
void
send_thread_activity_stats(
	const so_5::mbox_t &,
	const so_5::stats::prefix_t &,
	work_thread_no_activity_tracking_t< Thread_Type > & )
	{
		/* Nothing to do */
	}

//
// work_thread_with_activity_tracking_t
//
template< typename Thread_Type >
using work_thread_with_activity_tracking_t =
	work_thread_template_t<
			Thread_Type,
			work_thread_details::with_activity_tracking_impl_t >;

template< typename Thread_Type >
void
send_thread_activity_stats(
	const so_5::mbox_t & mbox,
	const so_5::stats::prefix_t & prefix,
	work_thread_with_activity_tracking_t< Thread_Type > & wt )
	{
		so_5::send< so_5::stats::messages::work_thread_activity >(
				mbox,
				prefix,
				so_5::stats::suffixes::work_thread_activity(),
				wt.thread_id(),
				wt.take_activity_stats() );
	}

//
// dispatcher_template_t
//
//FIXME: document this!
template< typename Work_Thread >
class dispatcher_template_t final : public actual_disp_binder_t
	{
		friend class disp_data_source_t;

	public:
		dispatcher_template_t(
			outliving_reference_t< environment_t > env,
			const std::string_view name_base,
			disp_params_t params )
			:	m_work_thread{ env.get(), params.io_context() }
			,	m_data_source{
					outliving_mutable(env.get().stats_repository()),
					name_base,
					outliving_mutable(*this)
				}
			{
				m_work_thread.start();
			}

		~dispatcher_template_t() noexcept override
			{
				m_work_thread.stop();
				m_work_thread.join();
			}

		void
		preallocate_resources(
			agent_t & /*agent*/ ) override
			{
				// Nothing to do.
			}

		void
		undo_preallocation(
			agent_t & /*agent*/ ) noexcept override
			{
				// Nothing to do.
			}

		void
		bind(
			agent_t & agent ) noexcept override
			{
				agent.so_bind_to_dispatcher( m_work_thread );
				++m_agents_bound;
			}

		void
		unbind(
			agent_t & /*agent*/ ) noexcept override
			{
				--m_agents_bound;
			}

		[[nodiscard]]
		asio::io_context &
		io_context() noexcept override
			{
				return m_work_thread.io_context();
			}

	private:

		/*!
		 * \brief Data source for run-time monitoring of whole dispatcher.
		 *
		 * \since
		 * v.5.5.8
		 */
		class disp_data_source_t : public stats::source_t
			{
				//! Dispatcher to work with.
				outliving_reference_t< dispatcher_template_t > m_dispatcher;

				//! Basic prefix for data sources.
				stats::prefix_t m_base_prefix;

			public :
				disp_data_source_t(
					const std::string_view name_base,
					outliving_reference_t< dispatcher_template_t > disp )
					:	m_dispatcher{ disp }
					,	m_base_prefix{ so_5::disp::reuse::make_disp_prefix(
								"ext-asio-ot",
								name_base,
								&(disp.get()) )
						}
					{}

				void
				distribute( const mbox_t & mbox ) override
					{
						so_5::send< stats::messages::quantity< std::size_t > >(
								mbox,
								this->m_base_prefix,
								stats::suffixes::agent_count(),
								m_dispatcher.get().m_agents_bound.load(
										std::memory_order_acquire ) );

						so_5::send< stats::messages::quantity< std::size_t > >(
								mbox,
								this->m_base_prefix,
								stats::suffixes::work_thread_queue_size(),
								m_dispatcher.get().m_work_thread.demands_counter().load(
										std::memory_order_acquire ) );

						send_thread_activity_stats(
								mbox,
								this->m_base_prefix,
								m_dispatcher.get().m_work_thread );
					}

			private:
			};

		//! Working thread for the dispatcher.
		Work_Thread m_work_thread;

		//! Data source for run-time monitoring.
		stats::auto_registered_source_holder_t< disp_data_source_t >
				m_data_source;

		//! Count of agents bound to this dispatcher.
		std::atomic< std::size_t > m_agents_bound = { 0 };
	};

//
// dispatcher_handle_maker_t
//
class dispatcher_handle_maker_t
	{
	public :
		[[nodiscard]]
		static dispatcher_handle_t
		make( actual_disp_binder_shptr_t binder ) noexcept
			{
				return { std::move( binder ) };
			}
	};

} /* namespace impl */

//
// default_traits_t
//
/*!
 * \brief Default traits of %asio_one_thread dispatcher.
 *
 * \since
 * v.1.4.1
 */
struct default_traits_t
	{
		//! Type of thread.
		using thread_type = std::thread;
	};

//
// make_dispatcher
//
//FIXME: document this!
template< typename Traits = default_traits_t >
dispatcher_handle_t
make_dispatcher(
	environment_t & env,
	const std::string_view data_sources_name_base,
	disp_params_t params )
	{
		using namespace so_5::disp::reuse;

		const auto io_svc_ptr = params.io_context();
		if( !io_svc_ptr )
			SO_5_THROW_EXCEPTION(
					errors::rc_io_context_is_not_set,
					"io_context is not set in disp_params" );

		using thread_type = typename Traits::thread_type;

		using dispatcher_no_activity_tracking_t =
				impl::dispatcher_template_t<
						impl::work_thread_no_activity_tracking_t< thread_type > >;

		using dispatcher_with_activity_tracking_t =
				impl::dispatcher_template_t<
						impl::work_thread_with_activity_tracking_t< thread_type > >;

		using so_5::stats::activity_tracking_stuff::create_appropriate_disp;
		impl::actual_disp_binder_shptr_t binder =
				create_appropriate_disp<
						// Type of result pointer.
						impl::actual_disp_binder_t,
						// Actual type of dispatcher without thread activity tracking.
						dispatcher_no_activity_tracking_t,
						// Actual type of dispatcher with thread activity tracking.
						dispatcher_with_activity_tracking_t >(
					outliving_mutable(env),
					data_sources_name_base,
					std::move(params) );

		return impl::dispatcher_handle_maker_t::make( std::move(binder) );
	}

} /* namespace asio_one_thread */

} /* namespace disp */

} /* namespace extra */

} /* namespace so_5 */