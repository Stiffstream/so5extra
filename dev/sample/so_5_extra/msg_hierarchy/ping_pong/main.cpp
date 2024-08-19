#include <so_5_extra/msg_hierarchy/pub.hpp>

#include <so_5/all.hpp>

#include <iostream>

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace sample
{

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
						"sample.so_5_extra.msg_hierarchy.ping_pong <options>\n"
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

namespace hierarchy_ns = so_5::extra::msg_hierarchy;

//
// Types for message for exchange.
//
struct basic : public hierarchy_ns::root_t< basic >
	{
		basic() = default;
	};

struct abstract_ping
	: public basic
	, public hierarchy_ns::node_t< abstract_ping, basic >
	{
		[[nodiscard]]
		virtual int
		payload() const noexcept = 0;

		abstract_ping()
			// This is required by msg_hierarchy's design.
			: hierarchy_ns::node_t< abstract_ping, basic >{ *this }
		{}
	};

struct abstract_pong
	: public basic
	, public hierarchy_ns::node_t< abstract_pong, basic >
	{
		[[nodiscard]]
		virtual int
		payload() const noexcept = 0;

		abstract_pong()
			// This is required by msg_hierarchy's design.
			: hierarchy_ns::node_t< abstract_pong, basic >{ *this }
		{}
	};

// Type of pinger agent.
template< typename Actual_Ping_Type >
class pinger_t final : public so_5::agent_t
	{
		// This object should live as long as the agent itself.
		hierarchy_ns::consumer_t< basic > m_consumer;

		// The mbox for outgoing messages.
		const so_5::mbox_t m_out_mbox;

		unsigned int m_pings_left;

	public :
		pinger_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< basic > & demuxer,
			unsigned int pings_left )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_out_mbox{ demuxer.sending_mbox() }
			, m_pings_left{ pings_left }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< abstract_pong >() ).event(
					[this]( mhood_t<abstract_pong> cmd ) {
						if( m_pings_left ) --m_pings_left;
						if( m_pings_left )
							so_5::send< Actual_Ping_Type >( m_out_mbox, cmd->payload() + 1 );
						else
							so_environment().stop();
					} );
			}

		void so_evt_start() override
			{
				so_5::send< Actual_Ping_Type >( m_out_mbox, 0 );
			}
	};

// Type of ponger agent.
template< typename Actual_Pong_Type >
class ponger_t final : public so_5::agent_t
	{
		// This object should live as long as the agent itself.
		hierarchy_ns::consumer_t< basic > m_consumer;

		// The mbox for outgoing messages.
		const so_5::mbox_t m_out_mbox;

	public :
		ponger_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< basic > & demuxer )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_out_mbox{ demuxer.sending_mbox() }
			{}

		void
		so_define_agent() override
			{
				so_subscribe( m_consumer.receiving_mbox< abstract_ping >() ).event(
					[this]( mhood_t<abstract_ping> cmd ) {
						so_5::send< Actual_Pong_Type >( m_out_mbox, cmd->payload() + 1 );
					} );
			}
	};

// Actual ping message type.
struct ping final
	: public abstract_ping
	, public hierarchy_ns::node_t< ping, abstract_ping >
{
	int m_payload;

	int
	payload() const noexcept override { return m_payload; }

	ping( int payload )
		: hierarchy_ns::node_t< ping, abstract_ping >{ *this }
		, m_payload{ payload }
		{}
};

// Actual pong message type.
struct pong final
	: public abstract_pong
	, public hierarchy_ns::node_t< pong, abstract_pong >
{
	int m_payload;

	int
	payload() const noexcept override { return m_payload; }

	pong( int payload )
		: hierarchy_ns::node_t< pong, abstract_pong >{ *this }
		, m_payload{ payload }
		{}
};

void
run_sample(
	const cfg_t & cfg )
	{
		so_5::launch(
			[&cfg]( so_5::environment_t & env )
			{
				auto first_binder = so_5::disp::one_thread::make_dispatcher(
						env, "first" ).binder();
				auto second_binder = cfg.m_separate_dispatchers
						? so_5::disp::one_thread::make_dispatcher( env, "second" ).binder()
						: first_binder;

				env.introduce_coop(
						[&]( so_5::coop_t & coop )
						{
							hierarchy_ns::demuxer_t< basic > demuxer{
									coop.environment(),
									hierarchy_ns::multi_consumer
								};

							// Pinger agent.
							coop.make_agent_with_binder< pinger_t< ping > >(
									first_binder,
									demuxer, cfg.m_request_count );
							// Ponger agent.
							coop.make_agent_with_binder< ponger_t< pong > >(
									second_binder,
									demuxer );
						} );
			} );
	}

} /* namespace sample */

using namespace sample;

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

