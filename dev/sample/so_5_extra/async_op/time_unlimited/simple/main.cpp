/*
 * Simple example of usage of async_op::time_unlimited stuff.
 */

#include <so_5_extra/async_op/time_unlimited.hpp>

#include <so_5/all.hpp>

// Class for service providers.
// Instances of that class will respond to ask_service messages.
class service_provider_t final : public so_5::agent_t
{
public:
	// A signal with a request for service to be provided.
	struct ask_service final : public so_5::signal_t {};
	// A signal about provision of a service.
	struct service_ack final : public so_5::signal_t {};

	service_provider_t(
		context_t ctx,
		const std::string & service_name)
		:	so_5::agent_t(std::move(ctx))
	{
		// A named mbox is necessary for providing service.
		const auto service_mbox = so_environment().create_mbox(service_name);
		// Create a subscription for service requests.
		so_subscribe(service_mbox).event([service_mbox](mhood_t<ask_service>) {
			// Just reply with a signal.
			so_5::send<service_ack>(service_mbox);
		});
	}
};

// A demo for services customer.
class customer_t final : public so_5::agent_t
{
public:
	customer_t(context_t ctx) : so_5::agent_t(std::move(ctx)) {}

	virtual void so_evt_start() override
	{
		// Do several service requests as async operations.
		// Do an async op with the first service provider.
		initiate_async_op_for("alpha");
		// Do the same thing with the second provider.
		initiate_async_op_for("beta");
	}
private:
	int m_acks_received = 0;

	void initiate_async_op_for(const std::string & service_name)
	{
		// Mbox of a service provider.
		const auto service_mbox = so_environment().create_mbox(service_name);

		// Prepare async operation and activate it.
		// Note that operation object create by make() is not stored.
		// There is no need for that because we do not cancel nor reuse
		// the operation. The instance of operation object will be
		// destroyed automatically.
		namespace asyncop = so_5::extra::async_op::time_unlimited;
		asyncop::make(*this).completed_on(
				service_mbox,
				so_default_state(),
				[this, service_name](mhood_t<service_provider_t::service_ack>) {
					std::cout << "ack from a service provider: " << service_name << std::endl;
					++m_acks_received;
					if(2 == m_acks_received)
						so_deregister_agent_coop_normally();
				})
			.activate([&]{
					// Actual start of async operation.
					so_5::send<service_provider_t::ask_service>(service_mbox);
				});
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
				make_dispatcher(env).binder(
					bind_params_t{}.fifo(fifo_t::individual)),
				[](so_5::coop_t & coop) {
					// Create two service providers to be used in example.
					coop.make_agent<service_provider_t>("alpha");
					coop.make_agent<service_provider_t>("beta");

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

