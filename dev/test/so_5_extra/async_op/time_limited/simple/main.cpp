#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace method_as_completion_handler
{

using namespace ::so_5::extra::async_op::time_limited;

class a_test_t final : public so_5::agent_t
	{
	private :
		so_5::outliving_reference_t< std::string > m_trace;

		struct demo_signal final : public so_5::signal_t {};

		struct finish_signal final : public so_5::signal_t {};

	public :
		a_test_t(
			context_t ctx,
			so_5::outliving_reference_t< std::string > trace )
			:	so_5::agent_t( std::move(ctx) )
			,	m_trace( trace )
			{}

		virtual void
		so_define_agent() override
			{
				so_subscribe_self().event( &a_test_t::on_finish );
			}

		virtual void
		so_evt_start() override
			{
				make< demo_signal >( *this )
					.reserve_timeout_handlers_capacity(2u)
					.reserve_completion_handlers_capacity(2u)
					.completed_on(
						so_direct_mbox(),
						so_default_state(),
						&a_test_t::on_demo_signal )
					.activate( std::chrono::seconds(1) );

				so_5::send<demo_signal>( *this );
				so_5::send<demo_signal>( *this );

				so_5::send<finish_signal>( *this );
			}

	private :
		void
		on_demo_signal(mhood_t<demo_signal>)
			{
				m_trace.get() += "demo;";
			}

		void
		on_finish(mhood_t<finish_signal>)
			{
				so_deregister_agent_coop_normally();
			}
	};

} /* namespace method_as_completion_handler */

namespace lambda_as_completion_handler
{

using namespace ::so_5::extra::async_op::time_limited;

class a_test_t final : public so_5::agent_t
	{
	private :
		so_5::outliving_reference_t< std::string > m_trace;

		struct demo_signal final : public so_5::signal_t {};

		struct finish_signal final : public so_5::signal_t {};

	public :
		a_test_t(
			context_t ctx,
			so_5::outliving_reference_t< std::string > trace )
			:	so_5::agent_t( std::move(ctx) )
			,	m_trace( trace )
			{}

		virtual void
		so_define_agent() override
			{
				so_subscribe_self().event( &a_test_t::on_finish );
			}

		virtual void
		so_evt_start() override
			{
				make< demo_signal >( *this )
					.completed_on(
						so_direct_mbox(),
						so_default_state(),
						[&](mhood_t<demo_signal>) {
							m_trace.get() += "demo;";
						} )
					.activate( std::chrono::seconds(1) );

				so_5::send<demo_signal>( *this );
				so_5::send<demo_signal>( *this );

				so_5::send<finish_signal>( *this );
			}

	private :
		void
		on_finish(mhood_t<finish_signal>)
			{
				so_deregister_agent_coop_normally();
			}
	};

} /* namespace lambda_as_completion_handler */

TEST_CASE( "agent method as event_handler" )
{
	using method_as_completion_handler::a_test_t;

	std::string trace;
	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_t >(
										::so_5::outliving_mutable(trace) ) );
					});
		},
		5 );

	REQUIRE( trace == "demo;" );
}

TEST_CASE( "lambda as event_handler" )
{
	using lambda_as_completion_handler::a_test_t;

	std::string trace;
	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_t >(
										::so_5::outliving_mutable(trace) ) );
					});
		},
		5 );

	REQUIRE( trace == "demo;" );
}

