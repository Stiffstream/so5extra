#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

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

TEST_CASE( "simple shutdown on empty environment" )
{
	std::string result;

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						auto svc = env.introduce_coop( [](so_5::coop_t & coop) {
								return coop.make_agent<service_t>()->so_direct_mbox();
							} );

						auto r = sync_ns::request_reply_t<int, reply_t>::
								request_value( svc, 5s, 2 );
						result = r.value();

						env.stop();
					} );
		},
		5 );

	REQUIRE( result == "4" );
}

