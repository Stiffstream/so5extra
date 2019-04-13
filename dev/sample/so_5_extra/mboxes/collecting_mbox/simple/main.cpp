/*
 * An example of using collecting_mbox.
 *
 * Parent agent of type sample_performer_t creates N children agents.
 * Every child agent sents a child_started_t signal to the parent.
 * When all N child_started_t signals are received the parent agent
 * stops the example.
 */

#include <so_5_extra/mboxes/collecting_mbox.hpp>

#include <so_5/all.hpp>

// Type of signal about readyness of child agent.
struct child_started_t final : public so_5::signal_t {};

// Type of a child agent.
class child_t final : public so_5::agent_t
{
public:
	child_t(context_t ctx, so_5::mbox_t ready_mbox)
		:	so_5::agent_t(std::move(ctx))
		,	m_ready_mbox(std::move(ready_mbox))
	{}

	virtual void so_evt_start() override
	{
		// Parent must be informed about child readyness.
		so_5::send<child_started_t>(m_ready_mbox);
	}

private:
	const so_5::mbox_t m_ready_mbox;
};

// Type of example performer agent.
class sample_performer_t final : public so_5::agent_t
{
	// Type of collecting mbox template for child_started_t signal.
	using child_started_mbox_t =
		so_5::extra::mboxes::collecting_mbox::mbox_template_t<child_started_t>;

public:
	sample_performer_t(context_t ctx, std::size_t child_count)
		:	so_5::agent_t(std::move(ctx))
		,	m_child_count(child_count)
	{
		so_subscribe_self()
			.event(&sample_performer_t::on_all_child_started);
	}

	virtual void so_evt_start() override
	{
		// Create instance of collecting mbox.
		auto ready_mbox = child_started_mbox_t::make(
				so_environment(),
				so_direct_mbox(),
				m_child_count);

		// All children agents will work on the same thread_pool dispatcher.
		auto tp_disp = so_5::disp::thread_pool::create_private_disp(
				so_environment(),
				3 );
		for(std::size_t i = 0; i != m_child_count; ++i)
		{
			introduce_child_coop(*this,
					tp_disp->binder(so_5::disp::thread_pool::bind_params_t{}),
					[&ready_mbox](so_5::coop_t & coop) {
						coop.make_agent<child_t>(ready_mbox);
					});
		}

		std::cout << "All children agents are created" << std::endl;
	}

private:
	// Count of children agents to be created.
	const std::size_t m_child_count;

	void on_all_child_started(mhood_t<child_started_mbox_t::messages_collected_t>)
	{
		std::cout << "All children agents are started" << std::endl;
		so_deregister_agent_coop_normally();
	}
};

int main()
{
	try
	{
		so_5::launch([](so_5::environment_t & env) {
			// Make parent coop.
			env.introduce_coop([&](so_5::coop_t & coop) {
				// Example performer will work on the default dispatcher.
				coop.make_agent<sample_performer_t>(25u);
			});
		});
	}
	catch(const std::exception & x)
	{
		std::cerr << "Exception caught: " << x.what() << std::endl;
	}
}

