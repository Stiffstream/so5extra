#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/revocable_timer/pub.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

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

struct shutdown final : public so_5::signal_t {};

template< typename Message >
class test_case_basic_t : public so_5::agent_t
	{
		so_5::outliving_reference_t< int > m_exceptions_thrown;

	public :
		test_case_basic_t(
			context_t ctx,
			so_5::outliving_reference_t< int > exceptions_thrown )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_exceptions_thrown{ std::move(exceptions_thrown) }
			{
				so_subscribe_self()
					.event( &test_case_basic_t::on_message )
					.event( &test_case_basic_t::on_shutdown );
			}

		void
		so_evt_start() override
			{
				so_5::send< Message >( *this, 0, "Hello!" );
				so_5::send< shutdown >( *this );
			}

	protected :
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

		virtual void
		on_message( mhood_t< Message > cmd ) = 0;

		void
		on_shutdown( mhood_t< shutdown > )
			{
				so_deregister_agent_coop_normally();
			}
	};

template< typename Message >
class test_case_env_mbox_t final : public test_case_basic_t< Message >
	{
	public :
		using base_type_t = test_case_basic_t< Message >;

		using base_type_t::base_type_t;

	protected :
		void
		on_message( ::so_5::mhood_t< Message > cmd ) override
			{
				this->try_send_periodic( [&]() {
					timer_ns::send_periodic(
							this->so_environment(),
							this->so_direct_mbox(),
							std::chrono::seconds(1),
							std::chrono::seconds(2),
							std::move(cmd) );
				} );
			}
	};

template< typename Message >
class test_case_agent_t final : public test_case_basic_t< Message >
	{
	public :
		using base_type_t = test_case_basic_t< Message >;

		using base_type_t::base_type_t;

	protected :
		void
		on_message( ::so_5::mhood_t< Message > cmd ) override
			{
				this->try_send_periodic( [&]() {
					timer_ns::send_periodic(
							*this,
							std::chrono::seconds(1),
							std::chrono::seconds(2),
							std::move(cmd) );
				} );
			}
	};

template< typename Message, template<class M> class Test_Case >
void
perform_test()
	{
		int exceptions_thrown = 0;

		run_with_time_limit( [&] {
				so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								"test",
								env.make_agent< Test_Case< Message > >(
										so_5::outliving_mutable(exceptions_thrown) ) );
				} );
			},
			5 );

		REQUIRE( 1 == exceptions_thrown );
	}


TEST_CASE( "send_periodic<mutable_msg<classical_message>>(env, mbox)" )
{
	perform_test<
			so_5::mutable_msg<classical_message_t>,
			test_case_env_mbox_t >();
}

TEST_CASE( "send_periodic<mutable_msg<classical_message>>(agent)" )
{
	perform_test<
			so_5::mutable_msg<classical_message_t>,
			test_case_agent_t >();
}

TEST_CASE( "send_periodic<mutable_msg<user_message>>(env, mbox)" )
{
	perform_test<
			so_5::mutable_msg<user_message_t>,
			test_case_env_mbox_t >();
}

TEST_CASE( "send_periodic<mutable_msg<user_message>>(agent)" )
{
	perform_test<
			so_5::mutable_msg<user_message_t>,
			test_case_agent_t >();
}

