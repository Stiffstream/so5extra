#include "a.hpp"

namespace test_coop_a {

class test_agent final : public so_5::agent_t
	{
	public :
		test_agent( context_t ctx, asio::io_context & io_svc )
			:	so_5::agent_t( std::move(ctx) )
			,	m_strand( io_svc )
			{}

		virtual void
		so_evt_start() override
			{
				std::cout << "test_coop_a::test_agent::evt_start" << std::endl;
				so_deregister_agent_coop_normally();
			}

		asio::io_context::strand &
		strand() noexcept { return m_strand; }

	private :
		asio::io_context::strand m_strand;
	};

} /* namespace test_coop_a */

void make_coop_a(
	so_5::environment_t & env,
	so_5::extra::disp::asio_thread_pool::private_dispatcher_handle_t disp )
	{
		env.introduce_coop( [&]( so_5::coop_t & coop )
			{
				auto agent = std::make_unique< test_coop_a::test_agent >(
						env, disp->io_context() );
				auto binder = disp->binder( agent->strand() );

				coop.add_agent( std::move(agent), std::move(binder) );
			} );
	}

