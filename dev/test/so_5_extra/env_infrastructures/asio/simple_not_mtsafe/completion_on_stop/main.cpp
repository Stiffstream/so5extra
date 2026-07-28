#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_not_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace test
{

using sequence_t = std::vector<int>;

class a_test_case_t final : public so_5::agent_t
{
	sequence_t & m_dest;

	struct start final : public so_5::signal_t {};

public :
	a_test_case_t( context_t ctx, sequence_t & dest )
		:	so_5::agent_t( std::move(ctx) )
		,	m_dest( dest )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe_self()
			.event( &a_test_case_t::on_start )
			.event( &a_test_case_t::on_number )
			;
	}

	virtual void
	so_evt_start() override
	{
		so_5::send< start >( *this );
	}

private :
	void
	on_number( mhood_t<int> cmd )
	{
		m_dest.push_back( *cmd );
	}

	void
	on_start( mhood_t<start> )
	{
		for( int i = 0; i < 10; ++i )
			so_5::send< int >( *this, i );

		so_environment().stop();
	}
};

} /* namespace test */

using namespace test;

TEST_CASE( "completion of in-flight messages on stop" )
{
	run_with_time_limit( [] {
			asio::io_context io_svc;

			sequence_t actual_seq;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t >( std::ref(actual_seq) ) );
					},
					[&io_svc](so_5::environment_params_t & params) {
						using namespace so_5::extra::env_infrastructures::asio
								::simple_not_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
					} );

			const sequence_t expected_seq{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

			REQUIRE( expected_seq == actual_seq );
		},
		5 );
}

