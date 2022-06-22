/*
 * Simple example for inflight_limit_mbox.
 */

#include <so_5_extra/mboxes/inflight_limit.hpp>
#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

// Simple logger to avoid message merging when they are going from
// different threads.
class logger
{
	std::mutex m_lock;

public:
	template< typename... Args >
	void info( Args && ...args )
	{
		std::lock_guard< std::mutex > lock{ m_lock };
		std::cout << "*** ";
		(std::cout << ... << args);
		std::cout << std::endl;
	}

	template< typename... Args >
	void err( Args && ...args )
	{
		std::lock_guard< std::mutex > lock{ m_lock };
		std::cout << "### ";
		(std::cout << ... << args);
		std::cout << std::endl;
	}
};

logger g_log;

// Message to be processed.
// Processed message has to be marked by calling `processed` method.
// If that method wasn't called the message is considered as discarded.
class msg_do_something final : public so_5::message_t
{
private:
	bool m_processed{ false };

public:
	std::string m_id;

	msg_do_something( std::string id ) : m_id{ std::move(id) } {}
	~msg_do_something() noexcept override
	{
		if( !m_processed )
		{
			g_log.err( "[", m_id, "] discarded without processing" );
		}
	}

	void processed() noexcept
	{
		m_processed = true;
	}
};

// Agent that perform message processing.
class processor final : public so_5::agent_t
{
	// Mbox from that incoming messages are expected.
	const so_5::mbox_t m_incoming_mbox;

	// Name of worker to be used for logging.
	const std::string m_name;

public:
	processor(
		context_t ctx,
		so_5::mbox_t incoming_mbox,
		std::string name )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_incoming_mbox{ std::move(incoming_mbox) }
		,	m_name{ std::move(name) }
	{}

	void so_define_agent() override
	{
		so_subscribe( m_incoming_mbox )
			.event( &processor::evt_do_something );
	}

private:
	void evt_do_something( mutable_mhood_t<msg_do_something> cmd )
	{
		g_log.info( m_name, " [", cmd->m_id, "] processing started" );

		// Block the worker thread for some time.
		// The message is seen as inflight while we're sleeping here.
		std::this_thread::sleep_for( 25ms );

		g_log.info( m_name, " [", cmd->m_id, "] processing finished" );

		// Mark the message as processed. It's necessary for correct logging.
		cmd->processed();
	}
};

// Agent that generates messages.
class generator final : public so_5::agent_t
{
	struct msg_generate_next final : public so_5::signal_t {};

	// Name of that agent. Will be a part of message ID.
	const std::string m_name;

	// The destination for messages.
	const so_5::mbox_t m_dest_mbox;

	// Initial delay for messages from that agent.
	const std::chrono::milliseconds m_initial_delay;

	// Counter to be used for message IDs.
	unsigned int m_ordinal{};

public:
	generator(
		context_t ctx,
		std::string name,
		so_5::mbox_t dest_mbox,
		std::chrono::milliseconds initial_delay )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_name{ std::move(name) }
		,	m_dest_mbox{ std::move(dest_mbox) }
		,	m_initial_delay{ initial_delay }
	{}

	void so_define_agent() override
	{
		so_subscribe_self()
			.event( &generator::evt_generate_next );
	}

	void so_evt_start() override
	{
		so_5::send_delayed< msg_generate_next >( *this, m_initial_delay );
	}

private:
	void evt_generate_next( mhood_t<msg_generate_next> )
	{
		++m_ordinal;

		auto id = m_name + "-" + std::to_string( m_ordinal );

		g_log.info( m_name, " sending [", id, "]" );

		so_5::send< so_5::mutable_msg<msg_do_something> >(
				m_dest_mbox,
				std::move(id) );

		so_5::send_delayed< msg_generate_next >( *this, 15ms );
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
				env.introduce_coop( []( so_5::coop_t & coop ) {
						// Create round_robin mbox to distribute messages
						// between processors.
						auto rr_mbox = so_5::extra::mboxes::round_robin::make_mbox<>(
								coop.environment() );

						// Create processors and collect their mboxes.
						constexpr unsigned int processors_count = 4u;
						auto thread_pool_binder =
								so_5::disp::thread_pool::make_dispatcher(
										coop.environment(),
										processors_count /* one thread per processor */ )
									.binder( []( auto & bind_params ) {
											// Every worker should have own demand queue.
											bind_params.fifo( so_5::disp::thread_pool::
													fifo_t::individual );
										} );
						for( unsigned int i = 0; i != processors_count; ++i )
						{
							coop.make_agent_with_binder< processor >(
									thread_pool_binder,
									rr_mbox,
									"worker-" + std::to_string( i + 1u ) );
						}

						// Create inflight_limit mbox to limit number of
						// unprocessed messages.
						auto dest_mbox = so_5::extra::mboxes::inflight_limit::
								make_mbox< so_5::mutable_msg<msg_do_something> >(
										rr_mbox,
										processors_count );

						// Create generators.
						constexpr std::size_t generators_count = 4u;
						std::string names[ generators_count ] =
								{ "alice", "bob", "eve", "kate" };
						std::chrono::milliseconds initial_delays[ generators_count ] =
								{ 7ms, 0ms, 17ms, 23ms };

						for( std::size_t i{}; i != generators_count; ++i )
							coop.make_agent< generator >(
									names[ i ],
									dest_mbox,
									initial_delays[ i ] );
					} );

				// Limit execution time.
				std::this_thread::sleep_for( 95ms );
				env.stop();
			} );
}

