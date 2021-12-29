#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

class custom_thread_type final
	:	public so_5::disp::abstract_work_thread_t
{
	std::thread m_thread;
public :
	custom_thread_type() = default;

	void start( body_func_t thread_body ) override
	{
		m_thread = std::thread{ [tb = std::move(thread_body)]() {
				tb();
			}
		};
	}

	void join() override { m_thread.join(); }
};

class custom_thread_factory final
	:	public so_5::disp::abstract_work_thread_factory_t
{
	std::atomic<unsigned int> m_acquired{};
	std::atomic<unsigned int> m_released{};

public:
	custom_thread_factory() = default;

	so_5::disp::abstract_work_thread_t &
	acquire( so_5::environment_t & /*env*/ ) override
	{
		++m_acquired;
		return *(new custom_thread_type{});
	}

	void
	release( so_5::disp::abstract_work_thread_t & thread ) noexcept override
	{
		++m_released;
		delete &thread;
	}

	[[nodiscard]]
	unsigned int
	acquired()
	{
		return m_acquired.load( std::memory_order_acquire );
	}

	[[nodiscard]]
	unsigned int
	released()
	{
		return m_released.load( std::memory_order_acquire );
	}
};

struct custom_traits
{
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
			asio::io_context::strand actor_strand{ io_svc };
			auto factory = std::make_shared< custom_thread_factory >();
			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_tp = so_5::extra::disp::asio_thread_pool;

						asio_tp::disp_params_t params;
						params.use_external_io_context( io_svc );
						params.thread_count( 3 );
						params.work_thread_factory( factory );

						auto disp = asio_tp::make_dispatcher<custom_traits>(
								env,
								"asio_tp",
								std::move(params) );

						env.introduce_coop(
								disp.binder( actor_strand ),
								[&]( so_5::coop_t & coop )
								{
									coop.make_agent< a_test_case_t >(
											std::ref(scenario) );
								} );
					} );

			REQUIRE( scenario == "start();hello();finish();" );
			REQUIRE( 3u == factory->acquired() );
			REQUIRE( 3u == factory->released() );
		},
		5 );
}

