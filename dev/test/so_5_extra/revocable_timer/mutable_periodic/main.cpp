#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/revocable_timer/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace timer_ns = so_5::extra::revocable_timer;

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

template< typename Message >
class test_case_t final : public so_5::agent_t
	{
		so_5::outliving_reference_t< int > m_exceptions_thrown;

	public :
		test_case_t(
			context_t ctx,
			so_5::outliving_reference_t< int > exceptions_thrown )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_exceptions_thrown{ std::move(exceptions_thrown) }
			{
			}

		void
		so_evt_start() override
			{
				try_env_mbox_form();
				try_agent_form();

				so_deregister_agent_coop_normally();
			}

	private :
		template< typename Lambda >
		void
		try_send_periodic( Lambda l )
			{
				try
					{
						l();
					}
				catch( const so_5::exception_t & x )
					{
						if( so_5::rc_mutable_msg_cannot_be_periodic == x.error_code() )
							m_exceptions_thrown.get() += 1;
					}
			}

		void
		try_env_mbox_form()
			{
				try_send_periodic( [this]() {
					timer_ns::send_periodic< Message >(
							so_direct_mbox(),
							std::chrono::seconds(1),
							std::chrono::seconds(2),
							0,
							"Hello!" );
				} );
			}

		void
		try_agent_form()
			{
				try_send_periodic( [this]() {
					timer_ns::send_periodic< Message >(
							*this,
							std::chrono::seconds(1),
							std::chrono::seconds(2),
							0,
							"Hello!" );
				} );
			}
	};

template< typename Message >
void
perform_test()
	{
		int exceptions_thrown = 0;

		run_with_time_limit( [&] {
				so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< test_case_t< Message > >(
										so_5::outliving_mutable(exceptions_thrown) ) );
				} );
			},
			5 );

		REQUIRE( 2 == exceptions_thrown );
	}


TEST_CASE( "send_periodic<mutable_msg<classical_message>>" )
{
	perform_test< so_5::mutable_msg<classical_message_t> >();
}

TEST_CASE( "send_periodic<mutable_msg<user_message>>" )
{
	perform_test< so_5::mutable_msg<user_message_t> >();
}

