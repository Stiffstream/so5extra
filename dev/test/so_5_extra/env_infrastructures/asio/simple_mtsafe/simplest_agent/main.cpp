#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/env_infrastructures/asio/simple_mtsafe.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

class a_test_case_t final : public so_5::agent_t
{
	std::string & m_dest;
	so_5::current_thread_id_t & m_thread_id;

	struct hello final : public so_5::signal_t {};

public :
	a_test_case_t(
		context_t ctx,
		std::string & dest,
		so_5::current_thread_id_t & thread_id )
		:	so_5::agent_t( std::move(ctx) )
		,	m_dest( dest )
		,	m_thread_id( thread_id )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe_self().event( &a_test_case_t::on_hello );
	}

	virtual void
	so_evt_start() override
	{
		m_dest += "start();";
		m_thread_id = so_5::query_current_thread_id();

		so_5::send< hello >( *this );
	}

	virtual void
	so_evt_finish() override
	{
		m_dest += "finish();";
	}

private :
	void
	on_hello( mhood_t<hello> )
	{
		m_dest += "hello();";
		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "simplest agent must handle so_evt_start and so_evt_finish" )
{
	run_with_time_limit( [] {
			asio::io_context io_svc;

			std::string scenario;
			so_5::current_thread_id_t actual_thread_id;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent< a_test_case_t >(
										std::ref(scenario),
										std::ref(actual_thread_id) ) );
					},
					[&io_svc](so_5::environment_params_t & params) {
						using namespace so_5::extra::env_infrastructures::asio::simple_mtsafe;

						params.infrastructure_factory( factory(io_svc) );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
			REQUIRE( actual_thread_id == so_5::query_current_thread_id() );
		},
		5 );
}

