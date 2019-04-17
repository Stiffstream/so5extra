#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/enveloped_msg/send_functions.hpp>
#include <so_5_extra/enveloped_msg/time_limited_delivery.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

#include <sstream>

namespace msg_ns = so_5::extra::enveloped_msg;

struct classical_message_t final : public so_5::message_t
	{
		int m_a;
		const char * m_b;

		classical_message_t( int a, const char * b )
			:	m_a{ a }, m_b{ b }
		{}
	};

struct user_message_t final
	{
		int m_a;
		const char * m_b;
	};

struct simple_signal final : public so_5::signal_t {};

template< typename Timepoint >
void
sleep_until( Timepoint deadline )
{
	while( std::chrono::steady_clock::now() <= deadline )
		std::this_thread::sleep_until( deadline );
}

TEST_CASE( "send_to(mchain)" )
{
	std::string trace;
	run_with_time_limit( [&] {
			so_5::wrapped_env_t sobj;
			auto mchain = create_mchain( sobj );

			auto deadline = std::chrono::steady_clock::now() +
					std::chrono::seconds{1};

			auto send_messages = [&] {
				msg_ns::make< classical_message_t >( 1, "Hello!" )
						.envelope< msg_ns::time_limited_delivery_t >( deadline )
						.send_to( mchain );

				msg_ns::make< user_message_t >( 2, "Bye!" )
						.envelope< msg_ns::time_limited_delivery_t >( deadline )
						.send_to( mchain );

				msg_ns::make< simple_signal >()
						.envelope< msg_ns::time_limited_delivery_t >( deadline )
						.send_to( mchain );
			};

			auto prepared_receive = prepare_receive(
					from(mchain).no_wait_on_empty().handle_all(),
					[&trace]( so_5::mhood_t<classical_message_t> cmd ) {
						std::ostringstream s;
						s << "classical{" << cmd->m_a << ", " << cmd->m_b << "};";
						trace += s.str();
					},
					[&trace]( so_5::mhood_t<user_message_t> cmd ) {
						std::ostringstream s;
						s << "user{" << cmd->m_a << ", " << cmd->m_b << "};";
						trace += s.str();
					},
					[&trace]( so_5::mhood_t<simple_signal> ) {
						trace += "simple_signal;";
					} );

			send_messages();

			receive( prepared_receive );

			send_messages();

			sleep_until( deadline );

			receive( prepared_receive );
		},
		5 );

	const std::string expected{
			"classical{1, Hello!};"
			"user{2, Bye!};"
			"simple_signal;"
	};

	REQUIRE( trace == expected );
}

TEST_CASE( "send_delayed_to(mchain)" )
{
	std::string trace;
	run_with_time_limit( [&] {
			using namespace std::chrono_literals;

			so_5::wrapped_env_t sobj;
			auto mchain = create_mchain( sobj );

			auto deadline = std::chrono::steady_clock::now() +
					std::chrono::seconds{1};

			msg_ns::make< classical_message_t >( 1, "Hello!" )
					.envelope< msg_ns::time_limited_delivery_t >( deadline )
					.send_delayed_to( mchain, 25ms );

			msg_ns::make< user_message_t >( 2, "Bye!" )
					.envelope< msg_ns::time_limited_delivery_t >( deadline )
					.send_delayed_to( mchain, 25ms );

			msg_ns::make< simple_signal >()
					.envelope< msg_ns::time_limited_delivery_t >( deadline )
					.send_delayed_to( mchain, 25ms );

			sleep_until( deadline );

			receive( from(mchain).no_wait_on_empty().handle_all(),
					[&trace]( so_5::mhood_t<classical_message_t> cmd ) {
						std::ostringstream s;
						s << "classical{" << cmd->m_a << ", " << cmd->m_b << "};";
						trace += s.str();
					},
					[&trace]( so_5::mhood_t<user_message_t> cmd ) {
						std::ostringstream s;
						s << "user{" << cmd->m_a << ", " << cmd->m_b << "};";
						trace += s.str();
					},
					[&trace]( so_5::mhood_t<simple_signal> ) {
						trace += "simple_signal;";
					} );
		},
		5 );

	const std::string expected{ "" };

	REQUIRE( trace == expected );
}

TEST_CASE( "send_periodic_to(mchain)" )
{
	std::string trace;
	run_with_time_limit( [&] {
			using namespace std::chrono_literals;

			so_5::wrapped_env_t sobj;
			auto mchain = create_mchain( sobj );

			auto deadline = std::chrono::steady_clock::now() +
					std::chrono::seconds{1};

			auto t1 = msg_ns::make< classical_message_t >( 1, "Hello!" )
					.envelope< msg_ns::time_limited_delivery_t >( deadline )
					.send_periodic_to( mchain, 25ms, 50ms );

			auto t2 = msg_ns::make< user_message_t >( 2, "Bye!" )
					.envelope< msg_ns::time_limited_delivery_t >( deadline )
					.send_periodic_to( mchain, 25ms, 50ms );

			auto t3 = msg_ns::make< simple_signal >()
					.envelope< msg_ns::time_limited_delivery_t >( deadline )
					.send_periodic_to( mchain, 25ms, 50ms );

			sleep_until( deadline );

			receive( from(mchain).no_wait_on_empty().handle_all(),
					[&trace]( so_5::mhood_t<classical_message_t> cmd ) {
						std::ostringstream s;
						s << "classical{" << cmd->m_a << ", " << cmd->m_b << "};";
						trace += s.str();
					},
					[&trace]( so_5::mhood_t<user_message_t> cmd ) {
						std::ostringstream s;
						s << "user{" << cmd->m_a << ", " << cmd->m_b << "};";
						trace += s.str();
					},
					[&trace]( so_5::mhood_t<simple_signal> ) {
						trace += "simple_signal;";
					} );
		},
		5 );

	const std::string expected{ "" };

	REQUIRE( trace == expected );
}

