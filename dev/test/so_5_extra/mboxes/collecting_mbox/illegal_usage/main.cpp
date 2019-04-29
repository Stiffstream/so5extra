#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/collecting_mbox.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace collecting_mbox_ns = so_5::extra::mboxes::collecting_mbox;

struct hello final : public so_5::message_t
{
	const std::string m_data;

	hello( std::string data ) : m_data( std::move(data) ) {}
};

template< typename M >
struct constexpr_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			M,
			collecting_mbox_ns::constexpr_size_traits_t<3> >;

	static auto
	make( const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( target );
	}
};

template< typename M >
struct runtime_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			M,
			collecting_mbox_ns::runtime_size_traits_t >;

	static auto
	make( const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( target, 3u );
	}
};

class dummy_actor final : public so_5::agent_t
{
public :
	using so_5::agent_t::agent_t;

	void so_evt_start() override
	{
		so_environment().stop();
	}
};

TEST_CASE( "mutable mboxes (constexpr case)" )
{
	run_with_time_limit( [] {
			int error_caught = 0;

			so_5::launch( [&](so_5::environment_t & env) {
				env.introduce_coop( [&](so_5::coop_t & coop) {
					auto a = coop.make_agent< dummy_actor >();

					REQUIRE_NOTHROW(
						constexpr_case< so_5::mutable_msg< hello > >::make(
								a->so_direct_mbox() ) );

					REQUIRE_THROWS_AS( [&]() {
						auto mpmc_mbox = env.create_mbox();
						try {
							constexpr_case< so_5::mutable_msg< hello > >::make(
									mpmc_mbox );
						}
						catch( const so_5::exception_t & x ) {
							error_caught = x.error_code();
							throw;
						}
					}(),
					so_5::exception_t );

				} );
			} );

			REQUIRE( so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox == error_caught );
		},
		5 );
}

TEST_CASE( "immutable mboxes (constexpr case)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				env.introduce_coop( [&](so_5::coop_t & coop) {
					auto a = coop.make_agent< dummy_actor >();

					REQUIRE_NOTHROW(
						constexpr_case< so_5::immutable_msg< hello > >::make(
								a->so_direct_mbox() ) );

					REQUIRE_NOTHROW( [&]() {
						auto mpmc_mbox = env.create_mbox();
						constexpr_case< so_5::immutable_msg< hello > >::make(
								mpmc_mbox );
					}() );

				} );
			} );
		},
		5 );
}

TEST_CASE( "mutable mboxes (runtime case)" )
{
	run_with_time_limit( [] {
			int error_caught = 0;

			so_5::launch( [&](so_5::environment_t & env) {
				env.introduce_coop( [&](so_5::coop_t & coop) {
					auto a = coop.make_agent< dummy_actor >();

					REQUIRE_NOTHROW(
						runtime_case< so_5::mutable_msg< hello > >::make(
								a->so_direct_mbox() ) );

					REQUIRE_THROWS_AS( [&]() {
						auto mpmc_mbox = env.create_mbox();
						try {
							runtime_case< so_5::mutable_msg< hello > >::make(
									mpmc_mbox );
						}
						catch( const so_5::exception_t & x ) {
							error_caught = x.error_code();
							throw;
						}
					}(),
					so_5::exception_t );

				} );
			} );

			REQUIRE( so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox == error_caught );
		},
		5 );
}

TEST_CASE( "immutable mboxes (runtime case)" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
				env.introduce_coop( [&](so_5::coop_t & coop) {
					auto a = coop.make_agent< dummy_actor >();

					REQUIRE_NOTHROW(
						runtime_case< so_5::immutable_msg< hello > >::make(
								a->so_direct_mbox() ) );

					REQUIRE_NOTHROW( [&]() {
						auto mpmc_mbox = env.create_mbox();
						runtime_case< so_5::immutable_msg< hello > >::make(
								mpmc_mbox );
					}() );

				} );
			} );
		},
		5 );
}

