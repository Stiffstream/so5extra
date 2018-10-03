#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

struct run_results
{
	bool evt_start_invoked_{ false };
	bool evt_finish_invoked_{ false };
	std::chrono::steady_clock::time_point finished_at_;
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
		results_.finished_at_ = std::chrono::steady_clock::now();
	}
};

TEST_CASE("Do not shutdown if there is no more work")
{
	run_with_time_limit( [] {
		so_5::environment_t * penv{};
		run_results results;

		std::thread sobj_thread( [&] {
			asio::io_context io_svc;

			so_5::launch(
					[&](so_5::environment_t & env) {
						penv = &env;

						env.introduce_coop([&](so_5::coop_t & coop) {
							coop.make_agent<dummy_agent>(std::ref(results));
						} );
					},
					[&io_svc](so_5::environment_params_t & params) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
					} );
			} );

		std::cout << "sleeping for some time..." << std::endl;
		std::this_thread::sleep_for( std::chrono::milliseconds(350) );

		std::cout << "stopping the SObjectizer..." << std::endl;
		const auto stop_at = std::chrono::steady_clock::now();
		penv->stop();

		std::cout << "waiting the SObjectizer's thread..." << std::endl;
		sobj_thread.join();

		REQUIRE( results.evt_start_invoked_ );
		REQUIRE( results.evt_finish_invoked_ );
		REQUIRE( stop_at <= results.finished_at_ );
	},
	5 );
}

