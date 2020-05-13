#include "a.hpp"

namespace test_coop_a {

class test_agent final : public so_5::agent_t
	{
	public :
		test_agent( context_t ctx )
			:	so_5::agent_t( std::move(ctx) )
			{}

		virtual void
		so_evt_start() override
			{
				std::cout << "test_coop_a::test_agent::evt_start" << std::endl;
				so_deregister_agent_coop_normally();
			}
	};

} /* namespace test_coop_a */

void make_coop_a(
	so_5::environment_t & env,
	so_5::extra::disp::asio_one_thread::dispatcher_handle_t disp )
	{
		env.introduce_coop( [&]( so_5::coop_t & coop )
			{
				coop.make_agent_with_binder< test_coop_a::test_agent >(
						disp.binder() );
			} );
	}

