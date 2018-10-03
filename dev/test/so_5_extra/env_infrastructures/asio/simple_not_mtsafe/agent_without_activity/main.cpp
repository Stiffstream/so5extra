#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_not_mtsafe.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

struct run_results
{
	bool evt_start_invoked_{ false };
	bool evt_finish_invoked_{ false };
};

class dummy_agent : public so_5::agent_t
{
	run_results & results_;

public :
	dummy_agent(context_t ctx, run_results & results)
		: so_5::agent_t(std::move(ctx))
		, results_(results)
	{}

	virtual void so_evt_start() override
	{
		results_.evt_start_invoked_ = true;
	}

	virtual void so_evt_finish() override
	{
		results_.evt_finish_invoked_ = true;
	}
};

TEST_CASE("Autoshutdown when there is no more work")
{
	run_with_time_limit( [] {
		asio::io_context io_svc;

		run_results results;

		so_5::launch( [&](so_5::environment_t & env) {
					env.introduce_coop([&](so_5::coop_t & coop) {
						coop.make_agent<dummy_agent>(std::ref(results));
					} );
				},
				[&io_svc](so_5::environment_params_t & params) {
					using namespace so_5::extra::env_infrastructures::asio::simple_not_mtsafe;

					params.infrastructure_factory( factory(io_svc) );
				} );

		REQUIRE( results.evt_start_invoked_ );
		REQUIRE( results.evt_finish_invoked_ );
	},
	5 );
}

