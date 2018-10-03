/*
 * An example of using Asio-based single-threaded and not-thread-safe
 * infrastructure.
 *
 * This example resolves names of hosts into IP addresses. Do at most N
 * resolvings at the same time. When a result is received the next waiting
 * resolving request is initated (if such waiting request exists).
 */
#include <so_5_extra/env_infrastructures/asio/simple_not_mtsafe.hpp>

#include <so_5/all.hpp>

#include <asio.hpp>
#include <asio/ip/tcp.hpp>

#include <sstream>

using asio::ip::tcp;

using hires_clock = std::chrono::high_resolution_clock;

// An agent for resolving host names into IP addresses.
//
class resolver_t final : public so_5::agent_t
{
public :
	// A message to be used for new request to resolver.
	struct resolve final : public so_5::message_t
	{
		// Mbox for reply.
		so_5::mbox_t m_reply_to;
		// Host name to be resolved.
		std::string m_what;
		// ID of request (to be used in response).
		std::size_t m_index;

		resolve( so_5::mbox_t reply_to, std::string what, std::size_t index )
			:	m_reply_to(std::move(reply_to))
			,	m_what(std::move(what))
			,	m_index(index)
			{}
	};

	// A reply for successful resolving result.
	struct resolve_successed final : public so_5::message_t
	{
		// ID of request.
		std::size_t m_index;
		// Address of the host.
		asio::ip::address m_result;

		resolve_successed( std::size_t index, asio::ip::address result )
			:	m_index(index)
			,	m_result(std::move(result))
		{}
	};

	// A reply for negative resolving result.
	struct resolve_failed : public so_5::message_t
	{
		// ID of request.
		std::size_t m_index;
		// Description of the problem.
		std::string m_description;

		resolve_failed( std::size_t index, std::string description )
			:	m_index(index)
			,	m_description(std::move(description))
		{}
	};

	resolver_t( context_t ctx, asio::io_context & io_context )
		:	so_5::agent_t( ctx )
		,	m_resolver( io_context )
	{
		// A subscription to just one message is needed.
		so_subscribe_self().event( &resolver_t::on_resolve );
	}

private :
	// Actual resolver to be used.
	tcp::resolver m_resolver;

	void
	on_resolve( const resolve & msg )
	{
		// Timeout for resolve operation will be ignored.
		m_resolver.async_resolve(
				msg.m_what,
				std::string() /* no service name or port */,
				tcp::resolver::numeric_service |
						tcp::resolver::address_configured,
				[reply_to = msg.m_reply_to, index = msg.m_index](
					const asio::error_code & ec,
					tcp::resolver::results_type results )
				{
					handle_resolve_result( reply_to, index, ec, results );
				} );
	}

	static void
	handle_resolve_result(
		const so_5::mbox_t & reply_to,
		std::size_t index,
		const asio::error_code & ec,
		tcp::resolver::results_type results )
	{
		if( !ec )
			so_5::send< resolve_successed >( reply_to, index,
					results.begin()->endpoint().address() );
		else
		{
			std::ostringstream s;
			s << ec;

			so_5::send< resolve_failed >( reply_to, index, s.str() );
		}
	}
};

// An agent which initiates requests for host name resolving and
// collects resolving results.
//
class resolve_request_manager_t final : public so_5::agent_t
{
	// Message about too long resolving for a host.
	struct resolve_timeout final : public so_5::message_t
	{
		std::size_t m_index;

		resolve_timeout(std::size_t index) : m_index(index) {}
	};

public :
	resolve_request_manager_t(
		// SObjectizer Environment to work in.
		context_t ctx,
		// Mbox of resolver agent.
		so_5::mbox_t resolver,
		// List of host names to be processed.
		std::vector< std::string > host_names )
		:	so_5::agent_t( std::move(ctx) )
		,	m_resolver( std::move(resolver) )
		,	m_data( make_data( std::move(host_names) ) )
	{
		// Create all necessary subscriptions.
		so_subscribe_self()
			.event( &resolve_request_manager_t::on_resolve_successed )
			.event( &resolve_request_manager_t::on_resolve_failed )
			.event( &resolve_request_manager_t::on_resolve_timeout );
	}

	virtual void
	so_evt_start() override
	{
		// The first N requests must be sent at the start.
		initiate_some_requests();
	}

private :
	// Description of one host name to be processed.
	struct host_t
	{
		enum class status_t
		{
			not_processed_yet,
			in_progress,
			resolved,
			resolving_failed
		};

		// Name of the host to be resolved.
		std::string m_name;
		// Status of this request.
		status_t m_status = status_t::not_processed_yet;
		// Time point at which request initiated.
		// Will be set only when request is sent.
		hires_clock::time_point m_started_at;
		// Timer ID for timeout message. Will be used to cancel
		// the timeout when reply from resolver received.
		so_5::timer_id_t m_timeout_timer;

		host_t() = default;
		host_t( std::string name ) : m_name( std::move(name) ) {}
	};

	// A type of container for information about hosts to be resolved.
	using data_container_t = std::vector< host_t >;

	const so_5::mbox_t m_resolver;

	// Data to be processed.
	data_container_t m_data;

	// Index of the first non-processed item.
	std::size_t m_first_unprocessed = 0u;
	// Count of items which are being processed.
	std::size_t m_in_progress_now = 0u;

	static data_container_t
	make_data( std::vector< std::string > host_names )
	{
		data_container_t result;
		result.reserve( host_names.size() );

		std::transform( std::begin(host_names), std::end(host_names),
				std::back_inserter(result),
				[]( auto & name ) { return host_t( std::move(name) ); } );

		return result;
	}

	static auto
	ms( const hires_clock::duration duration )
	{
		return std::chrono::duration_cast< std::chrono::milliseconds >(
				duration ).count();
	}

	void
	initiate_some_requests()
	{
		static constexpr std::size_t total_in_progress = 3u;

		if( m_first_unprocessed == m_data.size() && 0 == m_in_progress_now )
			// There is no more work to do.
			so_deregister_agent_coop_normally();
		else
		{
			// Send some more requests.
			while( m_first_unprocessed < m_data.size() &&
					m_in_progress_now < total_in_progress )
				send_next_unprocessed();
		}
	}

	void
	send_next_unprocessed()
	{
		auto & item = m_data[ m_first_unprocessed ];

		// Update status of request.
		item.m_status = host_t::status_t::in_progress;
		item.m_started_at = hires_clock::now();

		// Initiate request to the resolver.
		so_5::send< resolver_t::resolve >( m_resolver,
				so_direct_mbox(),
				item.m_name,
				m_first_unprocessed );

		// Start timeout for that request.
		item.m_timeout_timer = so_5::send_periodic< resolve_timeout >(
				*this,
				std::chrono::seconds(15),
				std::chrono::seconds::zero(),
				m_first_unprocessed );

		// Update the state of manager.
		++m_first_unprocessed;
		++m_in_progress_now;
	}

	void
	on_resolve_successed( mhood_t<resolver_t::resolve_successed> cmd )
	{
		handle_result(
			*cmd,
			[this]( const auto & result, auto & item, const auto duration ) {
				item.m_status = host_t::status_t::resolved;

				std::cout << item.m_name << " -> " << result.m_result
						<< " (" << duration << "ms)"
						<< std::endl;
			} );
	}

	void
	on_resolve_failed( mhood_t<resolver_t::resolve_failed> cmd )
	{
		handle_result(
			*cmd,
			[this]( const auto & result, auto & item, const auto duration ) {
				item.m_status = host_t::status_t::resolving_failed;

				std::cout << item.m_name << " FAILURE: " << result.m_description
						<< " (" << duration << "ms)"
						<< std::endl;
			} );
	}

	void
	on_resolve_timeout( mhood_t<resolve_timeout> cmd )
	{
		handle_result(
			*cmd,
			[this]( const auto & /*result*/, auto & item, const auto duration ) {
				item.m_status = host_t::status_t::resolving_failed;

				std::cout << item.m_name << " FAILURE: TIMEOUT"
						<< " (" << duration << "ms)"
						<< std::endl;
			} );
	}

	template< typename R, typename L >
	void
	handle_result( const R & msg, L && lambda )
	{
		const auto result_at = hires_clock::now();

		auto & item = m_data[ msg.m_index ];
		item.m_timeout_timer.release();
		if( host_t::status_t::in_progress != item.m_status )
			return;

		--m_in_progress_now;

		lambda( msg, item, ms( result_at - item.m_started_at ) );

		initiate_some_requests();
	}
};

// Just a helper to transform argv into vector of std::string.
auto
argv_to_host_name_list( int argc, char ** argv )
{
	if( 1 == argc )
		throw std::runtime_error( "a list of host names must be passed in "
				"command line" );

	std::vector< std::string > hosts;
	std::transform( &argv[1], &argv[argc],
			std::back_inserter(hosts),
			[](const char * n) { return std::string(n); } );

	return hosts;
}

int main( int argc, char ** argv )
{
	try
	{
		auto hosts = argv_to_host_name_list( argc, argv );

		// This io_context will be used by SObjectizer infrastructure.
		asio::io_context io_svc;

		so_5::launch( [&](so_5::environment_t & env) {
				// Create a coop with resolver and manager.
				env.introduce_coop( [&]( so_5::coop_t & coop ) {
						auto resolver = coop.make_agent< resolver_t >( std::ref(io_svc) );
						coop.make_agent< resolve_request_manager_t >(
							resolver->so_direct_mbox(),
							std::move( hosts ) );
					} );
			},
			[&io_svc](so_5::environment_params_t & params) {
				// Setup single-threaded Asio-based infrastructure.
				using namespace so_5::extra::env_infrastructures::asio::simple_not_mtsafe;

				params.infrastructure_factory( factory(io_svc) );
			} );
	}
	catch( const std::exception & x )
	{
		std::cout << "Exception caught: " << x.what() << std::endl;
	}

	return 0;
}

