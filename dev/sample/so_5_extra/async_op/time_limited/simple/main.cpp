/*
 * Simple example of usage of async_op::time_unlimited stuff.
 */

#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

using namespace std::literals::chrono_literals;

// Class for service providers.
// Instances of that class will respond to ask_service messages.
// Response will be delayed for some time. The duration of the
// response delay is set in the constructor.
class service_provider_t final : public so_5::agent_t
{
	const std::chrono::steady_clock::duration m_reply_delay;

public:
	// A signal with a request for service to be provided.
	struct ask_service final : public so_5::signal_t {};
	// A signal about provision of a service.
	struct service_ack final : public so_5::signal_t {};

	service_provider_t(
		context_t ctx,
		const std::string & service_name,
		std::chrono::steady_clock::duration reply_delay)
		:	so_5::agent_t(std::move(ctx))
		,	m_reply_delay(reply_delay)
	{
		// A named mbox is necessary for providing service.
		const auto service_mbox = so_environment().create_mbox(service_name);
		// Create a subscription for service requests.
		so_subscribe(service_mbox).event(
			[this,service_mbox](mhood_t<ask_service>) {
				// Just reply with a signal.
				so_5::send_delayed<service_ack>(
						so_environment(),
						service_mbox,
						m_reply_delay);
			});
	}
};

// A demo for services customer.
class customer_t final : public so_5::agent_t
{
	// This message will signal about service timeout.
	struct service_timedout final : public so_5::message_t
	{
		const std::string m_service_name;

		service_timedout(std::string name) : m_service_name(std::move(name)) {}
	};

	// This signal will be used for completion of the example.
	struct finish final : public so_5::signal_t {};

public:
	customer_t(context_t ctx) : so_5::agent_t(std::move(ctx))
	{
		so_subscribe_self().event([this](mhood_t<finish>) {
			so_deregister_agent_coop_normally();
		});
	}

	virtual void so_evt_start() override
	{
		// Do several service requests as async operations.
		// Do an async op with the first service provider.
		initiate_async_op_for("alpha");
		// Do the same thing with the second provider.
		initiate_async_op_for("beta");
		// Do the same thing with the third provider.
		initiate_async_op_for("gamma");
	}

private:
	int m_acks_received = 0;

	void initiate_async_op_for(const std::string & service_name)
	{
		// Mbox of a service provider.
		const auto service_mbox = so_environment().create_mbox(service_name);

		// Prepare async operation and activate it.
		// Note that operation object create by make() is not stored.
		// There is no need for that because we do not cancel
		// the operation.
		namespace asyncop = so_5::extra::async_op::time_limited;
		asyncop::make<service_timedout>(*this)
			// Define a completion handler for reply.
			.completed_on(
				service_mbox,
				so_default_state(),
				[this, service_name](mhood_t<service_provider_t::service_ack>) {
					std::cout << "ack from a service provider: "
							<< service_name << std::endl;
					on_ack_or_timeout();
				})
			// Define a timeout handler for absense of a reply.
			.timeout_handler(
				so_default_state(),
				[this](mhood_t<service_timedout> cmd) {
					std::cout << "*** no reply from service provider: "
							<< cmd->m_service_name << std::endl;
					on_ack_or_timeout();
				})
			// Now the operation can be activated.
			.activate(
				// Timeout for the whole operation.
				250ms,
				// Args for service_timedout's constructor.
				service_name);

			// Actual start of async operation.
			so_5::send<service_provider_t::ask_service>(service_mbox);
	}

	void on_ack_or_timeout()
	{
		++m_acks_received;
		if(3 == m_acks_received)
			so_5::send_delayed<finish>(*this, 200ms);
	}
};

int main()
{
	try
	{
		so_5::launch([](so_5::environment_t & env) {
			// All agents from example coop will work on thread pool dispatcher.
			using namespace so_5::disp::thread_pool;
			env.introduce_coop(
				// Dispatcher and binder for agents from example coop.
				create_private_disp(env)->binder(
					bind_params_t{}.fifo(fifo_t::individual)),
				[](so_5::coop_t & coop) {
					// Create two service providers to be used in example.
					coop.make_agent<service_provider_t>("alpha", 100ms);
					coop.make_agent<service_provider_t>("beta", 200ms);
					coop.make_agent<service_provider_t>("gamma", 300ms);

					// Main example agent.
					coop.make_agent<customer_t>();
				});
		});

		return 0;
	}
	catch(const std::exception & ex)
	{
		std::cerr << "Exception caught: " << ex.what() << std::endl;
	}

	return 2;
}

