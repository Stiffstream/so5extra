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
		reply_t() = default;

		reply_t( std::string value ) : m_value{ std::move(value) } {}

		reply_t( const reply_t & ) = delete;
		reply_t & operator=( const reply_t & ) = delete;

		reply_t( reply_t && ) = default;
		reply_t & operator=( reply_t && ) = default;

		const std::string &
		value() const noexcept { return m_value; }
	};

static_assert( std::is_default_constructible_v<reply_t> );
static_assert( std::is_move_constructible_v<reply_t> &&
		std::is_move_assignable_v<reply_t> );
static_assert( !std::is_copy_constructible_v<reply_t> &&
		!std::is_copy_assignable_v<reply_t> );

// reply2 is not default constructible.
class reply2_t final
	{
		std::string m_value;

	public :
		reply2_t( std::string value ) : m_value{ std::move(value) } {}

		reply2_t( const reply2_t & ) = delete;
		reply2_t & operator=( const reply2_t & ) = delete;

		reply2_t( reply2_t && ) = default;
		reply2_t & operator=( reply2_t && ) = default;

		const std::string &
		value() const noexcept { return m_value; }
	};

struct triple_t final
{
	int m_v;
};

class service_t final : public so_5::agent_t
	{
	public :
		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self()
					.event( &service_t::on_request )
					.event( &service_t::on_triple_request )
					;
			}

	private :
		void
		on_request( sync_ns::request_mhood_t<int, reply_t> cmd )
			{
				cmd->make_reply( std::to_string(cmd->request() * 2) );
			}

		void
		on_triple_request( sync_ns::request_mhood_t<triple_t, reply2_t> cmd )
			{
				cmd->make_reply( std::to_string(cmd->request().m_v * 3) );
			}
	};

class client_t final : public so_5::agent_t
	{
		const so_5::mbox_t m_service;

		std::string & m_result1;
		std::string & m_result2;
		std::string & m_result3;
		std::string & m_result4;

	public :
		client_t(
			context_t ctx,
			so_5::mbox_t service,
			std::string & result1,
			std::string & result2,
			std::string & result3,
			std::string & result4 )
			: so_5::agent_t{ ctx + so_5::enable_cpp_coro() }
			, m_service{ std::move(service) }
			, m_result1{ result1 }
			, m_result2{ result2 }
			, m_result3{ result3 }
			, m_result4{ result4 }
			{}

		so_5::cpp_coro::task_t< void >
		so_evt_async_start() override
			{
				auto r1 = co_await sync_ns::request_reply_async<int, reply_t>(
						m_service, 5s, 2 );
				m_result1 = r1.value();

				auto r2 = co_await sync_ns::request_opt_reply_async<int, reply_t>(
						m_service, 5s, 3 );
				m_result2 = r2->value();

				using my_triple_request =
						sync_ns::request_reply_t<triple_t, reply2_t>;
				auto r3 = co_await my_triple_request::ask_value_async( m_service, 5s, 4 );
				m_result3 = r3.value();

				auto r4 = co_await my_triple_request::ask_opt_value_async(
						m_service, 5s, 5 );
				m_result4 = r4->value();

				so_deregister_agent_coop_normally();
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "simple shutdown on empty environment" )
{
	std::string result1;
	std::string result2;
	std::string result3;
	std::string result4;

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [&](so_5::coop_t & coop) {
								auto * svc = coop.make_agent<service_t>();
								coop.make_agent<client_t>(
										svc->so_direct_mbox(),
										std::ref(result1),
										std::ref(result2),
										std::ref(result3),
										std::ref(result4) );
							} );
					} );
		},
		5 );

	REQUIRE( result1 == "4" );
	REQUIRE( result2 == "6" );
	REQUIRE( result3 == "12" );
	REQUIRE( result4 == "15" );
}

