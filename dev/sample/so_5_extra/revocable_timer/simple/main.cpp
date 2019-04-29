/*
 * A simple demo for revocable timers.
 */

#include <so_5_extra/revocable_timer/pub.hpp>

#include <so_5/all.hpp>

namespace timer_ns = so_5::extra::revocable_timer;

class example_t final : public so_5::agent_t
{
	// Several signals which will be used as delayed/periodic signals.
	struct first_delayed final : public so_5::signal_t {};
	struct second_delayed final : public so_5::signal_t {};
	struct last_delayed final : public so_5::signal_t {};

	struct periodic final : public so_5::signal_t {};

	// IDs for delayed/periodic signals.
	timer_ns::timer_id_t m_first;
	timer_ns::timer_id_t m_second;
	timer_ns::timer_id_t m_last;
	timer_ns::timer_id_t m_periodic;

public :
	example_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
	{
		so_subscribe_self()
			.event( &example_t::on_first_delayed )
			.event( &example_t::on_second_delayed )
			.event( &example_t::on_last_delayed )
			.event( &example_t::on_periodic );
	}

	void so_evt_start() override
	{
		using namespace std::chrono_literals;

		// Initiate all signals.
		m_first = timer_ns::send_delayed< first_delayed >( *this, 100ms );
		m_second = timer_ns::send_delayed< second_delayed >( *this, 200ms );
		m_last = timer_ns::send_delayed< last_delayed >( *this, 300ms );
		m_periodic = timer_ns::send_periodic< periodic >( *this, 75ms, 75ms );

		// Hang this agent for 220ms. It means that
		// first_delayed, second_delayed and several periodic will be
		// places in agent's event queue.
		std::cout << "hang the agent..." << std::flush;
		std::this_thread::sleep_for( 220ms );
		std::cout << "done" << std::endl;
	}

private :
	void on_first_delayed( mhood_t<first_delayed> )
	{
		std::cout << "first_delayed received" << std::endl;

		// Revoke second_delayed and periodic.
		// No second_delayed or periodic should be received since then.
		m_second.revoke();
		m_periodic.revoke();
	}

	void on_second_delayed( mhood_t<second_delayed> )
	{
		std::cout << "second_delayed received" << std::endl;
	}

	void on_last_delayed( mhood_t<last_delayed> )
	{
		std::cout << "last_delayed received" << std::endl;
		so_deregister_agent_coop_normally();
	}

	void on_periodic( mhood_t<periodic> )
	{
		std::cout << "periodic received" << std::endl;
	}
};

int main()
{
	so_5::launch( [](so_5::environment_t & env) {
		env.register_agent_as_coop( env.make_agent< example_t >() );
	} );

	return 0;
}

