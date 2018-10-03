/*
 * An example of using round-robin mbox for processing requests by different
 * workers (all of them have the same type).
 */

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <random>

#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

// A request to be processed.
struct request
{
	// Return address.
	so_5::mbox_t m_reply_to;
	// Request ID.
	int m_id;
	// Some payload.
	int m_payload;
};

// A reply to processed request.
struct reply
{
	// Worker ID.
	int m_worker_id;
	// Request ID.
	int m_id;
	// Was request processed successfully?
	bool m_processed;
};

// Message for logger.
struct log_message
{
	// Text to be logged.
	std::string m_what;
};

// Logger agent.
class a_logger_t : public so_5::agent_t
{
public :
	a_logger_t( context_t ctx )
		:	so_5::agent_t( ctx
				// Limit the count of messages.
				// Because we can't lost log messages the overlimit
				// must lead to application crash.
				+ limit_then_abort< log_message >( 100 ) )
		,	m_started_at( std::chrono::steady_clock::now() )
	{}

	virtual void so_define_agent() override
	{
		so_default_state().event(
			[this]( const log_message & evt ) {
				std::cout << "[+" << time_delta()
						<< "] -- " << evt.m_what << std::endl;
			} );
	}

private :
	const std::chrono::steady_clock::time_point m_started_at;

	std::string time_delta() const
	{
		auto now = std::chrono::steady_clock::now();

		std::ostringstream ss;
		ss << std::chrono::duration_cast< std::chrono::milliseconds >(
				now - m_started_at ).count() / 1000.0 << "ms";

		return ss.str();
	}
};

// Load generation agent.
class a_generator_t : public so_5::agent_t
{
public :
	a_generator_t(
		// Environment to work in.
		context_t ctx,
		// Address of message processor.
		so_5::mbox_t performer,
		// Address of logger.
		so_5::mbox_t logger )
		:	so_5::agent_t( ctx
				// Expect no more than just one next_turn signal.
				+ limit_then_drop< msg_next_turn >( 1 )
				// Limit the quantity of non-processed replies in the queue.
				+ limit_then_transform( 10,
					// All replies which do not fit to message queue
					// will be transformed to log_messages and sent
					// to logger.
					[this]( const reply & msg ) {
						return make_transformed< log_message >( m_logger,
								"generator: unable to process reply(" +
								std::to_string( msg.m_id ) + ")" );
					} ) )
		,	m_performer( std::move( performer ) )
		,	m_logger( std::move( logger ) )
		,	m_turn_pause( 250 )
	{
		m_random_engine.seed();
	}

	virtual void so_define_agent() override
	{
		so_default_state()
			.event< msg_next_turn >( &a_generator_t::evt_next_turn )
			.event( &a_generator_t::evt_reply );
	}

	virtual void so_evt_start() override
	{
		// Start work cycle.
		so_5::send< msg_next_turn >( *this );
	}

private :
	// Signal about start of the next turn.
	struct msg_next_turn : public so_5::signal_t {};

	// Performer for the requests processing.
	const so_5::mbox_t m_performer;
	// Logger.
	const so_5::mbox_t m_logger;

	// Pause between working turns.
	const std::chrono::milliseconds m_turn_pause;

	// Last generated ID for request.
	int m_last_id = 0;

	// Random number generator.
	std::default_random_engine m_random_engine;

	void evt_next_turn()
	{
		// Create and send new requests.
		generate_new_requests( random( 5, 8 ) );

		// Wait for next turn and process replies.
		so_5::send_delayed< msg_next_turn >( *this, m_turn_pause );
	}

	void evt_reply( const reply & evt )
	{
		so_5::send< log_message >( m_logger,
				"generator: reply received(" + std::to_string( evt.m_id ) +
				"), worker: " + std::to_string( evt.m_worker_id ) +
				", processed:" + std::to_string( evt.m_processed ) );
	}

	void generate_new_requests( int requests )
	{
		for(; requests > 0; --requests )
		{
			auto id = ++m_last_id;

			so_5::send< log_message >( m_logger,
					"generator: sending request(" + std::to_string( id ) + ")" );

			so_5::send< request >( m_performer,
					so_direct_mbox(), id, random( 30, 100 ) );
		}
	}

	int random( int l, int h )
	{
		return std::uniform_int_distribution< int >{l, h}( m_random_engine );
	}
};

// Performer agent.
class a_performer_t final : public so_5::agent_t
{
public :
	// Constructor for the performer.
	a_performer_t(
		context_t ctx,
		const so_5::mbox_t & rrmbox,
		int worker_id,
		so_5::mbox_t logger )
		:	so_5::agent_t( ctx
				// Limit count of requests in the queue.
				// If queue is full then request must be transformed
				// to negative reply.
				+ limit_then_transform( 3,
					[worker_id]( const request & evt ) {
						return make_transformed< reply >( evt.m_reply_to,
								worker_id, evt.m_id, false );
					} ) )
		,	m_worker_id( worker_id )
		,	m_logger( std::move( logger ) )
	{
		so_subscribe( rrmbox ).event( &a_performer_t::evt_request );
	}

private :
	const int m_worker_id;
	const so_5::mbox_t m_logger;

	void evt_request( const request & evt )
	{
		const auto processing_time = evt.m_payload;

		so_5::send< log_message >( m_logger,
				"worker_" + std::to_string( m_worker_id ) +
				": processing request(" +
				std::to_string( evt.m_id ) + ") for " +
				std::to_string( processing_time ) + "ms" );

		// Imitation of some intensive processing.
		std::this_thread::sleep_for(
				std::chrono::milliseconds( processing_time ) );

		// Generator must receive a reply for the request.
		so_5::send< reply >( evt.m_reply_to, m_worker_id, evt.m_id, true );
	}
};

void init( so_5::environment_t & env )
{
	env.introduce_coop( [&env]( so_5::coop_t & coop ) {
		// Logger will work on the default dispatcher.
		auto logger = coop.make_agent< a_logger_t >();

		// Round-robin mbox for work distribution.
		const auto rrmbox = so_5::extra::mboxes::round_robin::make_mbox<>( env );

		// Performer agents.
		// Must work on dedicated thread_pool dispatcher.
		auto performer_disp = so_5::disp::thread_pool::create_private_disp( env, 3 );
		auto performer_binding_params = so_5::disp::thread_pool::bind_params_t{}
				.fifo( so_5::disp::thread_pool::fifo_t::individual );

		// Create 3 performers.
		for( int worker_id = 0; worker_id < 3; ++worker_id )
		{
			coop.make_agent_with_binder< a_performer_t >(
					performer_disp->binder( performer_binding_params ),
					std::cref(rrmbox),
					worker_id,
					logger->so_direct_mbox() );
		}

		// Generator will work on dedicated one_thread dispatcher.
		coop.make_agent_with_binder< a_generator_t >(
				so_5::disp::one_thread::create_private_disp( env )->binder(),
				rrmbox,
				logger->so_direct_mbox() );
	});

	// Take some time to work.
	std::this_thread::sleep_for( std::chrono::seconds(5) );

	env.stop();
}

int main()
{
	try
	{
		so_5::launch( &init );
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

