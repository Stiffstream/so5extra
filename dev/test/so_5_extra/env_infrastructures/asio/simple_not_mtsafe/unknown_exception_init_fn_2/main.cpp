#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_not_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using namespace std;

class coop_resource_t
{
	so_5::atomic_counter_t & m_counter;

public :
	coop_resource_t( so_5::atomic_counter_t & counter )
		:	m_counter{ counter }
	{
		++m_counter;
	}

	~coop_resource_t() noexcept
	{
		--m_counter;
	}
};

TEST_CASE( "unknown exception from init_fn" )
{
	enum result_t {
		exception_not_caught,
		so_exception_is_caught,
		raw_exception_is_caught
	};

	result_t result{ exception_not_caught };
	so_5::atomic_counter_t counter{ 0 };

	run_with_time_limit(
		[&]() {
			try
			{
				asio::io_context io_ctx;

				so_5::launch(
					[&]( so_5::environment_t & env ) {
						// Create a bunch of empty coops.
						for( int i = 0; i != 10000; ++i )
						{
							auto coop = env.make_coop();
							coop->take_under_control(
									std::make_unique< coop_resource_t >(
											std::ref(counter) ) );

							auto id = env.register_coop( std::move(coop) );

							// Some of them will be deregistered.
							if( 0 == (i % 3) )
								env.deregister_coop( id,
										so_5::dereg_reason::normal );
						}

						throw "boom!";
					},
					[&]( so_5::environment_params_t & params ) {
						using namespace so_5::extra::env_infrastructures::asio::simple_not_mtsafe;

						params.infrastructure_factory( factory(io_ctx) );
					} );
			}
			catch( const so_5::exception_t & )
			{
				result = so_exception_is_caught;
			}
			catch( const char * )
			{
				result = raw_exception_is_caught;
			}
		},
		5 );

	REQUIRE( raw_exception_is_caught == result );

	const auto actual_counter = counter.load();
	REQUIRE( 0u == actual_counter );
}

