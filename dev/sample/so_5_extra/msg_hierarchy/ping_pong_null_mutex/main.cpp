#include <so_5_extra/msg_hierarchy/pub.hpp>

#include <so_5/all.hpp>

#include <iostream>

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace sample
{

struct cfg_t
{
	unsigned int m_request_count{ 1000u };
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
						"sample.so_5_extra.msg_hierarchy.ping_pong_null_mutex <options>\n"
						"\noptions:\n"
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
				if( is_arg( *current, "-r", "--requests" ) )
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
			<< ", requests: " << cfg.m_request_count
			<< std::endl;
	}

struct null_mutex_t
	{
		void lock() noexcept {}
		void unlock() noexcept {}

		void lock_shared() noexcept {}
		void unlock_shared() noexcept {}
	};

//
// Types for message for exchange.
//
struct basic : public so_5::extra::msg_hierarchy::root_t< basic >
	{
		basic() = default;
	};

struct abstract_ping
	: public basic
	, public so_5::extra::msg_hierarchy::node_t< abstract_ping, basic >
	{
		[[nodiscard]]
		virtual int
		payload() const noexcept = 0;

		abstract_ping()
			// This is required by msg_hierarchy's design.
			: so_5::extra::msg_hierarchy::node_t< abstract_ping, basic >{ *this }
		{}
	};

struct abstract_pong
	: public basic
	, public so_5::extra::msg_hierarchy::node_t< abstract_pong, basic >
	{
		[[nodiscard]]
		virtual int
		payload() const noexcept = 0;

		abstract_pong()
			// This is required by msg_hierarchy's design.
			: so_5::extra::msg_hierarchy::node_t< abstract_pong, basic >{ *this }
		{}
	};

// Type of demuxer to be used.
using demuxer_t = so_5::extra::msg_hierarchy::demuxer_t< basic, null_mutex_t >;

// Type of pinger agent.
template< typename Actual_Ping_Type >
class pinger_t final : public so_5::agent_t
	{
		// This object should live as long as the agent itself.
		so_5::extra::msg_hierarchy::consumer_t< basic > m_consumer;

		// The mbox for outgoing messages.
		const so_5::mbox_t m_out_mbox;

		unsigned int m_pings_left;

	public :
		pinger_t(
			context_t ctx,
			demuxer_t & demuxer,
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
		so_5::extra::msg_hierarchy::consumer_t< basic > m_consumer;

		// The mbox for outgoing messages.
		const so_5::mbox_t m_out_mbox;

	public :
		ponger_t(
			context_t ctx,
			demuxer_t & demuxer )
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
	, public so_5::extra::msg_hierarchy::node_t< ping, abstract_ping >
{
	int m_payload;

	int
	payload() const noexcept override { return m_payload; }

	ping( int payload )
		: so_5::extra::msg_hierarchy::node_t< ping, abstract_ping >{ *this }
		, m_payload{ payload }
		{}
};

// Actual pong message type.
struct pong final
	: public abstract_pong
	, public so_5::extra::msg_hierarchy::node_t< pong, abstract_pong >
{
	int m_payload;

	int
	payload() const noexcept override { return m_payload; }

	pong( int payload )
		: so_5::extra::msg_hierarchy::node_t< pong, abstract_pong >{ *this }
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
				env.introduce_coop(
						[&]( so_5::coop_t & coop )
						{
							demuxer_t demuxer{
									coop.environment(),
									so_5::extra::msg_hierarchy::multi_consumer
								};

							// Pinger agent.
							coop.make_agent< pinger_t< ping > >( demuxer, cfg.m_request_count );
							// Ponger agent.
							coop.make_agent< ponger_t< pong > >( demuxer );
						} );
			},
			[]( so_5::environment_params_t & params )
			{
				params.infrastructure_factory(
						so_5::env_infrastructures::simple_not_mtsafe::factory() );
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

