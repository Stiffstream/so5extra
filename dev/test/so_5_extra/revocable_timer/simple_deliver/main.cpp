#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/revocable_timer/pub.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

namespace timer_ns = so_5::extra::revocable_timer;

struct classical_message1_t final : public so_5::message_t
	{
		int m_a;
		const char * m_b;

		classical_message1_t( int a, const char * b )
			:	m_a{ a }, m_b{ b }
		{}
	};

struct classical_message2_t final : public so_5::message_t
	{
		int m_a;
		const char * m_b;

		classical_message2_t( int a, const char * b )
			:	m_a{ a }, m_b{ b }
		{}
	};

struct user_message1_t final
	{
		int m_a;
		const char * m_b;
	};

struct user_message2_t final
	{
		int m_a;
		const char * m_b;
	};

struct simple_signal1 final : public so_5::signal_t {};
struct simple_signal2 final : public so_5::signal_t {};

struct shutdown final : public so_5::signal_t {};

static const std::chrono::milliseconds delay_time{ 50 };

template<
	typename Periodic_Sender,
	typename Delayed_Sender >
class test_case_t final : public so_5::agent_t
	{
		so_5::outliving_reference_t< int > m_instances_received;

		timer_ns::timer_id_t m_periodic_id;
		timer_ns::timer_id_t m_delayed_id;

	public :
		test_case_t(
			context_t ctx,
			so_5::outliving_reference_t< int > instances_received )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_instances_received{ std::move(instances_received) }
			{
				so_subscribe_self()
					.event( &test_case_t::on_receive1 )
					.event( &test_case_t::on_receive2 )
					.event( &test_case_t::on_shutdown );
			}

		void
		so_evt_start() override
			{
				m_periodic_id = Periodic_Sender::send( *this );
				m_delayed_id = Delayed_Sender::send( *this );

				so_5::send_delayed< shutdown >(
						*this,
						std::chrono::milliseconds{ 175 } );
			}

	private :
		void
		on_receive1( mhood_t< typename Periodic_Sender::msg_type > )
			{
				m_instances_received.get() += 1;
			}

		void
		on_receive2( mhood_t< typename Delayed_Sender::msg_type > )
			{
				m_instances_received.get() += 1;
			}

		void
		on_shutdown( mhood_t<shutdown> )
			{
				so_deregister_agent_coop_normally();
			}
	};

template< typename Message >
struct sender_basics_t
	{
		using msg_type = Message;
	};

template< typename Message >
struct send_periodic_env_mbox_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_periodic<Message>(
						to.so_environment(),
						to.so_direct_mbox(),
						delay_time,
						delay_time,
						0,
						"Hello!" );
			}
	};

template< typename Message >
struct send_delayed_env_mbox_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_delayed<Message>(
						to.so_environment(),
						to.so_direct_mbox(),
						delay_time,
						0,
						"Hello!" );
			}
	};

template< typename Message >
struct send_periodic_agent_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_periodic<Message>(
						to,
						delay_time,
						delay_time,
						0,
						"Hello!" );
			}
	};

template< typename Message >
struct send_delayed_agent_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_delayed<Message>(
						to,
						delay_time,
						0,
						"Hello!" );
			}
	};

template< typename Message >
struct send_periodic_signal_env_mbox_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_periodic<Message>(
						to.so_environment(),
						to.so_direct_mbox(),
						delay_time,
						delay_time );
			}
	};

template< typename Message >
struct send_delayed_signal_env_mbox_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_delayed<Message>(
						to.so_environment(),
						to.so_direct_mbox(),
						delay_time );
			}
	};

template< typename Message >
struct send_periodic_signal_agent_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_periodic<Message>(
						to,
						delay_time,
						delay_time );
			}
	};

template< typename Message >
struct send_delayed_signal_agent_t
	:	public sender_basics_t< Message >
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_delayed<Message>(
						to,
						delay_time );
			}
	};

template<
		typename Periodic_Message,
		template<class T> class Periodic_Sender,
		typename Delayed_Message,
		template<class T> class Delayed_Sender >
void
perform_test()
	{
		int instances_received = 0;

		run_with_time_limit( [&] {
				so_5::launch( [&](so_5::environment_t & env) {
						using agent_type = test_case_t<
								Periodic_Sender<Periodic_Message>,
								Delayed_Sender<Delayed_Message> >;

						env.register_agent_as_coop(
								"test",
								env.make_agent< agent_type >(
										so_5::outliving_mutable(instances_received) ) );
				} /*,
				[](so_5::environment_params_t & params) {
					params.message_delivery_tracer(
							so_5::msg_tracing::std_cout_tracer() );
				}*/ );

			},
			5 );

		REQUIRE( 4 <= instances_received );
	}

TEST_CASE( "send<classical_message, classical_message>(env, mbox)" )
{
	perform_test< classical_message1_t,
			send_periodic_env_mbox_t,
			classical_message2_t,
			send_delayed_env_mbox_t >();
}

TEST_CASE( "send<classical_message, classical_message>(agent)" )
{
	perform_test< classical_message1_t,
			send_periodic_agent_t,
			classical_message2_t,
			send_delayed_agent_t >();
}

TEST_CASE( "send<classical_message, mutable<classical_message>>(env, mbox)" )
{
	perform_test< classical_message1_t,
			send_periodic_env_mbox_t,
			so_5::mutable_msg<classical_message2_t>,
			send_delayed_env_mbox_t >();
}

TEST_CASE( "send<classical_message, mutable<classical_message>>(agent)" )
{
	perform_test< classical_message1_t,
			send_periodic_agent_t,
			so_5::mutable_msg<classical_message2_t>,
			send_delayed_agent_t >();
}

TEST_CASE( "send<user_message, user_message>(env, mbox)" )
{
	perform_test< user_message1_t,
			send_periodic_env_mbox_t,
			user_message2_t,
			send_delayed_env_mbox_t >();
}

TEST_CASE( "send<user_message, user_message>(agent)" )
{
	perform_test< user_message1_t,
			send_periodic_agent_t,
			user_message2_t,
			send_delayed_agent_t >();
}

TEST_CASE( "send<user_message, mutable<user_message>>(env, mbox)" )
{
	perform_test< user_message1_t,
			send_periodic_env_mbox_t,
			so_5::mutable_msg<user_message2_t>,
			send_delayed_env_mbox_t >();
}

TEST_CASE( "send<user_message, mutable<user_message>>(agent)" )
{
	perform_test< user_message1_t,
			send_periodic_agent_t,
			so_5::mutable_msg<user_message2_t>,
			send_delayed_agent_t >();
}

TEST_CASE( "send<simple_signal>(env, mbox)" )
{
	perform_test<
		simple_signal1,
		send_periodic_signal_env_mbox_t,
		simple_signal2,
		send_delayed_signal_env_mbox_t >();
}

TEST_CASE( "send<simple_signal>(agent)" )
{
	perform_test<
		simple_signal1,
		send_periodic_signal_agent_t,
		simple_signal2,
		send_delayed_signal_agent_t >();
}

