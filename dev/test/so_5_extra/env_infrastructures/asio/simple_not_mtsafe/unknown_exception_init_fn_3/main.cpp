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

class dummy_actor_t final : public so_5::agent_t
{
	struct next_turn final : public so_5::signal_t {};

	so_5::atomic_counter_t & m_finish_counter;

public :
	dummy_actor_t( context_t ctx, so_5::atomic_counter_t & finish_counter )
		:	so_5::agent_t::agent_t{ std::move(ctx) }
		,	m_finish_counter{ finish_counter }
	{}

	void
	so_evt_start() override
	{
		so_subscribe_self().event( &dummy_actor_t::on_next_turn );

		so_5::send< next_turn >( *this );
	}

	void
	so_evt_finish() override
	{
		++m_finish_counter;
	}

private :
	void
	on_next_turn( mhood_t<next_turn> )
	{
		so_5::send_delayed< next_turn >( *this, std::chrono::milliseconds(10) );
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
	so_5::atomic_counter_t finish_counter{ 0 };

	run_with_time_limit(
		[&]() {
			try
			{
				asio::io_context io_ctx;

				so_5::launch(
					[&]( so_5::environment_t & env ) {
						// Create a bunch of coops with dummy_actors inside.
						for( int i = 0; i != 10000; ++i )
						{
							auto coop = env.make_coop();
							coop->take_under_control(
									std::make_unique< coop_resource_t >(
											std::ref(counter) ) );
							coop->make_agent< dummy_actor_t >(
									std::ref(finish_counter) );

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
		300 );

	REQUIRE( raw_exception_is_caught == result );

	const auto actual_counter = counter.load();
	REQUIRE( 0u == actual_counter );

	const auto actual_finish_counter = finish_counter.load();
	REQUIRE( 10000u == actual_finish_counter );
}

