#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

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
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_tp = so_5::extra::disp::asio_thread_pool;

						asio_tp::disp_params_t params;
						params.use_own_io_context();

						auto disp = asio_tp::create_private_disp(
								env,
								"asio_tp",
								std::move(params) );

						env.introduce_coop(
								[&]( so_5::coop_t & coop )
								{
									auto member_strand = coop.take_under_control(
											std::make_unique< asio::io_context::strand >(
													disp->io_context() ) );

									coop.make_agent_with_binder< a_test_case_t >(
											disp->binder( *member_strand ),
											std::ref(scenario) );
								} );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
		},
		5 );
}

