/*
 * An example of so_5::extra::disp::asio_thread_pool dispatcher with
 * custom thread class which is based on POSIX Threads API.
 */

#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

#include <so_5/all.hpp>

#include <system_error>

#include <pthread.h>

// Custom implementation of worker thread.
class my_pthread_t final : public so_5::disp::abstract_work_thread_t
{
	pthread_t m_thread;
	body_func_t m_func; // Type body_func_t is inherited.

	const std::size_t m_stack_size;
	const int m_priority;

	bool m_joined{ false };

	static void * thread_body( void * arg )
	{
		auto * self = reinterpret_cast<my_pthread_t*>(arg);
		self->m_func();

		return nullptr;
	}

public :
	struct stack_size_t { std::size_t v; };
	struct priority_t { int v; };

	my_pthread_t() = delete;
	my_pthread_t( const my_pthread_t & ) = delete;
	my_pthread_t( my_pthread_t &&r ) = delete;

	my_pthread_t(
		stack_size_t stack_size,
		priority_t priority )
		:	m_stack_size{ stack_size.v }
		,	m_priority{ priority.v }
	{}

	void start( body_func_t thread_body ) override
	{
		// Grab the actual body of worker thread.
		m_func = std::move(thread_body);

		// Prepare attributes for a new thread.
		// Doesn't check errors for simplicity.
		pthread_attr_t attr;
		pthread_attr_init( &attr );
		pthread_attr_setstacksize( &attr, m_stack_size );
		pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );

		struct sched_param schedp;
		// To get default values of sched_param's fields just
		// use getschedparam function.
		pthread_attr_getschedparam( &attr, &schedp );
		// Now we can change only the priority.
		schedp.sched_priority = m_priority;
		pthread_attr_setschedparam( &attr, &schedp );

		const auto rc = pthread_create(
				&m_thread,
				&attr,
				&my_pthread_t::thread_body,
				this );

		pthread_attr_destroy( &attr );

		if( 0 != rc )
			throw std::system_error(
					std::error_code(errno, std::generic_category()) );
	}

	void join() override
	{
		if( !m_joined )
		{
			pthread_join( m_thread, nullptr );
			m_joined = true;
		}
	}
};

// Factory for custom worker thread.
class my_pthread_factory_t final
	:	public so_5::disp::abstract_work_thread_factory_t
{
public:
	using stack_size_t = my_pthread_t::stack_size_t;
	using priority_t = my_pthread_t::priority_t;

private:
	const stack_size_t m_stack_size;
	const priority_t m_priority;

public:
	my_pthread_factory_t(
		stack_size_t stack_size,
		priority_t priority )
		:	m_stack_size{ stack_size }
		,	m_priority{ priority }
	{}

	so_5::disp::abstract_work_thread_t &
	acquire( so_5::environment_t & /*env*/ ) override
	{
		return *(new my_pthread_t{ m_stack_size, m_priority });
	}

	void
	release( so_5::disp::abstract_work_thread_t & thread ) noexcept override
	{
		delete &thread;
	}
};

// Implementation of arbiter agent. This agent finishes SObjectizer
// when all 'finished' signals are received.
class arbiter_t final : public so_5::agent_t
{
public :
	// Type of signal which every ring member must send when it finishes its
	// work.
	struct finished final : public so_5::signal_t {};

	arbiter_t( context_t ctx, std::size_t ring_size )
		:	so_5::agent_t( std::move(ctx) )
		,	m_ring_size( ring_size )
	{
		so_subscribe( so_environment().create_mbox( "arbiter" ) )
			.event( &arbiter_t::on_finished );
	}

private :
	const std::size_t m_ring_size;
	std::size_t m_finished_count{ 0 };

	void on_finished( mhood_t<finished> )
	{
		++m_finished_count;
		if( m_finished_count == m_ring_size )
		{
			std::cout << "all " << m_ring_size
				<< " agents finished its work" << std::endl;
			so_deregister_agent_coop_normally();
		}
	}
};

// Implementation of ring member.
class ring_member_t final : public so_5::agent_t
{
	struct your_turn final : public so_5::signal_t {};

public :
	ring_member_t(
		context_t ctx,
		std::size_t turns_count )
		:	so_5::agent_t( std::move(ctx) )
		,	m_turns_left( turns_count )
	{
		so_subscribe_self().event( &ring_member_t::on_your_turn );
	}

	void set_next( so_5::mbox_t next ) noexcept
	{
		m_next = std::move(next);
	}

	virtual void so_evt_start() override
	{
		make_next_turn();
	}

private :
	std::size_t m_turns_left;
	so_5::mbox_t m_next;

	void on_your_turn( mhood_t<your_turn> )
	{
		make_next_turn();
	}

	void make_next_turn()
	{
		if( m_turns_left )
		{
			--m_turns_left;
			so_5::send< your_turn >( m_next );
		}
		else
		{
			so_5::send< arbiter_t::finished >(
					so_environment().create_mbox( "arbiter" ) );
		}
	}
};

void fill_coop( so_5::coop_t & coop )
{
	constexpr std::size_t ring_size = 25;
	constexpr std::size_t turns_count = 100;

	// Creation of arbiter is straightforward.
	coop.make_agent< arbiter_t >( ring_size );

	// Private asio_thread_pool is necessary for ring of agents.
	namespace asio_tp = so_5::extra::disp::asio_thread_pool;

	asio_tp::disp_params_t disp_params;
	// Dispatcher must use its own copy of Asio IoService.
	disp_params.use_own_io_context();
	// Dispatcher must use a custom thread factory.
	disp_params.work_thread_factory(
			std::make_shared< my_pthread_factory_t >(
					// Those parameters will be used for worker thread.
					my_pthread_t::stack_size_t{ 4096u },
					my_pthread_t::priority_t{ 2 } ) );

	// Create dispatcher for ring of agents.
	auto disp = asio_tp::make_dispatcher(
			coop.environment(),
			"asio_tp",
			std::move(disp_params) );

	// Creation of every agent requires three steps:
	// 1. Creation of agent's instance with strand inside...
	std::array< ring_member_t *, ring_size > members;
	for( auto & ptr : members )
	{
		ptr = coop.make_agent_with_binder< ring_member_t >(
				disp.binder(),
				turns_count );
	}

	// 2. Setting the 'next' mbox for every member.
	for( std::size_t i = 0; i != ring_size; ++i )
		members[ i ]->set_next( members[ (i+1) % ring_size ]->so_direct_mbox() );
}

int main()
{
	try
	{
		so_5::launch( [](so_5::environment_t & env) {
			env.introduce_coop( [](so_5::coop_t & coop) {
				fill_coop(coop);
			} );
		} );
	}
	catch( const std::exception & x )
	{
		std::cerr << "Oops: " << x.what() << std::endl;
	}

	return 2;
}

