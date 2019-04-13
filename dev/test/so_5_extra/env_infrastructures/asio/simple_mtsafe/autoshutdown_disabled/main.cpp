#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

TEST_CASE("Do not shutdown if there is no more work")
{
	run_with_time_limit( [] {
		so_5::environment_t * penv{};
		std::chrono::steady_clock::time_point finished_at;

		std::thread sobj_thread( [&] {
			asio::io_context io_svc;

			so_5::launch(
					[&](so_5::environment_t & env) {
						penv = &env;
					},
					[&io_svc](so_5::environment_params_t & params) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
						params.disable_autoshutdown();
					} );

			finished_at = std::chrono::steady_clock::now();
		} );

		std::cout << "sleeping for some time..." << std::endl;
		std::this_thread::sleep_for( std::chrono::milliseconds(350) );

		std::cout << "stopping the SObjectizer..." << std::endl;
		const auto stop_at = std::chrono::steady_clock::now();
		penv->stop();

		std::cout << "waiting the SObjectizer's thread..." << std::endl;
		sobj_thread.join();

		REQUIRE( stop_at <= finished_at );
	},
	5 );
}

