#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/collecting_mbox.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

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
	make( so_5::environment_t & env, const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( env, target );
	}
};

template< typename M >
struct runtime_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			M,
			collecting_mbox_ns::runtime_size_traits_t >;

	static auto
	make( so_5::environment_t & env, const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( env, target, 3u );
	}
};

TEST_CASE( "mutable mboxes (constexpr case)" )
{
	run_with_time_limit( [] {
			int error_caught = 0;

			so_5::launch( [&](so_5::environment_t & env) {
				env.introduce_coop( [&](so_5::coop_t & coop) {
					auto a = coop.define_agent();
					a.on_start( [&]{ env.stop(); } );

					REQUIRE_NOTHROW(
						constexpr_case< so_5::mutable_msg< hello > >::make(
								env, a.direct_mbox() ) );

					REQUIRE_THROWS_AS( [&]() {
						auto mpmc_mbox = env.create_mbox();
						try {
							constexpr_case< so_5::mutable_msg< hello > >::make(
									env, mpmc_mbox );
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
					auto a = coop.define_agent();
					a.on_start( [&]{ env.stop(); } );

					REQUIRE_NOTHROW(
						constexpr_case< so_5::immutable_msg< hello > >::make(
								env, a.direct_mbox() ) );

					REQUIRE_NOTHROW( [&]() {
						auto mpmc_mbox = env.create_mbox();
						constexpr_case< so_5::immutable_msg< hello > >::make(
								env, mpmc_mbox );
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
					auto a = coop.define_agent();
					a.on_start( [&]{ env.stop(); } );

					REQUIRE_NOTHROW(
						runtime_case< so_5::mutable_msg< hello > >::make(
								env, a.direct_mbox() ) );

					REQUIRE_THROWS_AS( [&]() {
						auto mpmc_mbox = env.create_mbox();
						try {
							runtime_case< so_5::mutable_msg< hello > >::make(
									env, mpmc_mbox );
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
					auto a = coop.define_agent();
					a.on_start( [&]{ env.stop(); } );

					REQUIRE_NOTHROW(
						runtime_case< so_5::immutable_msg< hello > >::make(
								env, a.direct_mbox() ) );

					REQUIRE_NOTHROW( [&]() {
						auto mpmc_mbox = env.create_mbox();
						runtime_case< so_5::immutable_msg< hello > >::make(
								env, mpmc_mbox );
					}() );

				} );
			} );
		},
		5 );
}

TEST_CASE( "service request (constexpr case)" )
{
	run_with_time_limit( [] {
			int error_caught = 0;

			so_5::launch( [&](so_5::environment_t & env) {
				auto target = env.create_mbox();

				auto mbox = constexpr_case< hello >::make( env, target );
				auto svc_req = [&] {
					return so_5::request_value< std::string, hello >(
							mbox,
							std::chrono::milliseconds(150),
							"hello" );
				};

				REQUIRE_THROWS_AS( [&]() {
					try {
						svc_req();
					}
					catch( const so_5::exception_t & x ) {
						error_caught = x.error_code();
						throw;
					}
				}(),
				so_5::exception_t );
			} );

			REQUIRE( so_5::extra::mboxes::collecting_mbox::errors
					::rc_service_request_on_collecting_mbox == error_caught );
		},
		5 );
}

TEST_CASE( "service request (runtime case)" )
{
	run_with_time_limit( [] {
			int error_caught = 0;

			so_5::launch( [&](so_5::environment_t & env) {
				auto target = env.create_mbox();

				auto mbox = runtime_case< hello >::make( env, target );
				auto svc_req = [&] {
					return so_5::request_value< std::string, hello >(
							mbox,
							std::chrono::milliseconds(150),
							"hello" );
				};

				REQUIRE_THROWS_AS( [&]() {
					try {
						svc_req();
					}
					catch( const so_5::exception_t & x ) {
						error_caught = x.error_code();
						throw;
					}
				}(),
				so_5::exception_t );
			} );

			REQUIRE( so_5::extra::mboxes::collecting_mbox::errors
					::rc_service_request_on_collecting_mbox == error_caught );
		},
		5 );
}

