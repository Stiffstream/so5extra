#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

using ask_reply_t = sync_ns::request_reply_t<int, int>;

class service_t final : public so_5::agent_t
	{
	public :
		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self().event( &service_t::on_request );
			}

	private :
		void
		on_request( typename ask_reply_t::request_mhood_t cmd )
			{
				cmd->make_reply( cmd->request() * 2 );
			}
	};

class consumer_t final : public so_5::agent_t
	{
	public :
		consumer_t( context_t ctx, so_5::mbox_t svc, int & receiver )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_svc{ std::move(svc) }
			,	m_receiver{ receiver }
			{
				so_subscribe_self().event( &consumer_t::on_reply );
			}

		void
		so_evt_start() override
			{
				ask_reply_t::initiate_with_custom_reply_to(
						m_svc,
						so_direct_mbox(),
						2 );
			}

	private :
		const so_5::mbox_t m_svc;
		int & m_receiver;

		void
		on_reply( typename ask_reply_t::reply_mhood_t cmd )
			{
				m_receiver = *cmd;

				so_deregister_agent_coop_normally();
			}
	};

TEST_CASE( "simple shutdown on empty environment" )
{
	int result{};

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [&](so_5::coop_t & coop) {
								auto svc = coop.make_agent<service_t>()->so_direct_mbox();
								coop.make_agent<consumer_t>( svc, std::ref(result) );
							} );
					} );
		},
		5 );

	REQUIRE( result == 4 );
}

