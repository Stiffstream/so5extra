/*
 * A very simple usage of so_5::extra::sync::request_reply_t
 * without agents, just a plain std::thread and so_5::mchain.
 */

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

// Short alias for convenience.
namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

// The service provider thread.
void service_provider(
	so_5::mchain_t in_chain,
	int multiplier )
{
	// Handle all requests until the chain will be closed.
	receive( from(in_chain).handle_all(),
		[multiplier](sync_ns::request_mhood_t<int, std::string> cmd) {
			// Transform the incoming value, convert the result
			// to string and send the resulting string back.
			cmd->make_reply(std::to_string(cmd->request() * multiplier));
		} );
}

int main()
{
	so_5::wrapped_env_t sobj;

	// Holders for service providers threads.
	// Actual threads will be started later.
	std::thread provider1, provider2;
	// Threads should be automatically joined.
	auto joiner = so_5::auto_join(provider1, provider2);

	// Input chains for service providers.
	auto in1 = so_5::create_mchain(sobj);
	auto in2 = so_5::create_mchain(sobj);
	// Chains should be closed automatically.
	auto closer = so_5::auto_close_drop_content(in1, in2);

	// Now we can start servce providers threads.
	provider1 = std::thread{ service_provider, in1, 2 };
	provider2 = std::thread{ service_provider, in2, 3 };

	// Perform some requests.
	std::cout
		<< "First provider reply: "
		<< sync_ns::request_reply<int, std::string>(in1, 1s, 3)
		<< std::endl
		<< "Second provider reply: "
		<< sync_ns::request_reply<int, std::string>(in2, 1s, 3)
		<< std::endl;

	return 0;
}

