#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/disp/asio_one_thread/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

struct result_collector_t
{
	bool m_agent_count_received{ false };
	bool m_demands_count_received{ false };
};

class a_test_case_t final : public so_5::agent_t
{
	result_collector_t & m_dest;

public :
	a_test_case_t(
		context_t ctx,
		result_collector_t & dest )
		:	so_5::agent_t( std::move(ctx) )
		,	m_dest( dest )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe( so_environment().stats_controller().mbox() )
			.event( &a_test_case_t::on_distribution_finished )
			.event( &a_test_case_t::on_stats );
	}

	virtual void
	so_evt_start() override
	{
		so_environment().stats_controller().turn_on();
	}

private :
	void
	on_distribution_finished(
		mhood_t<so_5::stats::messages::distribution_finished> )
	{
		so_deregister_agent_coop_normally();
	}

	void
	on_stats(
		mhood_t<so_5::stats::messages::quantity<std::size_t>> cmd )
	{
		std::cout << cmd->m_prefix << cmd->m_suffix
			<< " -> " << cmd->m_value << std::endl;

		if( std::strstr( cmd->m_prefix.c_str(), "ext-asio-ot" ) )
		{
			if( so_5::stats::suffixes::agent_count() == cmd->m_suffix )
				m_dest.m_agent_count_received = true;
			else if( so_5::stats::suffixes::work_thread_queue_size() == cmd->m_suffix )
				m_dest.m_demands_count_received = true;
		}
	}
};

TEST_CASE( "retrive necessary data from run-time stats" )
{
	run_with_time_limit( [] {
			result_collector_t result;

			asio::io_context io_svc;
			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_ot = so_5::extra::disp::asio_one_thread;

						asio_ot::disp_params_t params;
						params.use_external_io_context( io_svc );

						auto disp = asio_ot::make_dispatcher(
								env,
								"asio_ot",
								std::move(params) );

						env.introduce_coop(
								disp.binder(),
								[&]( so_5::coop_t & coop )
								{
									coop.make_agent< a_test_case_t >(
											std::ref(result) );
								} );
					} );

			REQUIRE( result.m_agent_count_received );
			REQUIRE( result.m_demands_count_received );
		},
		5 );
}

