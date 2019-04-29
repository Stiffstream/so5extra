#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class a_test_case_t final : public so_5::agent_t
{
	std::string & m_dest;

	struct hello final : public so_5::signal_t {};

public :
	a_test_case_t(
		context_t ctx,
		std::string & dest )
		:	so_5::agent_t( std::move(ctx) )
		,	m_dest( dest )
		,	m_mbox( so_5::extra::mboxes::round_robin::make_mbox<>(
					so_environment() ) )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe( m_mbox ).event( &a_test_case_t::on_hello );
	}

	virtual void
	so_evt_start() override
	{
		m_dest += "start();";
		so_5::send< hello >( m_mbox );
	}

	virtual void
	so_evt_finish() override
	{
		m_dest += "finish();";
	}

private :
	const so_5::mbox_t m_mbox;

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
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t >(
										std::ref(scenario) ) );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
		},
		5 );
}

