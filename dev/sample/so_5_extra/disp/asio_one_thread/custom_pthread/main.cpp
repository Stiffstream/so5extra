/*
 * A very simple example of usage of asio_one_thread dispatcher
 * with custom thread type.
 */
#include <so_5_extra/disp/asio_one_thread/pub.hpp>

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

// Type of agent to be used in example.
class a_hello_t final : public so_5::agent_t
{
	struct hello final : public so_5::signal_t {};

public :
	using so_5::agent_t::agent_t;

	void so_define_agent() override
	{
		so_subscribe_self().event( &a_hello_t::on_hello );
	}

	void so_evt_start() override
	{
		std::cout << "Start" << std::endl;

		so_5::send< hello >( *this );
	}

	void so_evt_finish() override
	{
		std::cout << "Finish" << std::endl;
	}

private :
	void on_hello( mhood_t<hello> )
	{
		std::cout << "Hello" << std::endl;
		so_deregister_agent_coop_normally();
	}
};

int main()
{
	so_5::launch( [&](so_5::environment_t & env) {
		namespace asio_disp = so_5::extra::disp::asio_one_thread;

		// Create dispatcher instance.
		// That instance will use external io_context object.
		auto disp = asio_disp::make_dispatcher(
				env,
				"asio_disp",
				asio_disp::disp_params_t{}
					.use_own_io_context()
					.work_thread_factory(
						std::make_shared< my_pthread_factory_t >(
							// Those parameters will be used for worker thread.
							my_pthread_t::stack_size_t{ 4096u },
							my_pthread_t::priority_t{ 2 } ) )
			);

		// Create hello-agent that will be bound to asio_one_thread dispatcher.
		env.introduce_coop(
				// Agent will be protected by strand-object.
				disp.binder(),
				[&]( so_5::coop_t & coop )
				{
					coop.make_agent<a_hello_t>();
				} );
	} );
}

