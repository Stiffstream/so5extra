#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/first_last_subscriber_notification.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

#include <tuple>

namespace mbox_ns = so_5::extra::mboxes::first_last_subscriber_notification;

struct msg_dummy final : public so_5::message_t {};

TEST_CASE( "creation: MPMC mbox, immutable message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto dest = env.create_mbox();
						std::ignore = mbox_ns::make_multi_consumer_mbox< msg_dummy >(
								env, dest );
					} );
		},
		5 );
}

TEST_CASE( "creation: MPMC mbox, mutable message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto dest = env.create_mbox();

						REQUIRE_THROWS_AS(
							std::ignore = mbox_ns::make_multi_consumer_mbox< so_5::mutable_msg<msg_dummy> >(
									env, dest ),
							const so_5::exception_t & );
					} );
		},
		5 );
}

TEST_CASE( "creation: MPSC mbox, immutable message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto dest = env.create_mbox();
						std::ignore = mbox_ns::make_single_consumer_mbox< msg_dummy >(
								env, dest );
					} );
		},
		5 );
}

TEST_CASE( "creation: MPSC mbox, mutable message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto dest = env.create_mbox();
						std::ignore = mbox_ns::make_single_consumer_mbox< so_5::mutable_msg<msg_dummy> >(
								env, dest );
					} );
		},
		5 );
}

TEST_CASE( "sending: MPMC mbox, mutable message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto dest = env.create_mbox();
						const auto proxy = mbox_ns::make_multi_consumer_mbox< msg_dummy >(
									env, dest );

						REQUIRE_THROWS_AS(
							so_5::send< so_5::mutable_msg<msg_dummy> >( proxy ),
							const so_5::exception_t & );
					} );
		},
		5 );
}

TEST_CASE( "sending: MPSC mbox, mutable message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto dest = env.create_mbox();
						const auto proxy =
								mbox_ns::make_single_consumer_mbox< so_5::mutable_msg<msg_dummy> >(
											env, dest );

						so_5::send< so_5::mutable_msg<msg_dummy> >( proxy );
					} );
		},
		5 );
}

