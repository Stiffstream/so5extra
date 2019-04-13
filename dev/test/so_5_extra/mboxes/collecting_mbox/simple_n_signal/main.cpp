#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/collecting_mbox.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

namespace collecting_mbox_ns = so_5::extra::mboxes::collecting_mbox;

struct hello final : public so_5::signal_t {};

struct constexpr_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			hello,
			collecting_mbox_ns::constexpr_size_traits_t<3> >;

	static auto
	make( so_5::environment_t & env, const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( env, target );
	}
};

struct runtime_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			hello,
			collecting_mbox_ns::runtime_size_traits_t >;

	static auto
	make( so_5::environment_t & env, const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( env, target, 3u );
	}
};

template< typename CASE >
class a_test_case_t final : public so_5::agent_t
{
	struct stop final : public so_5::signal_t {};

	using collecting_mbox_t = typename CASE::collecting_mbox_t;

public :
	a_test_case_t(
		context_t ctx,
		unsigned int & collected )
		:	so_5::agent_t( std::move(ctx) )
		,	m_collected( collected )
		,	m_mbox( CASE::make( so_environment(), so_direct_mbox() ) )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe_self().event( &a_test_case_t::on_messages_collected );
		so_subscribe_self().event( &a_test_case_t::on_stop );
	}

	virtual void
	so_evt_start() override
	{
		for( int i = 0; i < 10; ++i )
		{
			so_5::send< hello >( m_mbox );
			so_5::send< hello >( m_mbox );
			so_5::send< hello >( m_mbox );
		}
		so_5::send< hello >( m_mbox );
		so_5::send< hello >( m_mbox );

		so_5::send< stop >( *this );
	}

private :
	unsigned int & m_collected;

	const so_5::mbox_t m_mbox;

	void
	on_messages_collected( mhood_t<typename collecting_mbox_t::messages_collected_t>)
	{
		m_collected += 1;
	}

	void
	on_stop( mhood_t<stop> )
	{
		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "constexpr case" )
{
	run_with_time_limit( [] {
			unsigned int collected = 0;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent< a_test_case_t< constexpr_case > >(
										std::ref(collected) ) );
					}/*,
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					}*/ );

			REQUIRE( collected == 10u );
		},
		5 );
}

TEST_CASE( "runtime case" )
{
	run_with_time_limit( [] {
			unsigned int collected = 0;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent< a_test_case_t< runtime_case > >(
										std::ref(collected) ) );
					}/*,
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					}*/ );

			REQUIRE( collected == 10u );
		},
		5 );
}

