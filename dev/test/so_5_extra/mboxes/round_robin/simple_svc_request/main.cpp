#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

struct hello final : public so_5::signal_t {};

class a_svc_provider_t final : public so_5::agent_t
{
public :
	a_svc_provider_t( context_t ctx, const so_5::mbox_t & rrmbox )
		:	so_5::agent_t( std::move(ctx) )
	{
		so_subscribe( rrmbox ).event( [](mhood_t<hello>) -> std::string {
			return "hello();";
		} );
	}
};

class a_test_case_t final : public so_5::agent_t
{
public :
	a_test_case_t(
		context_t ctx,
		so_5::mbox_t rrmbox,
		std::string & dest )
		:	so_5::agent_t( std::move(ctx) )
		,	m_rrmbox( std::move(rrmbox) )
		,	m_dest( dest )
	{}

	virtual void
	so_evt_start() override
	{
		m_dest += "start();";
		m_dest += so_5::request_value< std::string, hello >(
				m_rrmbox, so_5::infinite_wait );
		so_deregister_agent_coop_normally();
	}

	virtual void
	so_evt_finish() override
	{
		m_dest += "finish();";
	}

private :
	const so_5::mbox_t m_rrmbox;
	std::string & m_dest;
};

TEST_CASE( "simple service request on rrmbox" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						const auto rrmbox =
								so_5::extra::mboxes::round_robin::make_mbox<>( env );

						env.introduce_coop(
								so_5::disp::active_obj::create_private_disp( env )->binder(),
								[&]( so_5::coop_t & coop ) {
									coop.make_agent< a_svc_provider_t >( rrmbox );
									coop.make_agent< a_test_case_t >(
											std::cref(rrmbox), std::ref(scenario) );
								} );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
		},
		5 );
}

