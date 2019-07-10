#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/broadcast.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

#include <list>
#include <deque>

namespace broadcast_mbox = so_5::extra::mboxes::broadcast;

struct shutdown final : public so_5::signal_t {};

class a_test_case_t final : public so_5::agent_t
{
public :
	using so_5::agent_t::agent_t;

	void
	so_define_agent() override
	{
		so_subscribe_self().event( [this](mhood_t<shutdown>) {
				so_deregister_agent_coop_normally();
			} );
	}
};

TEST_CASE( "simplest case with std::vector (const reference)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				std::vector< so_5::mbox_t > destinations;
				for( int i = 0; i != 10; ++i ) {
					auto actor = env.make_agent< a_test_case_t >();
					destinations.push_back( actor->so_direct_mbox() );
					env.register_agent_as_coop( std::move(actor) );
				}

				auto mbox = broadcast_mbox::fixed_mbox_template_t<>::make(
						env, destinations );

				so_5::send< shutdown >( mbox );
			});

			REQUIRE( true );
		},
		5 );
}

TEST_CASE( "simplest case with std::vector (rvalue reference)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				auto maker = [&env] {
					std::vector< so_5::mbox_t > destinations;
					for( int i = 0; i != 10; ++i ) {
						auto actor = env.make_agent< a_test_case_t >();
						destinations.push_back( actor->so_direct_mbox() );
						env.register_agent_as_coop( std::move(actor) );
					}
					return destinations;
				};

				auto mbox = broadcast_mbox::fixed_mbox_template_t<>::make(
						env, maker() );

				so_5::send< shutdown >( mbox );
			});

			REQUIRE( true );
		},
		5 );
}

TEST_CASE( "simplest case with std::vector (two iterators)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				std::list< so_5::mbox_t > destinations;
				for( int i = 0; i != 10; ++i ) {
					auto actor = env.make_agent< a_test_case_t >();
					destinations.push_back( actor->so_direct_mbox() );
					env.register_agent_as_coop( std::move(actor) );
				}

				auto mbox = broadcast_mbox::fixed_mbox_template_t<>::make(
						env, begin(destinations), end(destinations) );

				so_5::send< shutdown >( mbox );
			});

			REQUIRE( true );
		},
		5 );
}

TEST_CASE( "simplest case with std::array (const reference)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				std::array< so_5::mbox_t, 10 > destinations;
				for( int i = 0; i != 10; ++i ) {
					auto actor = env.make_agent< a_test_case_t >();
					destinations[ i ] = actor->so_direct_mbox();
					env.register_agent_as_coop( std::move(actor) );
				}

				auto mbox = broadcast_mbox::fixed_mbox_template_t<
							std::array< so_5::mbox_t, 10 >
						>::make( env, destinations );

				so_5::send< shutdown >( mbox );
			});

			REQUIRE( true );
		},
		5 );
}

TEST_CASE( "simplest case with std::deque (another container)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				std::list< so_5::mbox_t > destinations;
				for( int i = 0; i != 10; ++i ) {
					auto actor = env.make_agent< a_test_case_t >();
					destinations.push_back( actor->so_direct_mbox() );
					env.register_agent_as_coop( std::move(actor) );
				}

				auto mbox = broadcast_mbox::fixed_mbox_template_t<
							std::deque< so_5::mbox_t >
						>::make( env, destinations );

				so_5::send< shutdown >( mbox );
			});

			REQUIRE( true );
		},
		5 );
}

TEST_CASE( "simplest case with plain C array -> std::deque" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				auto actor_maker = [&env]() {
					auto actor = env.make_agent< a_test_case_t >();
					auto mbox = actor->so_direct_mbox();
					env.register_agent_as_coop( std::move(actor) );
					return mbox;
				};

				so_5::mbox_t destinations[] = {
					actor_maker(),
					actor_maker(),
					actor_maker(),
					actor_maker()
				};

				auto mbox = broadcast_mbox::fixed_mbox_template_t<
							std::deque< so_5::mbox_t >
						>::make( env, destinations );

				so_5::send< shutdown >( mbox );
			});

			REQUIRE( true );
		},
		5 );
}
