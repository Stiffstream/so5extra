/*
 * An example of using Asio-based single-threaded and thread-safe
 * infrastructure.
 *
 * This example resolves names of hosts into IP addresses. User enters host
 * name in dialog on the context of the main thread. This name is passed to
 * SObjectizer's agent which works on different thread. The result is sent
 * back via mchain.
 */
#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <asio.hpp>
#include <asio/ip/tcp.hpp>

#include <sstream>

using asio::ip::tcp;

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

		resolve( so_5::mbox_t reply_to, std::string what )
			:	m_reply_to(std::move(reply_to))
			,	m_what(std::move(what))
			{}
	};

	// A reply for successful resolving result.
	struct resolve_successed final : public so_5::message_t
	{
		// Original host name.
		std::string m_what;
		// Address of the host.
		asio::ip::address m_result;

		resolve_successed( std::string what, asio::ip::address result )
			:	m_what(std::move(what))
			,	m_result(std::move(result))
		{}
	};

	// A reply for negative resolving result.
	struct resolve_failed : public so_5::message_t
	{
		// Original host name.
		std::string m_what;
		// Description of the problem.
		std::string m_description;

		resolve_failed( std::string what, std::string description )
			:	m_what(std::move(what))
			,	m_description(std::move(description))
		{}
	};

	resolver_t( context_t ctx, asio::io_context & io_service )
		:	so_5::agent_t( ctx )
		,	m_resolver( io_service )
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
				[reply_to = msg.m_reply_to, what = msg.m_what](
					const asio::error_code & ec,
					tcp::resolver::results_type results )
				{
					handle_resolve_result( reply_to, std::move(what), ec, results );
				} );
	}

	static void
	handle_resolve_result(
		const so_5::mbox_t & reply_to,
		std::string what,
		const asio::error_code & ec,
		tcp::resolver::results_type results )
	{
		if( !ec )
			so_5::send< resolve_successed >(
					reply_to,
					std::move(what),
					results.begin()->endpoint().address() );
		else
		{
			std::ostringstream s;
			s << ec;

			so_5::send< resolve_failed >(
					reply_to,
					std::move(what),
					s.str() );
		}
	}
};

// Launch separate thread on which the SObjectizer instance will work.
//
std::tuple<std::thread, so_5::environment_t *, so_5::mbox_t, so_5::mchain_t>
launch_sobjectizer()
{
	// Mutex and condition variable are necessary for safe synchronization
	// with a separate thread.
	std::mutex data_lock;
	std::condition_variable sobj_started;

	so_5::environment_t * penv{};
	so_5::mbox_t resolver_mbox;
	so_5::mchain_t reply_ch;

	// This lock stops the separate thread util this thread will
	// call sobj_started.wait().
	std::unique_lock< std::mutex > lock1{ data_lock };

	std::thread sobj_thread{ [&] {
		std::cout << "SObjectizer thread started" << std::endl;

		// This io_service will be used by SObjectizer infrastructure.
		asio::io_context io_svc;

		so_5::launch(
			[&](so_5::environment_t & env) {
				std::lock_guard< std::mutex > lock2{ data_lock };

				// Pointer to the environment must be returned.
				penv = &env;

				// Mchain for replies from resolver must be created.
				reply_ch = so_5::create_mchain( env );

				// Create a coop with resolver and manager.
				env.introduce_coop( [&]( so_5::coop_t & coop ) {
						auto resolver = coop.make_agent< resolver_t >( std::ref(io_svc) );
						resolver_mbox = resolver->so_direct_mbox();
					} );

				// Waiting thread must be informed that all variables have its values.
				sobj_started.notify_one();
			},
			[&io_svc](so_5::environment_params_t & params) {
				// Setup single-threaded Asio-based infrastructure.
				using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;

				params.infrastructure_factory( factory(io_svc) );
			} );

		std::cout << "SObjectizer thread finished" << std::endl;
	} };

	// Wait for start of sobjectizer.
	sobj_started.wait( lock1, [&]{ return penv != nullptr; } );

	// Now all variables have its values. We can return them.
	return std::make_tuple( std::move(sobj_thread), penv, resolver_mbox, reply_ch );
}

void
do_sample()
{
	std::thread sobj_thread;
	so_5::environment_t * penv;
	so_5::mbox_t resolver_mbox;
	so_5::mchain_t reply_ch;

	// Launch SObjectizer on the separate thread.
	std::tie(sobj_thread, penv, resolver_mbox, reply_ch) = launch_sobjectizer();
	// SObjectizer's thread must be automatically joined.
	auto sobj_thread_joiner = so_5::auto_join(sobj_thread);

	// Start interactive dialog with user.
	while( true )
	{
		std::cout << "Enter host name or 'quit' for exit: " << std::flush;
		std::string host_name;
		if( std::getline(std::cin, host_name) )
		{
			if( host_name.empty() )
				continue;

			if( "quit" == host_name )
				break;

			// User enters a host name. It must be resolved.
			// Send a resolve request.
			so_5::send< resolver_t::resolve >(
					resolver_mbox,
					reply_ch->as_mbox(), // reply_to
					std::move(host_name) ); // host name to be resolved.

			// Wait for resolve result.
			so_5::receive( from(reply_ch).handle_n(1),
					[]( so_5::mhood_t<resolver_t::resolve_successed> cmd ) {
						std::cout << "Successed: '" << cmd->m_what << "' -> "
								<< cmd->m_result << std::endl;
					},
					[]( so_5::mhood_t<resolver_t::resolve_failed> cmd ) {
						std::cout << "Failed: '" << cmd->m_what << "', "
								<< cmd->m_description << std::endl;
					} );
		}
		else
			break;
	}	

	std::cout << "Stopping SObjectizer..." << std::endl;
	penv->stop();
}

int main()
{
	try
	{
		do_sample();
	}
	catch( const std::exception & x )
	{
		std::cout << "Exception caught: " << x.what() << std::endl;
	}

	return 0;
}

