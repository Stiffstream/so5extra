/*
 * Simplest usage of round-robin mbox.
 */

#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

// A simple worker for handling messages from round-robin mbox.
class worker_t final : public so_5::agent_t
{
public :
	worker_t(
		// Environment to work in.
		context_t ctx,
		// Name of worker.
		std::string name,
		// Round-robin mbox.
		const so_5::mbox_t & src )
		:	so_5::agent_t( std::move(ctx) )
		,	m_name( std::move(name) )
	{
		// Subscribe worker to messages from round-robin mbox.
		so_subscribe( src ).event( &worker_t::on_task );
	}

private :
	const std::string m_name;

	void on_task( mhood_t<std::string> cmd )
	{
		std::cout << m_name << ": " << *cmd << std::endl;
		so_deregister_agent_coop_normally();
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
		// Create round-robin mbox.
		const auto rrmbox = so_5::extra::mboxes::round_robin::make_mbox<>( env );

		// Create several workers. All of them will be subscribed to
		// that round-robin mbox.
		for( int i = 0; i < 3; ++i )
			env.introduce_coop( [&]( so_5::coop_t & coop ) {
				coop.make_agent< worker_t >(
						"worker-" + std::to_string( i + 1 ),
						std::cref(rrmbox) );
			} );

		// Send several messages to be handled by workers.
		so_5::send< std::string >( rrmbox, "Alpha" );
		so_5::send< std::string >( rrmbox, "Beta" );
		so_5::send< std::string >( rrmbox, "Gamma" );
	} );

	return 0;
}

