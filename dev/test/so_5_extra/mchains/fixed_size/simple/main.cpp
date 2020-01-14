#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mchains/fixed_size.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace fixed_mchain_ns = so_5::extra::mchains::fixed_size;

using namespace std::chrono_literals;

TEST_CASE( "no waiting case (with msg_tracing)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					auto ch = fixed_mchain_ns::create_mchain<2>(
							env,
							so_5::mchain_props::overflow_reaction_t::remove_oldest );

					REQUIRE( 0u == ch->size() );

					so_5::send< int >( ch, 0 );
					REQUIRE( 1u == ch->size() );

					so_5::send< int >( ch, 1 );
					REQUIRE( 2u == ch->size() );

					so_5::send< int >( ch, 2 );
					REQUIRE( 2u == ch->size() );

					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 1 == v ); } );
					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 2 == v ); } );
				},
				[](so_5::environment_params_t & params) {
					params.message_delivery_tracer(
							so_5::msg_tracing::std_cout_tracer() );
				} );
		},
		5 );
}

TEST_CASE( "no waiting case (without msg_tracing)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					auto ch = fixed_mchain_ns::create_mchain<2>(
							env,
							so_5::mchain_props::overflow_reaction_t::remove_oldest );

					REQUIRE( 0u == ch->size() );

					so_5::send< int >( ch, 0 );
					REQUIRE( 1u == ch->size() );

					so_5::send< int >( ch, 1 );
					REQUIRE( 2u == ch->size() );

					so_5::send< int >( ch, 2 );
					REQUIRE( 2u == ch->size() );

					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 1 == v ); } );
					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 2 == v ); } );
				} );
		},
		5 );
}

TEST_CASE( "waiting case (with msg_tracing)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					const auto wait_time = 100ms;

					auto ch = fixed_mchain_ns::create_mchain<2>(
							env,
							wait_time,
							so_5::mchain_props::overflow_reaction_t::remove_oldest );

					REQUIRE( 0u == ch->size() );

					so_5::send< int >( ch, 0 );
					REQUIRE( 1u == ch->size() );

					so_5::send< int >( ch, 1 );
					REQUIRE( 2u == ch->size() );

					const auto send_started_at = std::chrono::steady_clock::now();
					so_5::send< int >( ch, 2 );
					const auto send_finished_at = std::chrono::steady_clock::now();

					REQUIRE( 2u == ch->size() );
					REQUIRE( wait_time < (send_finished_at - send_started_at + 10ms) );

					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 1 == v ); } );
					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 2 == v ); } );
				},
				[](so_5::environment_params_t & params) {
					params.message_delivery_tracer(
							so_5::msg_tracing::std_cout_tracer() );
				} );
		},
		5 );
}

TEST_CASE( "waiting case (without msg_tracing)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					const auto wait_time = 100ms;

					auto ch = fixed_mchain_ns::create_mchain<2>(
							env,
							wait_time,
							so_5::mchain_props::overflow_reaction_t::remove_oldest );

					REQUIRE( 0u == ch->size() );

					so_5::send< int >( ch, 0 );
					REQUIRE( 1u == ch->size() );

					so_5::send< int >( ch, 1 );
					REQUIRE( 2u == ch->size() );

					const auto send_started_at = std::chrono::steady_clock::now();
					so_5::send< int >( ch, 2 );
					const auto send_finished_at = std::chrono::steady_clock::now();

					REQUIRE( 2u == ch->size() );
					REQUIRE( wait_time < (send_finished_at - send_started_at + 10ms) );

					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 1 == v ); } );
					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 2 == v ); } );
				} );
		},
		5 );
}

TEST_CASE( "waiting case with mchain_params_t" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					const auto wait_time = 100ms;

					auto params = so_5::make_limited_with_waiting_mchain_params(
							100, // Should be ignored.
							so_5::mchain_props::memory_usage_t::dynamic, // Should be ignored.
							so_5::mchain_props::overflow_reaction_t::drop_newest,
							wait_time );
					params.disable_msg_tracing();

					auto ch = fixed_mchain_ns::create_mchain<2>(
							env,
							params );

					REQUIRE( 0u == ch->size() );

					so_5::send< int >( ch, 0 );
					REQUIRE( 1u == ch->size() );

					so_5::send< int >( ch, 1 );
					REQUIRE( 2u == ch->size() );

					const auto send_started_at = std::chrono::steady_clock::now();
					so_5::send< int >( ch, 2 );
					const auto send_finished_at = std::chrono::steady_clock::now();

					REQUIRE( 2u == ch->size() );
					REQUIRE( wait_time < (send_finished_at - send_started_at + 10ms) );

					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 0 == v ); } );
					so_5::receive( so_5::from(ch).handle_n(1),
						[](int v) { REQUIRE( 1 == v ); } );
				},
				[](so_5::environment_params_t & params) {
					params.message_delivery_tracer(
							so_5::msg_tracing::std_cout_tracer() );
				} );
		},
		5 );
}

