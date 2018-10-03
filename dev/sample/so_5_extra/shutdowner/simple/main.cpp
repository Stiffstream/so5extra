/*
 * A very simple example of usage so_5::extra::shutdowner
 */

#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

//
// An agent that will initiate shutdown.
//
class shutdown_initiator_t final : public so_5::agent_t
{
	// Two states are required for that agent:
	// - the first will be used for waiting for shutdown time;
	// - the second will be used for wait the completion of shutdown.
	state_t st_wait_shutdown_start{ this };
	state_t st_wait_shutdown_finish{ this };

	// This signal will be used for timer counter (it will be a periodic
	// signal which is repeated every second).
	struct tick final : public so_5::signal_t {};

public :
	shutdown_initiator_t( context_t ctx )
		: so_5::agent_t( std::move(ctx) )
	{
		this >>= st_wait_shutdown_start;

		st_wait_shutdown_start.event( &shutdown_initiator_t::on_tick_1 );
		st_wait_shutdown_finish.event( &shutdown_initiator_t::on_tick_2 );
	}

	virtual void so_evt_start() override
	{
		// Start time counting.
		m_timer = so_5::send_periodic< tick >( *this,
				std::chrono::seconds::zero(),
				std::chrono::seconds(1) );
	}

	virtual void so_evt_finish() override
	{
		std::cout << "Application finally finishes..." << std::endl;
	}

private :
	so_5::timer_id_t m_timer;
	int m_counter = 3;

	void on_tick_1( mhood_t<tick> )
	{
		if( 0 < m_counter )
		{
			std::cout << "Stop in " << m_counter << " second(s)..." << std::endl;
			--m_counter;
		}
		else
		{
			// It is the shutdown time.
			std::cout << "Stop started!" << std::endl;
			this >>= st_wait_shutdown_finish;
			// Tell SObjectizer Environment to finish its work.
			so_environment().stop();
		}
	}

	void on_tick_2( mhood_t<tick> )
	{
		++m_counter;
		std::cout << "Shutdown is in progress for " << m_counter << " second(s)"
				<< std::endl;
	}
};

//
// An agent that needs some time to finish its work before shutdown.
//
class worker_t final : public so_5::agent_t
{
	// The first state is used for "normal" work.
	state_t st_normal{ this };
	// The second state is used during shutdown of the agent.
	state_t st_shutting_down{ this };

	// This signal will be used for counting time before the completion
	// of shutdown procedure.
	struct tick final : public so_5::signal_t {};

public :
	worker_t(
		context_t ctx,
		// Every worker will have the unique name.
		std::string name,
		// Every worker must perform several steps during shutdown procedure.
		// This parameter tells how long will be every step.
		std::chrono::steady_clock::duration tick_size,
		// How much steps must be taken before completion of the shutdown.
		unsigned int ticks_before_shutdown )
		: so_5::agent_t( std::move(ctx) )
		, m_name( std::move(name) )
		, m_tick_size( tick_size )
		, m_ticks_before_shutdown( ticks_before_shutdown )
	{
		this >>= st_normal;

		// Create subscription to shutdown notification.
		// While this subscription exists the shutdown of the SObjectizer
		// Environment can't completed.
		st_normal.event(
				so_5::extra::shutdowner::layer( so_environment() ).notify_mbox(),
				&worker_t::on_shutdown_initiated );

		st_shutting_down
			// Counting of shutdown steps must be started at switching to
			// that state.
			.on_enter( [this] {
					m_timer = so_5::send_periodic< tick >( *this,
							std::chrono::seconds::zero(),
							m_tick_size );
				} )
			.event( &worker_t::on_tick );
	}

	virtual void so_evt_finish() override
	{
		std::cout << "worker: " << m_name << ", finished!" << std::endl;
	}

private :
	const std::string m_name;
	const std::chrono::steady_clock::duration m_tick_size;
	unsigned int m_ticks_before_shutdown;

	so_5::timer_id_t m_timer;

	void on_shutdown_initiated( mhood_t<so_5::extra::shutdowner::shutdown_initiated_t> )
	{
		std::cout << "worker: " << m_name << ", shutdown started." << std::endl;
		this >>= st_shutting_down;
	}

	void on_tick( mhood_t<tick> )
	{
		using namespace std::chrono;

		std::cout << "worker: " << m_name << ", stop in "
				<< duration_cast< milliseconds >(m_tick_size *
						m_ticks_before_shutdown).count() << "ms" << std::endl;

		if( !m_ticks_before_shutdown )
			// Shutdown is completed. The coop can be deregistered.
			// During deregistration the subscription to
			// so_5::extra::shutdowner::shutdown_initiated_t will be destroyed.
			so_deregister_agent_coop_normally();
		else
			--m_ticks_before_shutdown;
	}
};

void make_worker(
	so_5::environment_t & env,
	std::string name,
	std::chrono::steady_clock::duration tick_size,
	unsigned int ticks_before_shutdown )
{
	env.introduce_coop( [&]( so_5::coop_t & coop ) {
		coop.make_agent< worker_t >(
				std::move(name),
				tick_size,
				ticks_before_shutdown );
	} );
}

void run_example()
{
	so_5::launch( []( so_5::environment_t & env ) {
			env.introduce_coop( []( so_5::coop_t & coop ) {
					coop.make_agent< shutdown_initiator_t >();
				} );

			make_worker( env, "worker-1", std::chrono::milliseconds(250), 5u );
			make_worker( env, "worker-2", std::chrono::milliseconds(350), 6u );
			make_worker( env, "worker-3", std::chrono::milliseconds(750), 3u );
			make_worker( env, "worker-4", std::chrono::milliseconds(150), 10u );
			make_worker( env, "worker-5", std::chrono::milliseconds(0), 0u );
		},
		[]( so_5::environment_params_t & params ) {
			params.add_layer(
					so_5::extra::shutdowner::make_layer<>(
							std::chrono::seconds(15) ) );
		} );
}

int main()
{
	try
	{
		run_example();
		return 0;
	}
	catch( const std::exception & x )
	{
		std::cerr << "Exception caught: " << x.what() << std::endl;
	}

	return 2;
}

