#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

#include <random>

constexpr std::size_t N = 200;

class a_shutdown_initiator_t final : public so_5::agent_t
{
public :
	struct i_am_ready final : public so_5::signal_t {};

	a_shutdown_initiator_t( context_t ctx )
		:	so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self().event( [this](mhood_t<i_am_ready>) {
				++m_subscribed_agents;
				if( m_subscribed_agents >= N )
				{
					so_environment().stop();
				}
			} );
	}

private :
	std::size_t m_subscribed_agents = 0;
};

class a_test_t final : public so_5::agent_t
{
	struct do_subscription final : public so_5::signal_t {};
	struct do_dereg final : public so_5::signal_t {};

public :
	a_test_t( context_t ctx, so_5::mbox_t ready_mbox )
		:	so_5::agent_t( std::move(ctx) )
		,	m_ready_mbox( std::move(ready_mbox) )
	{
		so_subscribe_self()
			.event( [this](mhood_t<do_subscription>) {
					auto & s = so_5::extra::shutdowner::layer( so_environment() );
					so_subscribe( s.notify_mbox() ).event( &a_test_t::on_shutdown );

					so_5::send< a_shutdown_initiator_t::i_am_ready >( m_ready_mbox );
				} )
			.event( [this](mhood_t<do_dereg>) {
					so_deregister_agent_coop_normally();
				} );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send_delayed< do_subscription >( *this, random_delay() );
	}

private :
	const so_5::mbox_t m_ready_mbox;

	void
	on_shutdown( mhood_t< so_5::extra::shutdowner::shutdown_initiated_t > )
	{
		so_5::send_delayed< do_dereg >( *this, random_delay() );
	}

	static std::chrono::steady_clock::duration
	random_delay()
	{
		std::default_random_engine random_dev;
		const auto v = std::uniform_int_distribution<int>{ 50, 100 }( random_dev );
		return std::chrono::milliseconds(v);
	}
};

TEST_CASE( "shutdown with a single subscriber" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						so_5::mbox_t ready_mbox;
						env.introduce_coop( [&ready_mbox](so_5::coop_t & coop) {
								auto a = coop.make_agent< a_shutdown_initiator_t >();
								ready_mbox = a->so_direct_mbox();
							} );

						for( std::size_t i = 0; i != N; ++i )
							env.introduce_coop( [ready_mbox](so_5::coop_t & coop) {
									coop.make_agent< a_test_t >( ready_mbox );
								} );
					},
					[](so_5::environment_params_t & params) {
						params.add_layer(
								so_5::extra::shutdowner::make_layer< so_5::null_mutex_t >(
										std::chrono::milliseconds(2000) ) );
						params.infrastructure_factory(
								so_5::env_infrastructures::simple_not_mtsafe::factory() );
					} );
		},
		5 );
}

