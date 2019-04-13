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

struct simple_signal final : public so_5::signal_t {};

struct shutdown final : public so_5::signal_t {};

template< typename Message, typename Sender >
class test_case_t final : public so_5::agent_t
	{
		so_5::outliving_reference_t< int > m_instances_received;

	public :
		test_case_t(
			context_t ctx,
			so_5::outliving_reference_t< int > instances_received )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_instances_received{ std::move(instances_received) }
			{
				so_subscribe_self()
					.event( &test_case_t::on_receive )
					.event( &test_case_t::on_shutdown );
			}

		void
		so_evt_start() override
			{
				auto timer_id = Sender::send( *this );
				std::this_thread::sleep_for( std::chrono::milliseconds(100) );
				timer_id.release();
				so_5::send< shutdown >( *this );
			}

	private :
		void
		on_receive( mhood_t<Message> )
			{
				m_instances_received.get() += 1;
			}

		void
		on_shutdown( mhood_t<shutdown> )
			{
				so_deregister_agent_coop_normally();
			}
	};

static const std::chrono::milliseconds delay_time{ 25 };

template< typename Message >
struct send_periodic_env_mbox_t
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
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return timer_ns::send_delayed<Message>(
						to,
						delay_time );
			}
	};

template< typename Message, template<class T> class Sender >
void
perform_test()
	{
		int instances_received = 0;

		run_with_time_limit( [&] {
				so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								"test",
								env.make_agent< test_case_t< Message, Sender<Message> > >(
										so_5::outliving_mutable(instances_received) ) );
				} );
			},
			5 );

		REQUIRE( 0 == instances_received );
	}


TEST_CASE( "send_periodic<classical_message>(env, mbox)" )
{
	perform_test< classical_message_t, send_periodic_env_mbox_t >();
}

TEST_CASE( "send_delayed<classical_message>(env, mbox)" )
{
	perform_test< classical_message_t, send_delayed_env_mbox_t >();
}

TEST_CASE( "send_periodic<classical_message>(agent)" )
{
	perform_test< classical_message_t, send_periodic_agent_t >();
}

TEST_CASE( "send_delayed<classical_message>(agent)" )
{
	perform_test< classical_message_t, send_delayed_agent_t >();
}

TEST_CASE( "send_periodic<user_message>(env, mbox)" )
{
	perform_test< user_message_t, send_periodic_env_mbox_t >();
}

TEST_CASE( "send_delayed<user_message>(env, mbox)" )
{
	perform_test< user_message_t, send_delayed_env_mbox_t >();
}

TEST_CASE( "send_periodic<user_message>(agent)" )
{
	perform_test< user_message_t, send_periodic_agent_t >();
}

TEST_CASE( "send_delayed<user_message>(agent)" )
{
	perform_test< user_message_t, send_delayed_agent_t >();
}

TEST_CASE( "send_delayed<mutable_msg<classical_message>>(env, mbox)" )
{
	perform_test< so_5::mutable_msg<classical_message_t>, send_delayed_env_mbox_t >();
}

TEST_CASE( "send_delayed<mutable_msg<classical_message>>(agent)" )
{
	perform_test< so_5::mutable_msg<classical_message_t>, send_delayed_agent_t >();
}

TEST_CASE( "send_delayed<mutable_msg<user_message>>(env, mbox)" )
{
	perform_test< so_5::mutable_msg<user_message_t>, send_delayed_env_mbox_t >();
}

TEST_CASE( "send_delayed<mutable_msg<user_message>>(agent)" )
{
	perform_test< so_5::mutable_msg<user_message_t>, send_delayed_agent_t >();
}

TEST_CASE( "send_periodic<simple_signal>(env, mbox)" )
{
	perform_test< simple_signal, send_periodic_signal_env_mbox_t >();
}

TEST_CASE( "send_delayed<simple_signal>(env, mbox)" )
{
	perform_test< simple_signal, send_delayed_signal_env_mbox_t >();
}

TEST_CASE( "send_periodic<simple_signal>(agent)" )
{
	perform_test< simple_signal, send_periodic_signal_agent_t >();
}

TEST_CASE( "send_delayed<simple_signal>(agent)" )
{
	perform_test< simple_signal, send_delayed_signal_agent_t >();
}

