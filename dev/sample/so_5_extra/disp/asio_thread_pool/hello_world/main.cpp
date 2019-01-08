/*
 * A very simple example of usage of Asio-based thread pool dispatcher.
 */
#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

#include <so_5/all.hpp>

// Type of agent to be used in example.
class a_hello_t final : public so_5::agent_t
{
	struct hello final : public so_5::signal_t {};

public :
	using so_5::agent_t::agent_t;

	void so_define_agent() override
	{
		so_subscribe_self().event( &a_hello_t::on_hello );
	}

	void so_evt_start() override
	{
		std::cout << "Start" << std::endl;

		so_5::send< hello >( *this );
	}

	void so_evt_finish() override
	{
		std::cout << "Finish" << std::endl;
	}

private :
	void on_hello( mhood_t<hello> )
	{
		std::cout << "Hello" << std::endl;
		so_deregister_agent_coop_normally();
	}
};

int main()
{
	// IO-context to be used for thread-pool dispatcher.
	asio::io_context io_svc;
	// Strand object for hello-agent.
	asio::io_context::strand actor_strand{ io_svc };

	so_5::launch( [&](so_5::environment_t & env) {
		namespace asio_tp = so_5::extra::disp::asio_thread_pool;

		// Create dispatcher instance.
		// That instance will use external io_context object.
		auto disp = asio_tp::create_private_disp(
				env,
				"asio_tp",
				asio_tp::disp_params_t{}.use_external_io_context( io_svc ) );

		// Create hello-agent that will be bound to thread pool dispatcher.
		env.introduce_coop(
				// Agent will be protected by strand-object.
				disp->binder( actor_strand ),
				[&]( so_5::coop_t & coop )
				{
					coop.make_agent<a_hello_t>();
				} );
	} );
}

