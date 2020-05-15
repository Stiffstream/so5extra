/*!
 * \file
 * \brief Implementation of Asio's Thread Pool dispatcher.
 *
 * \since
 * v.1.0.2
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

#include <so_5/outliving.hpp>

#include <asio/io_context.hpp>
#include <asio/io_context_strand.hpp>
#include <asio/post.hpp>

namespace so_5 {

namespace extra {

namespace disp {

namespace asio_thread_pool {

namespace errors {

//! Asio IoService is not set for asio_thread_pool dispatcher.
const int rc_io_context_is_not_set =
		so_5::extra::errors::asio_thread_pool_errors;

} /* namespace errors */

//
// disp_params_t
//
/*!
 * \brief Parameters for %asio_thread_pool dispatcher.
 *
 * \since
 * v.1.0.2
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

				swap( a.m_thread_count, b.m_thread_count );
				swap( a.m_io_context, b.m_io_context );
			}

		//! Setter for thread count.
		disp_params_t &
		thread_count( std::size_t count )
			{
				m_thread_count = count;
				return *this;
			}

		//! Getter for thread count.
		std::size_t
		thread_count() const
			{
				return m_thread_count;
			}

		//! Use external Asio io_context object with dispatcher.
		/*!
		 * Usage example:
		 * \code
		 * int main() {
		 * 	asio::io_context svc;
		 * 	so_5::launch( [&](so_5::environment_t & env) {
		 * 		namespace asio_tp = so_5::extra::disp::asio_thread_pool;
		 * 		auto disp = asio_tp::create_private_disp(
		 * 				env, "asio_tp",
		 * 				asio_tp::disp_params_t{}.use_external_io_context(
		 * 					so_5::outliving_mutable(svc) ) );
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
		//! Count of working threads.
		/*!
		 * Value 0 means that actual thread will be detected automatically.
		 */
		std::size_t m_thread_count = { 0 };

		//! Asio's io_context which must be used with this dispatcher.
		std::shared_ptr< ::asio::io_context > m_io_context;
	};

namespace impl {

class actual_dispatcher_iface_t;

//
// basic_dispatcher_iface_t
//
/*!
 * \brief The very basic interface of %asio_thread_pool dispatcher.
 *
 * This class contains a minimum that is necessary for implementation
 * of dispatcher_handle class.
 *
 * \since
 * v.1.3.0
 */
class basic_dispatcher_iface_t
	:	public std::enable_shared_from_this<actual_dispatcher_iface_t>
	{
	public :
		virtual ~basic_dispatcher_iface_t() noexcept = default;

		//! Create a binder for that dispatcher.
		/*!
		 * The binder will use an external strand object.
		 */
		[[nodiscard]]
		virtual disp_binder_shptr_t
		binder_with_external_strand( ::asio::io_context::strand & ) = 0;

		//! Create a binder for that dispatcher.
		/*!
		 * The binder will use an internal (automatically created)
		 * strand object.
		 */
		[[nodiscard]]
		virtual disp_binder_shptr_t
		binder_with_own_strand() = 0;

		//! Get reference to io_context from that dispatcher.
		virtual ::asio::io_context &
		io_context() const noexcept = 0;
	};

using basic_dispatcher_iface_shptr_t =
		std::shared_ptr< basic_dispatcher_iface_t >;

class dispatcher_handle_maker_t;

} /* namespace impl */

//
// dispatcher_handle_t
//

/*!
 * \brief A handle for %asio_thread_pool dispatcher.
 *
 * \since
 * v.1.3.0
 */
class [[nodiscard]] dispatcher_handle_t
	{
		friend class impl::dispatcher_handle_maker_t;

		//! A reference to actual implementation of a dispatcher.
		impl::basic_dispatcher_iface_shptr_t m_dispatcher;

		dispatcher_handle_t(
			impl::basic_dispatcher_iface_shptr_t dispatcher ) noexcept
			:	m_dispatcher{ std::move(dispatcher) }
			{}

		//! Is this handle empty?
		[[nodiscard]]
		bool
		empty() const noexcept { return !m_dispatcher; }

	public :
		dispatcher_handle_t() noexcept = default;

		//! Get a binder for that dispatcher.
		/*!
		 * This method requires a reference to manually created strand
		 * object for protection of agents bound via binder returned.
		 * A user should create this strand object and ensure the right
		 * lifetime of it.
		 *
		 * Usage example:
		 * \code
		 * using namespace so_5::extra::disp::asio_thread_pool;
		 *
		 * asio::io_context io_ctx;
		 * asio::io_context::strand agents_strand{ io_ctx };
		 *
		 * so_5::environment_t & env = ...;
		 * auto disp = make_dispatcher( env, "my_disp", io_ctx );
		 *
		 * env.introduce_coop( [&]( so_5::coop_t & coop ) {
		 * 	coop.make_agent_with_binder< some_agent_type >(
		 * 		disp.binder( agents_strand ),
		 * 		... );
		 *
		 * 	coop.make_agent_with_binder< another_agent_type >(
		 * 		disp.binder( agents_strand ),
		 * 		... );
		 *
		 * 	...
		 * } );
		 * \endcode
		 *
		 * \attention
		 * An attempt to call this method on empty handle is UB.
		 */
		[[nodiscard]]
		disp_binder_shptr_t
		binder(
			::asio::io_context::strand & strand ) const
			{
				return m_dispatcher->binder_with_external_strand( strand );
			}

		//! Get a binder for that dispatcher.
		/*!
		 * This method requires creates an internal strand object by itself.
		 *
		 * Usage example:
		 * \code
		 * using namespace so_5::extra::disp::asio_thread_pool;
		 *
		 * asio::io_context io_ctx;
		 *
		 * so_5::environment_t & env = ...;
		 * auto disp = make_dispatcher( env, "my_disp", io_ctx );
		 *
		 * env.introduce_coop( [&]( so_5::coop_t & coop ) {
		 * 	// This agent will use its own strand object.
		 * 	coop.make_agent_with_binder< some_agent_type >(
		 * 		disp.binder(),
		 * 		... );
		 *
		 * 	// This agent will use its own strand object.
		 * 	coop.make_agent_with_binder< another_agent_type >(
		 * 		disp.binder(),
		 * 		... );
		 * 	...
		 * } );
		 * \endcode
		 *
		 * \attention
		 * An attempt to call this method on empty handle is UB.
		 *
		 * \since
		 * v.1.3.0
		 */
		[[nodiscard]]
		disp_binder_shptr_t
		binder() const
			{
				return m_dispatcher->binder_with_own_strand();
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
				return m_dispatcher->io_context();
			}

		//! Is this handle empty?
		[[nodiscard]]
		operator bool() const noexcept { return empty(); }

		//! Does this handle contain a reference to dispatcher?
		[[nodiscard]]
		bool
		operator!() const noexcept { return !empty(); }

		//! Drop the content of handle.
		void
		reset() noexcept { m_dispatcher.reset(); }
	};

namespace impl {

//
// demands_counter_t
//
/*!
 * \brief Type of atomic counter for counting waiting demands.
 *
 * \since
 * v.1.0.2
 */
using demands_counter_t = std::atomic< std::size_t >;

//
// actual_dispatcher_iface_t
//
/*!
 * \brief An actual interface of thread pool dispatcher.
 *
 * \since
 * v.1.3.0
 */
class actual_dispatcher_iface_t : public basic_dispatcher_iface_t
	{
	public :
		//! Notification about binding of yet another agent.
		virtual void
		agent_bound() noexcept = 0;

		//! Notification about unbinding of an agent.
		virtual void
		agent_unbound() noexcept = 0;

		//! Get a reference for counter of pending demands.
		virtual demands_counter_t &
		demands_counter() noexcept = 0;
	};

//
// actual_dispatcher_shptr_t
//
using actual_dispatcher_shptr_t =
		std::shared_ptr< actual_dispatcher_iface_t >;

//
// thread_local_ptr_holder_t
//
/*!
 * \brief A helper for declaration of static and thread_local pointer
 * in a header file.
 *
 * If non-template class will define a static member in a header file
 * then there is a possibility to get a link-time error about multiple
 * definition of that member. But if a static member is defined for
 * template class then there won't be such problem.
 *
 * A typical usage intended to be:
 * \code
 * class some_useful_class_t : public thread_local_ptr_holder_t<some_useful_class_t> {
 * 	...
 * };
 * \endcode
 *
 * \since
 * v.1.0.2
 */
template< class T >
class thread_local_ptr_holder_t
	{
	private :
		//! Value of the pointer which need to be stored.
		static thread_local T * m_ptr;

	protected :
		//! Access to the current value of the pointer.
		static T *
		ptr() noexcept { return m_ptr; }

		//! Setter for the pointer.
		static void
		set_ptr( T * p ) noexcept { m_ptr = p; }
	};

template< class T >
thread_local T * thread_local_ptr_holder_t<T>::m_ptr = nullptr;

//
// work_thread_t
//
/*!
 * \brief Base type for implementations of work thread wrappers.
 *
 * Work thread wrapper creates an instance of some type on the stack
 * of the new thread. Then the pointer of this instance is stored in
 * thread_local variable (as a pointer to work_thread_t). This pointer
 * then can be retrieved later by demand handlers to get access to
 * some dispatcher-specific data.
 *
 * It is assumed that there will be two derived classes:
 * 1. One for the case when thread activity should not be tracked.
 * 2. Another for the case when thread activity must be tracked.
 *
 * These derived classes will reuse some functionality from
 * work_thread_t. And should implement on_demand() method for
 * actual demands processing.
 *
 * \since
 * v.1.0.2
 */
class work_thread_t : private thread_local_ptr_holder_t< work_thread_t >
	{
	private :
		//! ID of the work thread.
		/*!
		 * Gets its value in the constructor and doesn't changed later.
		 */
		current_thread_id_t m_thread_id;

	protected :
		// Constructor and destructor are accessible for derived classes only.
		work_thread_t()
			: m_thread_id( query_current_thread_id() )
			{}

		// Just to make compilers happy.
		virtual ~work_thread_t() = default;

		//! Actual processing of the demand.
		/*!
		 * Must be implemented in derived classes.
		 */
		virtual void
		on_demand( execution_demand_t demand ) noexcept = 0;

		//! ID of the work thread.
		current_thread_id_t
		thread_id() const noexcept
			{
				return m_thread_id;
			}

	public :
		//! Lauch processing of demand on the context of current thread.
		/*!
		 * Creates an instance of Derived class, stores pointer to it into
		 * a thread_local static variable, then calls io_svc.run() method.
		 *
		 * \attention
		 * Terminates the whole application if an exception will be thrown.
		 *
		 * \tparam Derived Type of an object to be created on the stack.
		 * \tparam Args Types of arguments for Derived's constructor.
		 */
		template< typename Derived, typename... Args >
		static void
		run(
			//! SObjectizer Environment for which work thread was created.
			environment_t & env,
			//! Asio IoService to be run on the context of that thread.
			::asio::io_context & io_svc,
			//! Arguments to Derived's constructor.
			Args &&... args )
			{
				// We don't expect any errors here.
				// But if something happens then there is no way to
				// recover and the whole application should be aborted.
				try
					{
						Derived actual_handler{ std::forward<Args>(args)... };
						// actual_handler must be accessible via thread_local variable.
						set_ptr( &actual_handler );

						// Prevent return from io_context::run() if there is no
						// more Asio's events.
						auto work = ::asio::make_work_guard( io_svc );
						io_svc.run();
					}
				catch( const std::exception & x )
					{
						::so_5::details::abort_on_fatal_error( [&] {
							SO_5_LOG_ERROR( env, log_stream ) {
								log_stream << "An exception caught in work thread "
									"of so_5::extra::disp::asio_thread_pool dispatcher."
									" Exception: "
									<< x.what() << std::endl;
							}
						} );
					}
				catch( ... )
					{
						::so_5::details::abort_on_fatal_error( [&] {
							SO_5_LOG_ERROR( env, log_stream ) {
								log_stream << "An unknown exception caught in work thread "
									"of so_5::extra::disp::asio_thread_pool dispatcher."
									<< std::endl;
							}
						} );
					}
			}

		//! An interface method for passing a demand to processing.
		static void
		handle_demand( execution_demand_t demand )
			{
				ptr()->on_demand( std::move(demand) );
			}
	};

//
// work_thread_without_activity_tracking_t
//
/*!
 * \brief An implementation of work thread stuff for the case when
 * thread activity tracking is not needed.
 *
 * \since
 * v.1.0.2
 */
class work_thread_without_activity_tracking_t final : public work_thread_t
	{
	public :
		work_thread_without_activity_tracking_t() = default;
		~work_thread_without_activity_tracking_t() override = default;

	protected :
		virtual void
		on_demand( execution_demand_t demand ) noexcept override
			{
				demand.call_handler( thread_id() );
			}
	};

//
// work_thread_activity_collector_t
//
/*!
 * \brief Type of collector of work thread activity data.
 *
 * Objects of this class store also an ID of work thread. This ID is
 * necessary for so_5::stats::messages::work_thread_activity message.
 * Because of that a work thread must call setup_thread_id() method
 * before use of activity collector.
 *
 * \since
 * v.1.0.2
 */
class work_thread_activity_collector_t
	{
	private :
		//! ID of thread for which activity stats is collected.
		current_thread_id_t m_thread_id;

		//! Collected activity stats.
		::so_5::stats::activity_tracking_stuff::stats_collector_t<
					::so_5::stats::activity_tracking_stuff::internal_lock >
				m_work_activity;

	public :
		/*!
		 * \brief Setup ID of the current work thread.
		 *
		 * \attention
		 * Must be called as soon as possible after the start of the work thread.
		 */
		void
		setup_thread_id( current_thread_id_t tid )
			{
				m_thread_id = std::move(tid);
			}

		/*!
		 * \brief Get the ID of the thread.
		 *
		 * \attention
		 * Returns actual value only after call to setup_thread_id.
		 */
		current_thread_id_t
		thread_id() const noexcept { return m_thread_id; }

		/*!
		 * \brief Mark start point of new activity.
		 */
		void
		activity_started() noexcept
			{
				m_work_activity.start();
			}

		/*!
		 * \brief Mark completion of the current activity.
		 */
		void
		activity_finished() noexcept
			{
				m_work_activity.stop();
			}

		/*!
		 * \brief Get the current stats.
		 */
		::so_5::stats::work_thread_activity_stats_t
		take_activity_stats() noexcept
		{
			::so_5::stats::work_thread_activity_stats_t result;
			result.m_working_stats = m_work_activity.take_stats();

			return result;
		}
	};

//
// work_thread_with_activity_tracking_t
//
/*!
 * \brief An implementation of work thread stuff for the case when
 * thread activity tracking must be used.
 *
 * \since
 * v.1.0.2
 */
class work_thread_with_activity_tracking_t final : public work_thread_t
	{
	private :
		//! Activity statistics.
		outliving_reference_t< work_thread_activity_collector_t > m_activity_stats;

	public :
		work_thread_with_activity_tracking_t(
			outliving_reference_t< work_thread_activity_collector_t > activity_stats )
			:	m_activity_stats(activity_stats)
			{
				// Collector must receive ID of this thread.
				m_activity_stats.get().setup_thread_id( thread_id() );
			}

		~work_thread_with_activity_tracking_t() override = default;

	protected :
		virtual void
		on_demand( execution_demand_t demand ) noexcept override
			{
				m_activity_stats.get().activity_started();

				demand.call_handler( thread_id() );

				m_activity_stats.get().activity_finished();
			}
	};

//
// class basic_binder_impl_t
//
/*!
 * \brief Basic part of implementation of a binder for %asio_thread_pool
 * dispatcher.
 *
 * \since
 * v.1.3.0
 */
class basic_binder_impl_t
	:	public disp_binder_t
	,	public event_queue_t
	{
	public :
		//! Initializing constructor.
		basic_binder_impl_t(
			//! The actual dispatcher to be used with that binder.
			actual_dispatcher_shptr_t dispatcher )
			:	m_dispatcher{ std::move(dispatcher) }
			{}

		void
		preallocate_resources(
			agent_t & /*agent*/ ) override
			{
				// There is no need to do something.
			}

		void
		undo_preallocation(
			agent_t & /*agent*/ ) noexcept override
			{
				// There is no need to do something.
			}

		void
		bind(
			agent_t & agent ) noexcept override
			{
				// Dispatcher should know about yet another agent bound.
				m_dispatcher->agent_bound();
				// Agent should receive its event_queue.
				agent.so_bind_to_dispatcher( *this );
			}

		void
		unbind(
			agent_t & /*agent*/ ) noexcept override
			{
				// Dispatcher should know about yet another agent unbound.
				m_dispatcher->agent_bound();
			}

	protected :
		//! The actual dispatcher.
		actual_dispatcher_shptr_t m_dispatcher;
	};

//
// binder_template_t
//
/*!
 * \brief An implementation of a binder for %asio_thread_pool dispatcher.
 *
 * This binder is also an event_queue for the agents bound via that binder.
 *
 * There is no such thing as event_queue for %asio_thread_pool dispacher.
 * All execution demands will be stored inside Asio IoServce and dispatched
 * for execution via asio::post mechanism. But SObjectizer requires
 * an implementation of event_queue which must be used for agents bound
 * to %asio_thread_pool dispatcher. This class implements this event_queue
 * concepts.
 *
 * This templates implements CRTP and should be parametrized by
 * derived type. The derived type should provide method:
 * \code
 * ::asio::io_context::strand & strand() noexcept;
 * \endcode
 *
 * \since
 * v.1.3.0
 */
template< typename Derived >
class binder_template_t
	:	public basic_binder_impl_t
	{
		auto &
		self_reference() noexcept
			{
				return static_cast< Derived & >( *this );
			}

	public :
		using basic_binder_impl_t::basic_binder_impl_t;

		void
		push( execution_demand_t demand ) override
			{
				demands_counter_t & counter = m_dispatcher->demands_counter();

				// Another demand will wait for processing.
				++counter;

				asio::post( self_reference().strand(),
					[d = std::move(demand), &counter]() mutable {
						// Another demand will be processed.
						--counter;

						// Delegate processing of the demand to actual
						// work thread.
						work_thread_t::handle_demand( std::move(d) );
					} );
			}
	};

//
// binder_with_external_strand_t
//
/*!
 * \brief An implementation of binder that uses an external strand object.
 *
 * \since
 * v.1.3.0
 */
class binder_with_external_strand_t final
	: public binder_template_t< binder_with_external_strand_t >
	{
		using base_type = binder_template_t< binder_with_external_strand_t >;

	public :
		binder_with_external_strand_t(
			actual_dispatcher_shptr_t dispatcher,
			outliving_reference_t< ::asio::io_context::strand > strand )
			:	base_type{ std::move(dispatcher) }
			,	m_strand{ strand }
			{}

		::asio::io_context::strand &
		strand() noexcept { return m_strand.get(); }

	private :
		//! Strand to be used with this event_queue.
		outliving_reference_t< ::asio::io_context::strand > m_strand;
	};

//
// binder_with_own_strand_t
//
/*!
 * \brief An implementation of binder that uses an own strand object.
 *
 * This own strand object will be a part of the binder object.
 *
 * \since
 * v.1.3.0
 */
class binder_with_own_strand_t final
	: public binder_template_t< binder_with_own_strand_t >
	{
		using base_type = binder_template_t< binder_with_own_strand_t >;

	public :
		binder_with_own_strand_t(
			actual_dispatcher_shptr_t dispatcher )
			:	base_type{ std::move(dispatcher) }
			,	m_strand{ m_dispatcher->io_context() }
			{}

		::asio::io_context::strand &
		strand() noexcept { return m_strand; }

	private :
		//! Strand to be used with this event_queue.
		::asio::io_context::strand m_strand;
	};

//
// basic_dispatcher_skeleton_t
//
/*!
 * \brief Basic stuff for all implementations of dispatcher.
 *
 * Derived classes should implement the following virtual methods:
 * - data_source();
 * - launch_work_threads();
 * - wait_work_threads().
 *
 * \since
 * v.1.0.2
 */
class basic_dispatcher_skeleton_t : public actual_dispatcher_iface_t
	{
	protected :
		class disp_data_source_t;
		friend class disp_data_source_t;

	public:
		basic_dispatcher_skeleton_t(
			disp_params_t params )
			:	m_thread_count( params.thread_count() )
			,	m_io_context( params.io_context() )
			{
			}

		[[nodiscard]]
		disp_binder_shptr_t
		binder_with_external_strand(
			::asio::io_context::strand & strand ) override
			{
				return { std::make_shared< binder_with_external_strand_t >(
						shared_from_this(),
						outliving_mutable(strand) )
				};
			}

		[[nodiscard]]
		disp_binder_shptr_t
		binder_with_own_strand() override
			{
				return { std::make_shared< binder_with_own_strand_t >(
						shared_from_this() )
					};
			}

		::asio::io_context &
		io_context() const noexcept override { return *m_io_context; }

		void
		agent_bound() noexcept override
			{
				++m_agents_bound;
			}

		void
		agent_unbound() noexcept override
			{
				--m_agents_bound;
			}

		demands_counter_t &
		demands_counter() noexcept override
			{
				return m_demands_counter;
			}

	protected :
		void
		start(
			environment_t & env,
			std::string_view data_sources_name_base )
			{
				data_source().set_data_sources_name_base( data_sources_name_base );
				data_source().start( outliving_mutable(env.stats_repository()) );

				::so_5::details::do_with_rollback_on_exception(
						[&] { launch_work_threads(env); },
						[this] { data_source().stop(); } );
			}

		void
		shutdown()
			{
				::so_5::details::invoke_noexcept_code( [this] {
						// Stopping Asio IO service.
						m_io_context->stop();
					} );
			}

		void
		wait()
			{
				::so_5::details::invoke_noexcept_code( [this] {
						// Waiting for complete stop of all work threads.
						wait_work_threads();
						// Stopping data source.
						data_source().stop();
					} );
			}

	protected :
		/*!
		 * \brief Get the count of work threads to be created.
		 */
		std::size_t
		thread_count() const noexcept { return m_thread_count; }

		/*!
		 * \brief Get access to actual data source instance for that
		 * dispatcher.
		 */
		virtual disp_data_source_t &
		data_source() noexcept = 0;


#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif

		/*!
		 * \brief Data source for run-time monitoring of whole dispatcher.
		 *
		 * \since
		 * v.1.0.2
		 */
		class disp_data_source_t
			:	public ::so_5::stats::source_t
			{
				//! Dispatcher to work with.
				basic_dispatcher_skeleton_t & m_dispatcher;

				//! Basic prefix for data sources.
				::so_5::stats::prefix_t m_base_prefix;

				//! Data source repository.
				/*!
				 * Will receive actual value in start() method.
				 *
				 * \since
				 * v.1.3.0
				 */
				::so_5::stats::repository_t * m_stats_repo{ nullptr };

			protected :
				//! Access to data source prefix for derived classes.
				const ::so_5::stats::prefix_t &
				base_prefix() const noexcept { return m_base_prefix; }

			public :
				disp_data_source_t( basic_dispatcher_skeleton_t & disp )
					:	m_dispatcher( disp )
					{}

				virtual void
				distribute( const mbox_t & mbox ) override
					{
						const auto agents_count = m_dispatcher.m_agents_bound.load(
								std::memory_order_acquire );

						const auto demands_count = m_dispatcher.m_demands_counter.load(
								std::memory_order_acquire );

						send< ::so_5::stats::messages::quantity< std::size_t > >(
								mbox,
								m_base_prefix,
								::so_5::stats::suffixes::agent_count(),
								agents_count );

						// Note: because there is no way to detect on which thread a
						// demand will be handled, the total number of waiting
						// demands is destributed for the whole dispatcher.
						send< ::so_5::stats::messages::quantity< std::size_t > >(
								mbox,
								m_base_prefix,
								::so_5::stats::suffixes::work_thread_queue_size(),
								demands_count );
					}

				void
				set_data_sources_name_base(
					std::string_view name_base )
					{
						using namespace ::so_5::disp::reuse;

						m_base_prefix = make_disp_prefix(
								"ext-asio-tp",
								name_base,
								&m_dispatcher );
					}

				void
				start( ::so_5::stats::repository_t & repo )
					{
						repo.add( *this );
						m_stats_repo = &repo;
					}

				void
				stop() noexcept
					{
						m_stats_repo->remove( *this );
					}
			};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

	private:
		//! Count of work threads.
		const std::size_t m_thread_count;

		//! IO Service to work with.
		const std::shared_ptr< ::asio::io_context > m_io_context;

		//! Count of agents bound to that dispatcher.
		std::atomic< std::size_t > m_agents_bound{ 0u };

		//! Count of waiting demands.
		demands_counter_t m_demands_counter;

		//! Start all working threads.
		virtual void
		launch_work_threads(
			//! SObjectizer Environment for which threads will be created.
			environment_t & env ) = 0;

		//! Wait for finish of all threads.
		/*!
		 * It is a blocking call. The current thread will be stopped until
		 * all work thread will finish their work.
		 */
		virtual void
		wait_work_threads() noexcept = 0;
	};

//
// dispatcher_skeleton_without_thread_activity_tracking_t
//
/*!
 * \brief Extension of basic dispatcher skeleton for the case when
 * work thread activity is not collected.
 *
 * This class contains disp_data_source_t instance and implements
 * virtual method data_source() for accessing this instance.
 *
 * It also provides static method run_work_thread() which must be called
 * at the beginnig of work thread.
 *
 * \since
 * v.1.0.2
 */
class dispatcher_skeleton_without_thread_activity_tracking_t
	:	public basic_dispatcher_skeleton_t
	{
	public :
		dispatcher_skeleton_without_thread_activity_tracking_t(
			disp_params_t params )
			:	basic_dispatcher_skeleton_t( std::move(params) )
			{}

	protected :
		virtual disp_data_source_t &
		data_source() noexcept { return m_data_source; }

		//! Implementation of main function for a work thread.
		static void
		run_work_thread(
			environment_t & env,
			::asio::io_context & io_svc,
			dispatcher_skeleton_without_thread_activity_tracking_t & /*self*/,
			std::size_t /*index*/ )
			{
				work_thread_t::run< work_thread_without_activity_tracking_t >(
						env, io_svc );
			}

	private :
		//! Actual data source instance.
		disp_data_source_t m_data_source{ *this };
	};

//
// dispatcher_skeleton_with_thread_activity_tracking_t
//
/*!
 * \brief Extension of basic dispatcher skeleton for the case when
 * work thread activity must be collected.
 *
 * This class defines its own actual_disp_data_source_t type and
 * contains an instance of that type. There is also implementation
 * of data_source() virtual method for accessing this instance.
 *
 * It provides static method run_work_thread() which must be called
 * at the beginnig of work thread.
 *
 * \since
 * v.1.0.2
 */
class dispatcher_skeleton_with_thread_activity_tracking_t
	:	public basic_dispatcher_skeleton_t
	{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif
		/*!
		 * \brief Actual data source type for dispatcher with
		 * work thread activity tracking.
		 *
		 * \since
		 * v.1.0.2
		 */
		class actual_disp_data_source_t
			:	public disp_data_source_t
			{
			private :
				//! Collectors for run-time stats for every thread.
				std::vector<
							std::unique_ptr< work_thread_activity_collector_t > >
					m_collectors;

			public :
				actual_disp_data_source_t(
					basic_dispatcher_skeleton_t & disp,
					std::size_t thread_count )
					:	disp_data_source_t( disp )
					,	m_collectors( thread_count )
					{
						for( auto & c : m_collectors )
							c = std::make_unique< work_thread_activity_collector_t >();
					}

				virtual void
				distribute( const mbox_t & mbox ) override
					{
						disp_data_source_t::distribute( mbox );

						for( std::size_t i = 0; i != m_collectors.size(); ++i )
							distribute_stats_for_work_thread_at( mbox, i );
					}

				/*!
				 * \note \a index is not checked for validity!
				 */
				work_thread_activity_collector_t &
				collector_at( std::size_t index ) noexcept
					{
						return *(m_collectors[index]);
					}

			private :
				void
				distribute_stats_for_work_thread_at(
					const mbox_t & mbox,
					std::size_t index )
					{
						std::ostringstream ss;
						ss << base_prefix().c_str() << "/wt-" << index;

						const ::so_5::stats::prefix_t prefix{ ss.str() };
						auto & collector = collector_at( index );

						so_5::send< ::so_5::stats::messages::work_thread_activity >(
								mbox,
								prefix,
								::so_5::stats::suffixes::work_thread_activity(),
								collector.thread_id(),
								collector.take_activity_stats() );
					}
			};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

	public :
		dispatcher_skeleton_with_thread_activity_tracking_t(
			disp_params_t params )
			:	basic_dispatcher_skeleton_t( params )
			,	m_actual_data_source( *this, params.thread_count() )
			{}

	protected :
		virtual disp_data_source_t &
		data_source() noexcept override { return m_actual_data_source; }

		//! Implementation of main function for a work thread.
		static void
		run_work_thread(
			//! SObjectizer Environment for which the work thread is created.
			environment_t & env,
			//! Asio IoService to be used.
			::asio::io_context & io_svc,
			//! Dispatcher who owns this thread.
			dispatcher_skeleton_with_thread_activity_tracking_t & self,
			//! Ordinal number of this thread.
			std::size_t index )
			{
				work_thread_t::run< work_thread_with_activity_tracking_t >(
						env,
						io_svc,
						outliving_mutable(
								self.m_actual_data_source.collector_at(index) ) );
			}

	private :
		//! Data source instance.
		actual_disp_data_source_t m_actual_data_source;
	};

//
// dispatcher_template_t
//
/*!
 * \brief Template-based implementation of dispatcher.
 *
 * Implements virual methods launch_work_threads() and wait_work_threads()
 * from basic_dispatcher_skeleton_t.
 *
 * \tparam Traits Traits-type to be used.
 * \tparam Basic_Skeleton A specific skeleton to be used as base type.
 * It expected to be dispatcher_skeleton_with_thread_activity_tracking_t or
 * dispatcher_skeleton_without_thread_activity_tracking_t.
 *
 * \since
 * v.1.0.2
 */
template<
	typename Traits,
	typename Basic_Skeleton >
class dispatcher_template_t final : public Basic_Skeleton
	{
	public:
		dispatcher_template_t(
			//! SObjectizer Environment to work in.
			outliving_reference_t< environment_t > env,
			//! Value for creating names of data sources for
			//! run-time monitoring.
			std::string_view data_sources_name_base,
			//! Parameters for the dispatcher.
			disp_params_t params )
			:	Basic_Skeleton{ std::move(params) }
			{
				this->start( env.get(), data_sources_name_base );
			}

		~dispatcher_template_t() noexcept override
			{
				this->shutdown();
				this->wait();
			}

	private:
		//! An alias for actual thread type.
		using thread_t = typename Traits::thread_type;
		//! An alias for unique_ptr to thread.
		using thread_unique_ptr_t = std::unique_ptr< thread_t >;

		//! Working threads.
		std::vector< thread_unique_ptr_t > m_threads;

		virtual void
		launch_work_threads(
			environment_t & env ) override
			{
				using namespace std;

				m_threads.resize( this->thread_count() );

				::so_5::details::do_with_rollback_on_exception( [&] {
						for( std::size_t i = 0u; i != this->thread_count(); ++i )
							{
								m_threads[ i ] = this->make_work_thread( env, i );
							}
					},
					[&] {
						::so_5::details::invoke_noexcept_code( [&] {
							this->io_context().stop();

							// Shutdown all started threads.
							for( auto & t : m_threads )
								if( t )
									{
										t->join();
										t.reset();
									}
								else
									// No more started threads.
									break;
						} );
					} );
			}

		virtual void
		wait_work_threads() noexcept override 
			{
				for( auto & t : m_threads )
					{
						t->join();
						t.reset();
					}
			}

		thread_unique_ptr_t
		make_work_thread(
			environment_t & env,
			std::size_t index )
			{
				Basic_Skeleton * self = this;
				return std::make_unique< thread_t >(
					[&env, io_svc = &this->io_context(), self, index]()
					{
						Basic_Skeleton::run_work_thread( env, *io_svc, *self, index );
					} );
			}
	};

//
// dispatcher_handle_maker_t
//
class dispatcher_handle_maker_t
	{
	public :
		[[nodiscard]]
		static dispatcher_handle_t
		make( actual_dispatcher_shptr_t disp ) noexcept
			{
				return { std::move( disp ) };
			}
	};

} /* namespace impl */

//
// default_thread_pool_size
//
/*!
 * \brief A helper function for detecting default thread count for
 * thread pool.
 *
 * \since
 * v.1.0.2
 */
inline std::size_t
default_thread_pool_size()
	{
		auto c = std::thread::hardware_concurrency();
		if( !c )
			c = 2;

		return c;
	}

//
// default_traits_t
//
/*!
 * \brief Default traits of %asio_thread_pool dispatcher.
 *
 * \since
 * v.1.0.2
 */
struct default_traits_t
	{
		//! Type of thread.
		using thread_type = std::thread;
	};

//
// make_dispatcher
//
/*!
 * \brief A function for creation an instance of %asio_thread_pool dispatcher.
 *
 * Usage examples:
 * \code
 * // Dispatcher which uses own Asio IoContext and default traits.
 * namespace asio_tp = so_5::extra::disp::asio_thread_pool;
 * asio_tp::disp_params_t params;
 * params.use_own_io_context(); // Asio IoContext object will be created here.
 * 		// This object will be accessible later via
 * 		// dispatcher_handle_t::io_context() method.
 * auto disp = asio_tp::make_dispatcher(
 * 	env,
 * 	"my_asio_tp",
 * 	std::move(disp_params) );
 *
 *
 * // Dispatcher which uses external Asio IoContext and default traits.
 * asio::io_context & io_svc = ...;
 * namespace asio_tp = so_5::extra::disp::asio_thread_pool;
 * asio_tp::disp_params_t params;
 * params.use_external_io_context( io_svc );
 * auto disp = asio_tp::make_dispatcher(
 * 	env,
 * 	"my_asio_tp",
 * 	std::move(disp_params) );
 *
 *
 * // Dispatcher which uses own Asio IoContext and custom traits.
 * struct my_traits
 * {
 * 	using thread_type = my_custom_thread_type;
 * };
 * namespace asio_tp = so_5::extra::disp::asio_thread_pool;
 * asio_tp::disp_params_t params;
 * params.use_own_io_context();
 * auto disp = asio_tp::make_dispatcher< my_traits >(
 * 	env,
 * 	"my_asio_tp",
 * 	std::move(disp_params) );
 * \endcode
 *
 * \par Requirements for traits type
 * Traits type must define a type which looks like:
 * \code
 * struct traits
 * {
 * 	// Name of type to be used for thread class.
 * 	using thread_type = ...;
 * };
 * \endcode
 *
 * \par Requirements for custom thread type
 * By default std::thread is used as a class for working with threads.
 * But user can specify its own custom thread type via \a Traits::thread_type
 * parameter. A custom thread type must be a class which looks like:
 * \code
 * class custom_thread_type {
 * public :
 * 	// Must provide this constructor.
 * 	// F -- is a type of functional object which can be converted
 * 	// into std::function<void()>.
 * 	template<typename F>
 * 	custom_thread_type(F && f) {...}
 *
 * 	// Destructor must join thread if it is not joined yet.
 * 	~custom_thread_type() noexcept {...}
 *
 * 	// The same semantic like std::thread::join.
 * 	void join() noexcept {...}
 * };
 * \endcode
 * This class doesn't need to be DefaultConstructible, CopyConstructible,
 * MoveConstructible, Copyable or Moveable.
 *
 * \tparam Traits Type with traits for a dispatcher. For the requirements
 * for \a Traits type see the section "Requirements for traits type" above.
 *
 * \since
 * v.1.0.2
 */
template< typename Traits = default_traits_t >
[[nodiscard]]
inline dispatcher_handle_t
make_dispatcher(
	//! SObjectizer Environment to work in.
	environment_t & env,
	//! Value for creating names of data sources for
	//! run-time monitoring.
	const std::string_view data_sources_name_base,
	//! Parameters for the dispatcher.
	disp_params_t disp_params )
	{
		const auto io_svc_ptr = disp_params.io_context();
		if( !io_svc_ptr )
			SO_5_THROW_EXCEPTION(
					errors::rc_io_context_is_not_set,
					"io_context is not set in disp_params" );

		if( !disp_params.thread_count() )
			disp_params.thread_count( default_thread_pool_size() );

		using so_5::stats::activity_tracking_stuff::create_appropriate_disp;
		auto disp = create_appropriate_disp<
				// Type of result pointer.
				impl::actual_dispatcher_iface_t,
				// Actual type of dispatcher without thread activity tracking.
				impl::dispatcher_template_t<
						Traits,
						impl::dispatcher_skeleton_without_thread_activity_tracking_t >,
				// Actual type of dispatcher with thread activity tracking.
				impl::dispatcher_template_t<
						Traits,
						impl::dispatcher_skeleton_with_thread_activity_tracking_t > >(
			outliving_mutable(env),
			data_sources_name_base,
			std::move(disp_params) );

		return impl::dispatcher_handle_maker_t::make( std::move(disp) );
	}

} /* namespace asio_thread_pool */

} /* namespace disp */

} /* namespace extra */

} /* namespace so_5 */


