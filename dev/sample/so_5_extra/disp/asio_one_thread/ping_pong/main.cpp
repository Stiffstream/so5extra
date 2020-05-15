#include <so_5_extra/disp/asio_one_thread/pub.hpp>

#include <so_5/all.hpp>

#include <iostream>

#include <cstdio>
#include <cstring>
#include <cstdlib>

struct	cfg_t
{
	unsigned int	m_request_count{ 1000u };

	bool	m_separate_dispatchers{ false };
};

[[nodiscard]]
cfg_t
try_parse_cmdline(
	int argc,
	char ** argv )
	{
		if( 1 == argc )
			{
				std::cout << "usage:\n"
						"sample.so_5_extra.disp.asio_one_thread.ping_pong <options>\n"
						"\noptions:\n"
						"-s, --separate-dispatchers agents should work on "
								"different dispatchers\n"
						"-r, --requests             count of requests to send\n"
						<< std::endl;

				throw std::runtime_error( "No command-line arguments" );
			}

		auto is_arg = []( const char * value,
				const char * v1,
				const char * v2 )
				{
					return 0 == std::strcmp( value, v1 ) ||
							0 == std::strcmp( value, v2 );
				};

		cfg_t result;

		char ** current = argv + 1;
		char ** last = argv + argc;

		while( current != last )
			{
				if( is_arg( *current, "-s", "--separate-dispatchers" ) )
					{
						result.m_separate_dispatchers = true;
					}
				else if( is_arg( *current, "-r", "--requests" ) )
					{
						++current;
						if( current == last )
							throw std::runtime_error( "-r requires argument" );

						result.m_request_count = static_cast< unsigned int >(
								std::atoi( *current ) );
					}
				else
					{
						throw std::runtime_error(
							std::string( "unknown argument: " ) + *current );
					}
				++current;
			}

		return result;
	}

void
show_cfg(
	const cfg_t & cfg )
	{
		std::cout << "Configuration: "
			<< "separate dispatchers: "
			<< ( cfg.m_separate_dispatchers ? "yes" : "no" )
			<< ", requests: " << cfg.m_request_count
			<< std::endl;
	}

// Types of signals for the agents.
struct msg_ping final : public so_5::signal_t {};
struct msg_pong final : public so_5::signal_t {};

// Type of pinger agent.
class pinger_t final : public so_5::agent_t
	{
		const so_5::mbox_t m_mbox;
		unsigned int m_pings_left;
	public :
		pinger_t(
			context_t ctx,
			so_5::mbox_t mbox,
			unsigned int pings_left )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_mbox{ std::move(mbox) }
			,	m_pings_left{ pings_left }
			{
				so_subscribe( m_mbox ).event(
					[this]( mhood_t<msg_pong> ) {
						if( m_pings_left ) --m_pings_left;
						if( m_pings_left )
							so_5::send< msg_ping >( m_mbox );
						else
							so_environment().stop();
					} );
			}

		void so_evt_start() override
			{
				so_5::send< msg_ping >( m_mbox );
			}
	};

// Type of ponger agent.
class ponger_t final : public so_5::agent_t
	{
	public :
		ponger_t( context_t ctx, const so_5::mbox_t & mbox )
			:	so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe( mbox ).event(
					[mbox]( mhood_t<msg_ping> ) {
						so_5::send< msg_pong >( mbox );
					} );
			}
	};

// Just a helper to create an instance of asio_one_thread dispatcher.
[[nodiscard]]
auto
make_asio_disp(
	so_5::environment_t & env,
	std::string dispatcher_name )
{
	namespace asio_disp = so_5::extra::disp::asio_one_thread;

	asio_disp::disp_params_t params;
	params.use_own_io_context();
	return asio_disp::make_dispatcher( env, dispatcher_name, std::move(params) );
}

void
run_sample(
	const cfg_t & cfg )
	{
		so_5::launch(
			[&cfg]( so_5::environment_t & env )
			{
				auto first_binder = make_asio_disp( env, "first" ).binder();
				auto second_binder = cfg.m_separate_dispatchers ?
						make_asio_disp( env, "second" ).binder() :
						first_binder;

				env.introduce_coop(
						[&]( so_5::coop_t & coop )
						{
							auto mbox = env.create_mbox();

							// Pinger agent.
							coop.make_agent_with_binder< pinger_t >(
									first_binder,
									mbox, cfg.m_request_count );
							// Ponger agent.
							coop.make_agent_with_binder< ponger_t >(
									second_binder,
									std::cref(mbox) );
						} );
			} );
	}

int main( int argc, char ** argv )
{
	try
	{
		cfg_t cfg = try_parse_cmdline( argc, argv );
		show_cfg( cfg );

		run_sample( cfg );

		return 0;
	}
	catch( const std::exception & x )
	{
		std::cerr << "*** Exception caught: " << x.what() << std::endl;
	}

	return 2;
}

