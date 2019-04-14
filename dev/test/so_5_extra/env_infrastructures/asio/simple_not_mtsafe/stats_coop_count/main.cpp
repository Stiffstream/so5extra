#include <so_5_extra/env_infrastructures/asio/simple_not_mtsafe.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class a_test_t : public so_5::agent_t
	{
	public :
		a_test_t( context_t ctx )
			:	so_5::agent_t( ctx )
			{}

		virtual void
		so_define_agent() override
			{
				so_default_state().event(
						so_environment().stats_controller().mbox(),
						&a_test_t::evt_monitor_quantity );
			}

		virtual void
		so_evt_start() override
			{
				create_child_coops();

				// This big value can lead to test failure if there is an
				// error in implementation of autoshutdown feature.
				so_environment().stats_controller().
					set_distribution_period(
							std::chrono::seconds(30) );

				so_environment().stats_controller().turn_on();
			}

	private :
		unsigned int m_actual_values = { 0 };

		void
		evt_monitor_quantity(
			const so_5::stats::messages::quantity< std::size_t > & evt )
			{
				namespace stats = so_5::stats;

				std::cout << evt.m_prefix.c_str()
						<< evt.m_suffix.c_str()
						<< ": " << evt.m_value << std::endl;

				if( stats::prefixes::coop_repository() == evt.m_prefix )
					{
						if( stats::suffixes::coop_reg_count() == evt.m_suffix )
							{
								// Count of registered cooperations could be
								// 12 or 11 (it depends of deregistration of
								// the special autoshutdown-guard cooperation).
								if( 12 != evt.m_value && 11 != evt.m_value )
									throw std::runtime_error( "unexpected count of "
											"registered cooperations: " +
											std::to_string( evt.m_value ) );
								else
									++m_actual_values;
							}
						else if( stats::suffixes::coop_dereg_count() == evt.m_suffix )
							{
								// Count of registered cooperations could be
								// 0 or 1 (it depends of deregistration of
								// the special autoshutdown-guard cooperation).
								if( 0 != evt.m_value && 1 != evt.m_value )
									throw std::runtime_error( "unexpected count of "
											"deregistered cooperations: " +
											std::to_string( evt.m_value ) );
								else
									++m_actual_values;
							}
						else if( stats::suffixes::agent_count() == evt.m_suffix )
							{
								// Count of registered agents could be
								// 11 or 12 (it depends of deregistration of
								// the special autoshutdown-guard cooperation).
								if( 11 != evt.m_value && 12 != evt.m_value )
									throw std::runtime_error( "unexpected count of "
											"registered agents: " +
											std::to_string( evt.m_value ) );
								else
									++m_actual_values;
							}
						else if( stats::suffixes::coop_final_dereg_count() == evt.m_suffix )
							{
								// Count of coops waiting for final dereg could be
								// 0 or 1 (it depends of deregistration of
								// the special autoshutdown-guard cooperation).
								if( 0 != evt.m_value && 1 != evt.m_value )
									throw std::runtime_error( "unexpected count of "
											"coops in final dereg state: " +
											std::to_string( evt.m_value ) );
								else
									++m_actual_values;
							}
					}

				if( 4 == m_actual_values )
					so_deregister_agent_coop_normally();
			}

		void
		create_child_coops()
			{
				for( int i = 0; i != 10; ++i )
					{
						auto coop = so_5::create_child_coop(
								*this, so_5::autoname );
						coop->define_agent();

						so_environment().register_coop( std::move( coop ) );
					}
			}
	};

void
init( so_5::environment_t & env )
	{
		env.register_agent_as_coop( env.make_agent< a_test_t >() );
	}

int
main()
{
	try
	{
		run_with_time_limit(
			[]()
			{
				asio::io_context io_svc;

				so_5::launch( &init,
					[&]( so_5::environment_params_t & params ) {
						using namespace so_5::extra::env_infrastructures::asio::simple_not_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
					} );
			},
			20 );
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

