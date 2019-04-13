/*
 * An example of so_5::extra::disp::asio_thread_pool dispatcher with
 * custom thread class which is based on POSIX Threads API.
 */

#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

#include <so_5/all.hpp>

#include <system_error>

#include <pthread.h>

// Custom implementation of std::thread-like class.
class my_pthread_t
{
	pthread_t m_thread;
	std::function<void()> m_func;
	bool m_joined{ false };

	static void * thread_body( void * arg )
	{
		auto * self = reinterpret_cast<my_pthread_t*>(arg);
		self->m_func();

		return nullptr;
	}

public :
	my_pthread_t() = delete;
	my_pthread_t( const my_pthread_t & ) = delete;
	my_pthread_t( my_pthread_t &&r ) = delete;

	template<typename F>
	my_pthread_t( F && f )
		:	m_func( std::move(f) )
	{
		const auto rc = pthread_create(
				&m_thread,
				nullptr,
				&my_pthread_t::thread_body,
				this );
		if( 0 != rc )
			throw std::system_error(
					std::error_code(errno, std::generic_category()) );
	}

	~my_pthread_t()
	{
		join();
	}

	void join() noexcept
	{
		if( !m_joined )
		{
			pthread_join( m_thread, nullptr );
			m_joined = true;
		}
	}
};

// Definition of traits to be used with asio_thread_pool.
struct my_disp_traits
{
	// Actual type of thread to be used.
	using thread_type = my_pthread_t;
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
		asio::io_context & io_svc,
		std::size_t turns_count )
		:	so_5::agent_t( std::move(ctx) )
		,	m_strand( io_svc )
		,	m_turns_left( turns_count )
	{
		so_subscribe_self().event( &ring_member_t::on_your_turn );
	}

	asio::io_context::strand &
	strand() noexcept
	{
		return m_strand;
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
	asio::io_context::strand m_strand;
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

	// Create dispatcher for ring of agents.
	auto disp = asio_tp::create_private_disp<my_disp_traits>(
			coop.environment(),
			"asio_tp",
			std::move(disp_params) );

	// Creation of every agent requires three steps:
	// 1. Creation of agent's instance with strand inside...
	std::array< std::unique_ptr<ring_member_t>, ring_size > members;
	for( auto & ptr : members )
		ptr = std::make_unique< ring_member_t >(
				coop.make_agent_context(),
				disp->io_context(),
				turns_count );

	// 2. Setting the 'next' mbox for every member.
	for( std::size_t i = 0; i != ring_size; ++i )
		members[ i ]->set_next( members[ (i+1) % ring_size ]->so_direct_mbox() );

	// 3. Addition of the agent to the coop by using special binder.
	for( auto & ptr : members )
	{
		auto & strand = ptr->strand();
		coop.add_agent( std::move(ptr), disp->binder(strand) );
	}
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

