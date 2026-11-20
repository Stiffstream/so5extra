#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

namespace test
{

using my_request = sync_ns::request_reply_t<int, int>;

class service_t final : public so_5::agent_t
	{
	public :
		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self().event( &service_t::on_request );
			}

	private :
		std::vector< typename my_request::holder_t > m_requests;

		void
		on_request( sync_ns::request_mhood_t<int, int> cmd )
			{
				m_requests.push_back( cmd.make_holder() );

				if( 2u == m_requests.size() )
					{
						for( auto & req : m_requests )
							req->make_reply( req->request() * 2 );
						so_deregister_agent_coop_normally();
					}
			}
	};

class client_t final : public so_5::agent_t
	{
	public :
		client_t( context_t ctx, so_5::mbox_t service, int v )
			:	so_5::agent_t{ ctx + so_5::enable_cpp_coro() }
			,	m_service{ std::move(service) }
			,	m_value{ v }
			{}

		so_5::cpp_coro::task_t< void >
		so_evt_async_start() override
			{
				auto r = co_await my_request::ask_value_async( m_service, 10s, m_value );
				ensure_or_die( r == m_value*2,
						"unexpected result for " + std::to_string(m_value) +
						"; result=" + std::to_string(r) );

				so_deregister_agent_coop_normally();
			}

	private :
		const so_5::mbox_t m_service;
		const int m_value;
	};

} /* namespace test */

using namespace test;

TEST_CASE( "simple holder" )
{
	int result{};

	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						auto svc = env.introduce_coop(
							[](so_5::coop_t & coop) {
								return coop.make_agent<service_t>()->so_direct_mbox();
							} );
						env.introduce_coop(
							[svc](so_5::coop_t & coop) {
								return coop.make_agent<client_t>(svc, 2);
							} );
						env.introduce_coop(
							[svc](so_5::coop_t & coop) {
								return coop.make_agent<client_t>(svc, 3);
							} );
					} );
		},
		5 );

	REQUIRE( result == 0 );
}

