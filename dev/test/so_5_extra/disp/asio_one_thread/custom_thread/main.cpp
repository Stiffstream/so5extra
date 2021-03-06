#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/disp/asio_one_thread/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

struct custom_thread_init_params
{
	int m_priority;
	std::size_t m_stack_size;
};

class custom_thread_type 
{
	std::thread m_thread;
public :
	custom_thread_type() = delete;
	custom_thread_type( const custom_thread_type & ) = delete;
	custom_thread_type( custom_thread_type && ) = delete;

	template< typename F >
	custom_thread_type( F && f ) : m_thread( std::forward<F>(f) ) {}

	template< typename F >
	custom_thread_type( F && f, custom_thread_init_params params )
	{
		std::cout << "*** priority: " << params.m_priority
				<< ", stack_size: " << params.m_stack_size
				<< std::endl;

		m_thread = std::thread{ std::forward<F>(f) };
	}

	custom_thread_type & operator=( const custom_thread_type & ) = delete;
	custom_thread_type & operator=( custom_thread_type && ) = delete;

	void join() { m_thread.join(); }
};

struct custom_traits
{
	using thread_type = custom_thread_type;
};

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

			asio::io_context io_svc;
			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_ot = so_5::extra::disp::asio_one_thread;

						asio_ot::disp_params_t params;
						params.use_external_io_context( io_svc );

						auto disp = asio_ot::make_dispatcher<custom_traits>(
								env,
								"asio_ot",
								std::move(params) );

						env.introduce_coop(
								disp.binder(),
								[&]( so_5::coop_t & coop )
								{
									coop.make_agent< a_test_case_t >(
											std::ref(scenario) );
								} );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
		},
		5 );
}

TEST_CASE( "simplest agent must handle so_evt_start and so_evt_finish "
		"+ custom thread init params" )
{
	run_with_time_limit( [] {
			std::string scenario;

			asio::io_context io_svc;
			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_ot = so_5::extra::disp::asio_one_thread;

						asio_ot::disp_params_t params;
						params.use_external_io_context( io_svc );

						auto disp = asio_ot::make_dispatcher<custom_traits>(
								env,
								"asio_ot",
								std::move(params),
								custom_thread_init_params{ -2, 10240u } );

						env.introduce_coop(
								disp.binder(),
								[&]( so_5::coop_t & coop )
								{
									coop.make_agent< a_test_case_t >(
											std::ref(scenario) );
								} );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
		},
		5 );
}

