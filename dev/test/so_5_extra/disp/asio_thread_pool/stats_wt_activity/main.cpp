#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

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
						&a_test_t::evt_thread_activity );
			}

		virtual void
		so_evt_start() override
			{
				// This big value can lead to test failure if there is an
				// error in implementation of autoshutdown feature.
				so_environment().stats_controller().
					set_distribution_period(
							std::chrono::seconds(30) );

				so_environment().stats_controller().turn_on();
			}

	private :
		void
		evt_thread_activity(
			const so_5::stats::messages::work_thread_activity & evt )
			{
				namespace stats = so_5::stats;

				std::cout << evt.m_prefix.c_str()
						<< evt.m_suffix.c_str()
						<< ": [" << evt.m_thread_id << "] = ("
						<< evt.m_stats.m_working_stats << ", "
						<< evt.m_stats.m_waiting_stats << ")"
						<< std::endl;

				so_deregister_agent_coop_normally();
			}
	};

void
fill_test_coop(
	asio::io_context & io_svc,
	so_5::coop_t & coop,
	so_5::extra::disp::asio_thread_pool::dispatcher_handle_t disp )
{
	auto member_strand = coop.take_under_control(
			std::make_unique< asio::io_context::strand >( std::ref(io_svc) ) );

	coop.make_agent_with_binder< a_test_t >(
			disp.binder( *member_strand ) );
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

				so_5::launch( [&]( so_5::environment_t & env ) {
						namespace asio_tp = so_5::extra::disp::asio_thread_pool;

						asio_tp::disp_params_t params;
						params.use_external_io_context( io_svc );

						auto disp = asio_tp::make_dispatcher(
								env,
								"asio_tp",
								std::move(params) );

						env.introduce_coop( [&]( so_5::coop_t & coop ) {
								fill_test_coop( io_svc, coop, disp );
							} );
					},
					[&]( so_5::environment_params_t & params ) {
						params.turn_work_thread_activity_tracking_on();
					} );
			},
			20,
			"simple work thread activity monitoring test" );
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

