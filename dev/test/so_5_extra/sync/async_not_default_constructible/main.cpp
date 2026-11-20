#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

namespace test
{

class reply_t final
	{
		std::string m_value;

	public :
		reply_t() = delete;

		reply_t( std::string value ) : m_value{ std::move(value) } {}

		const std::string &
		value() const noexcept { return m_value; }
	};

static_assert( !std::is_default_constructible_v<reply_t> );
static_assert( std::is_move_constructible_v<reply_t> &&
		std::is_move_assignable_v<reply_t> );
static_assert( std::is_copy_constructible_v<reply_t> &&
		std::is_copy_assignable_v<reply_t> );

class service_t final : public so_5::agent_t
	{
	public :
		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self().event( &service_t::on_request );
			}

	private :
		void
		on_request( sync_ns::request_mhood_t<int, reply_t> cmd )
			{
				cmd->make_reply( std::to_string(cmd->request() * 2) );
			}
	};

class client_t final : public so_5::agent_t
	{
		std::string & m_result;

		const so_5::mbox_t m_service;

	public:
		client_t( context_t ctx, std::string & result, so_5::mbox_t service )
			: so_5::agent_t{ ctx + so_5::enable_cpp_coro() }
			, m_result{ result }
			, m_service{ std::move(service) }
			{}

		so_5::cpp_coro::task_t< void >
		so_evt_async_start() override
			{
				auto r = co_await sync_ns::request_reply_async<int, reply_t>(
						m_service, 5s, 2 );
				m_result = r.value();

				so_deregister_agent_coop_normally();
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "simple shutdown on empty environment" )
{
	std::string result;

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [&result](so_5::coop_t & coop) {
								auto * svc = coop.make_agent<service_t>();
								coop.make_agent<client_t>(
										std::ref(result),
										svc->so_direct_mbox() );
							} );
					} );
		},
		5 );

	REQUIRE( result == "4" );
}

