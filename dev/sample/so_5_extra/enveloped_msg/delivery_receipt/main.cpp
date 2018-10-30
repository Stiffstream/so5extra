/*
 * A demo for enveloped messages.
 */

#include <so_5_extra/enveloped_msg/just_envelope.hpp>
#include <so_5_extra/enveloped_msg/send_functions.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

namespace envelope_ns = so_5::extra::enveloped_msg;

using request_id_t = int;

// Type of request to be processed.
struct request_t final
{
	request_id_t m_id;
	std::string m_data;
};

// Message to be used as delivery receipt for request delivery.
struct delivery_receipt_t final
{
	// ID of delivered request.
	request_id_t m_id;
};

// Agent to process requests.
class processor_t final : public so_5::agent_t
{
	// Normal state. Agent accepts new requests in that state.
	state_t st_normal{this, "normal"};
	// Busy state. Agent don't accepts new requests in that state.
	state_t st_busy{this, "busy"};

public:
	processor_t(context_t ctx) : so_5::agent_t{std::move(ctx)}
	{
		this >>= st_normal;

		st_normal.event(&processor_t::on_request);

		// No event handlers for st_busy.
		// But time for standing in st_busy is limited.
		st_busy.time_limit(2s, st_normal);
	}

private:
	void on_request(mhood_t<request_t> cmd)
	{
		std::cout << "processor: on_request(" << cmd->m_id << ", "
				<< cmd->m_data << ")" << std::endl;

		this >>= st_busy;
	}
};

// A custom envelope for sending delivery_receipt.
class custom_envelope_t final : public envelope_ns::just_envelope_t
{
	// Destination for delivery receipt.
	const so_5::mbox_t m_to;

	// ID of delivered request.
	const request_id_t m_id;

public:
	custom_envelope_t(
		so_5::message_ref_t payload,
		so_5::mbox_t to,
		request_id_t id)
		:	envelope_ns::just_envelope_t{std::move(payload)}
		,	m_to{std::move(to)}
		,	m_id{id}
	{}

	void
	access_hook(
		access_context_t context,
		handler_invoker_t & invoker) noexcept override
	{
		if(access_context_t::handler_found == context)
		{
			// Send delivery receipt.
			so_5::send<delivery_receipt_t>(m_to, m_id);
		}
		// Delegate an actual work to base class.
		envelope_ns::just_envelope_t::access_hook(context, invoker);
	}
};

// Agent to issue requests and resend them after some time.
class requests_generator_t final : public so_5::agent_t
{
	// Processor's mbox.
	const so_5::mbox_t m_processor;

	// Map of requests in flight.
	std::map<request_id_t, std::string> m_requests;

	struct resend_requests final : public so_5::signal_t {};

public:
	requests_generator_t(context_t ctx, so_5::mbox_t processor)
		:	so_5::agent_t{std::move(ctx)}
		,	m_processor{std::move(processor)}
	{
		so_subscribe_self()
			.event(&requests_generator_t::on_delivery_receipt)
			.event(&requests_generator_t::on_resend);
	}

	void so_evt_start() override
	{
		// Create requests to be delivered to processor.
		m_requests.emplace(0, "First");
		m_requests.emplace(1, "Second");
		m_requests.emplace(2, "Third");
		m_requests.emplace(3, "Four");

		// Send requests to processor.
		send_requests();
	}

private:
	void on_delivery_receipt(mhood_t<delivery_receipt_t> cmd)
	{
		std::cout << "request delivered: " << cmd->m_id << std::endl;
		m_requests.erase(cmd->m_id);

		if(m_requests.empty())
			// No more requests. Work can be finished.
			so_deregister_agent_coop_normally();
	}

	void on_resend(mhood_t<resend_requests>)
	{
		std::cout << "time to resend requests, pending requests: "
				<< m_requests.size() << std::endl;

		send_requests();
	}

	void send_requests()
	{
		for(const auto & item : m_requests)
		{
			std::cout << "sending request: (" << item.first << ", "
					<< item.second << ")" << std::endl;

			envelope_ns::make<request_t>(item.first, item.second)
					.envelope<custom_envelope_t>(so_direct_mbox(), item.first)
					.send_to(m_processor);
		}

		// Send delayed message to resend non-delivered requests later.
		so_5::send_delayed<resend_requests>(*this, 3s);
	}
};

int main()
{
	so_5::launch([](so_5::environment_t & env) {
		env.introduce_coop([](so_5::coop_t & coop) {
			auto processor = coop.make_agent<processor_t>();
			coop.make_agent<requests_generator_t>(processor->so_direct_mbox());
		});
	});
}

