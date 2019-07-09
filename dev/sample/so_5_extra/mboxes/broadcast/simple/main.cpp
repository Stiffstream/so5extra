/*
 * Simple usage of broadcasting fixed_mbox_template.
 */

#include <so_5_extra/mboxes/broadcast.hpp>

#include <so_5/all.hpp>

#include <array>

using namespace std::chrono_literals;

// Class of worker agent.
class worker_t final : public so_5::agent_t 
{
public :
	struct start final : public so_5::signal_t {};
	struct stop final : public so_5::signal_t {};

	worker_t( context_t ctx, std::string name )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_name{ std::move(name) }
	{}

	void so_define_agent() override
	{
		// Make subscriptions for messages from the direct mbox.
		so_subscribe_self()
			.event( &worker_t::on_start )
			.event( &worker_t::on_stop );
	}

private :
	const std::string m_name;

	void on_start( mhood_t<start> )
	{
		std::cout << m_name << ": starting..." << std::endl;
	}

	void on_stop( mhood_t<stop> )
	{
		std::cout << m_name << ": stopping..." << std::endl;
	}
};

// Class of manager that sends start/stop commands.
class manager_t final : public so_5::agent_t
{
	struct time_to_stop final : public so_5::signal_t {};

public :
	manager_t( context_t ctx, so_5::mbox_t worker )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_worker{ std::move(worker) }
	{}

	void so_define_agent() override
	{
		so_subscribe_self().event( &manager_t::on_stop );
	}

	void so_evt_start() override
	{
		so_5::send_delayed< time_to_stop >( *this, 250ms );

		// Initiate 'start' signal.
		so_5::send< worker_t::start >( m_worker );
	}

public :
	const so_5::mbox_t m_worker;

	void on_stop( mhood_t<time_to_stop> )
	{
		// Initiate 'stop' signal.
		so_5::send< worker_t::stop >( m_worker );

		so_deregister_agent_coop_normally();
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
		// Create main example coop.
		env.introduce_coop( [&]( so_5::coop_t & coop ) {
			// Create workers and collect worker's mboxes.
			std::array< so_5::mbox_t, 3 > worker_mboxes{
				coop.make_agent< worker_t >( "First" )->so_direct_mbox(),
				coop.make_agent< worker_t >( "Second" )->so_direct_mbox(),
				coop.make_agent< worker_t >( "Third" )->so_direct_mbox()
			};

			// Create broadcasting mbox.
			auto broadcast_mbox = so_5::extra::mboxes::broadcast::
					fixed_mbox_template_t<>::make( coop.environment(), worker_mboxes );
			// Move broadcasting mbox to the example manager.
			coop.make_agent< manager_t >( std::move(broadcast_mbox) );
		} );
	} );

	return 0;
}

