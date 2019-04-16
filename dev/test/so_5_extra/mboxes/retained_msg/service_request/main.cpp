#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/retained_msg.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

class a_test_case_t final : public so_5::agent_t
{
	struct retained_data final : public so_5::message_t
	{
		int m_value;

		retained_data(int value) : m_value(value) {}
	};

public :
	a_test_case_t( context_t ctx )
		:	so_5::agent_t( std::move(ctx) )
		,	m_mbox( so_5::extra::mboxes::retained_msg::make_mbox<>(
					so_environment() ) )
	{}

	virtual void
	so_evt_start() override
	{
		try
		{
			so_5::request_value<void, retained_data>(
					m_mbox, so_5::infinite_wait,42);

			throw std::runtime_error("request_value should throw!");
		}
		catch(const so_5::exception_t & x)
		{
			ensure_or_die(
				so_5::extra::mboxes::retained_msg::errors::
						rc_service_request_via_retained_msg_mbox ==
				x.error_code(),
				"rc_service_request_via_retained_msg_mbox expected, got: " +
				std::to_string(x.error_code()) );
		}

		so_deregister_agent_coop_normally();
	}

private :
	const so_5::mbox_t m_mbox;
};

TEST_CASE( "simplest agent with single retained message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

