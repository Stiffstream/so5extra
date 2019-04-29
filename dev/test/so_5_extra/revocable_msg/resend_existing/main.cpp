#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/revocable_msg/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace delivery_ns = so_5::extra::revocable_msg;

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

template< typename Message, typename Sender >
class test_case_t final : public so_5::agent_t
	{
		state_t st_first{ this };
		state_t st_second{ this };

		so_5::outliving_reference_t< int > m_instances_received;

		delivery_ns::delivery_id_t m_id;

	public :
		test_case_t(
			context_t ctx,
			so_5::outliving_reference_t< int > instances_received )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_instances_received{ std::move(instances_received) }
			{
				st_first
					.event( &test_case_t::on_receive1 );

				st_second
					.event( &test_case_t::on_receive2 );
			}

		void
		so_evt_start() override
			{
				this >>= st_first;

				m_id = Sender::send( *this );
			}

	private :
		void
		on_receive1( mhood_t<Message> cmd )
			{
				m_instances_received.get() += 1;

				this >>= st_second;

				m_id = delivery_ns::send( *this, std::move(cmd) );
			}

		void
		on_receive2( mhood_t<Message> )
			{
				m_instances_received.get() += 1;
				so_deregister_agent_coop_normally();
			}
	};

template< typename Message >
struct send_msg_to_agent_t
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return delivery_ns::send<Message>( to, 0, "Hello!" );
			}
	};

template< typename Message >
struct send_signal_to_agent_t
	{
		static auto
		send( const so_5::agent_t & to )
			{
				return delivery_ns::send<Message>( to );
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
								env.make_agent< test_case_t< Message, Sender<Message> > >(
										so_5::outliving_mutable(instances_received) ) );
				} );
			},
			5 );

		REQUIRE( 2 == instances_received );
	}


TEST_CASE( "send<classical_message>" )
{
	perform_test< classical_message_t, send_msg_to_agent_t >();
}

TEST_CASE( "send<mutable<classical_message>>" )
{
	perform_test< so_5::mutable_msg<classical_message_t>, send_msg_to_agent_t >();
}

TEST_CASE( "send<user_message>" )
{
	perform_test< user_message_t, send_msg_to_agent_t >();
}

TEST_CASE( "send<mutable<user_message>>" )
{
	perform_test< so_5::mutable_msg<user_message_t>, send_msg_to_agent_t >();
}

TEST_CASE( "send<simple_signal>" )
{
	perform_test< simple_signal, send_signal_to_agent_t >();
}

