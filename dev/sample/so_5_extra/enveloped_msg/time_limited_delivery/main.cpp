/*
 * Simple demo for time_limited_delivery_t envelope.
 */

#include <so_5_extra/enveloped_msg/time_limited_delivery.hpp>
#include <so_5_extra/enveloped_msg/send_functions.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

namespace envelope_ns = so_5::extra::enveloped_msg;

int main()
{
	// Lauch empty SObjectizer Environment.
	so_5::wrapped_env_t sobj;

	// Create mchain to be used for message delivery.
	auto ch = create_mchain(sobj);

	// Send a couple of messages with different deadlines.
	envelope_ns::make<std::string>("Hello!")
			.envelope<envelope_ns::time_limited_delivery_t>(250ms)
			.send_to(ch);
	envelope_ns::make<std::string>("Bye!")
			.envelope<envelope_ns::time_limited_delivery_t>(2s)
			.send_to(ch);

	// There should be 2 messages in mchain.
	std::cout << "ch.size: " << ch->size() << std::endl;

	// Sleep for 1s. The first message should be expired.
	std::this_thread::sleep_for(1s);

	// Try to process messages from mchain.
	// Just one message should be processed.
	const auto receive_result =
			receive(from(ch).no_wait_on_empty().handle_all(),
					[](so_5::mhood_t<std::string> cmd) {
						std::cout << "Msg: " << *cmd << std::endl;
					});

	std::cout << "messages extracted: " << receive_result.extracted()
			<< ", handled: " << receive_result.handled() << std::endl;

	return 0;
}

