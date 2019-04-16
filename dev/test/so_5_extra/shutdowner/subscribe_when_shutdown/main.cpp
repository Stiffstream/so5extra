#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/shutdowner/shutdowner.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class a_master_t final : public so_5::agent_t
{
	struct complete_shutdown final : public so_5::signal_t {};

public :
	struct try_subscribe final : public so_5::signal_t {};

	a_master_t( context_t ctx, so_5::mbox_t slave_mbox )
		:	so_5::agent_t( std::move(ctx) )
		,	m_slave_mbox( std::move(slave_mbox) )
	{
		auto & s = so_5::extra::shutdowner::layer( so_environment() );
		so_subscribe( s.notify_mbox() ).event( &a_master_t::on_shutdown );

		so_subscribe_self().event( &a_master_t::on_complete_shutdown );
	}

private :
	const so_5::mbox_t m_slave_mbox;

	void
	on_shutdown( mhood_t< so_5::extra::shutdowner::shutdown_initiated_t > )
	{
		so_5::send< try_subscribe >( m_slave_mbox );
		so_5::send_delayed< complete_shutdown >( *this,
				std::chrono::milliseconds(200) );
	}

	void
	on_complete_shutdown( mhood_t< complete_shutdown > )
	{
		so_deregister_agent_coop_normally();
	}
};

class a_slave_t final : public so_5::agent_t
{
public :
	a_slave_t( context_t ctx, so_5::outliving_reference_t<bool> result )
		:	so_5::agent_t( std::move(ctx) )
		,	m_result( result )
	{
		so_subscribe_self().event( &a_slave_t::on_try_subscribe );
	}

private :
	so_5::outliving_reference_t<bool> m_result;

	void
	on_try_subscribe( mhood_t<a_master_t::try_subscribe> )
	{
		try
		{
			auto & s = so_5::extra::shutdowner::layer( so_environment() );
			so_subscribe( s.notify_mbox() ).event( &a_slave_t::on_shutdown );
		}
		catch( const so_5::exception_t & ex )
		{
			if( so_5::extra::shutdowner::errors::rc_subscription_disabled_during_shutdown
					== ex.error_code() )
				m_result.get() = true;
			else
				throw;
		}
	}

	void
	on_shutdown( mhood_t< so_5::extra::shutdowner::shutdown_initiated_t > )
	{
		throw std::runtime_error( "This should't happen!" );
	}
};

TEST_CASE( "shutdown with a single subscriber" )
{
	run_with_time_limit( [] {
			bool exception_caught = false;

			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop(
							[&exception_caught](so_5::coop_t & coop) {
								auto slave = coop.make_agent< a_slave_t >(
										so_5::outliving_mutable(exception_caught) );

								coop.make_agent< a_master_t >(
										slave->so_direct_mbox() );
							} );

						env.stop();
					},
					[](so_5::environment_params_t & params) {
						params.add_layer(
								so_5::extra::shutdowner::make_layer<>(
										std::chrono::milliseconds(2000) ) );
					} );

			REQUIRE( exception_caught );
		},
		5 );
}

