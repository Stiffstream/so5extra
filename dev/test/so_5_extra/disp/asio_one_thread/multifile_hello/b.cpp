#include "a.hpp"

namespace test_coop_b {

class test_agent final : public so_5::agent_t
	{
	public :
		test_agent( context_t ctx, int ordinal )
			:	so_5::agent_t( std::move(ctx) )
			,	m_ordinal( ordinal )
			{}

		virtual void
		so_evt_start() override
			{
				std::cout << "test_coop_b::test_agent::evt_start("
						<< m_ordinal << ")" << std::endl;
				so_deregister_agent_coop_normally();
			}

	private :
		const int m_ordinal;
	};

} /* namespace test_coop_b */

void make_coop_b(
	so_5::environment_t & env,
	so_5::extra::disp::asio_one_thread::dispatcher_handle_t disp )
	{
		env.introduce_coop( [&]( so_5::coop_t & coop )
			{
				for( int i = 0; i != 3; ++i )
				{
					coop.make_agent_with_binder< test_coop_b::test_agent >(
							disp.binder(), i );
				}
			} );
	}

