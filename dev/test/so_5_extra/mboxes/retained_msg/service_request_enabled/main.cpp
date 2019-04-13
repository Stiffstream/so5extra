#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/retained_msg.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

class a_service_provider_t final : public so_5::agent_t
{
public :
	struct get_value final : public so_5::signal_t {};

	a_service_provider_t( context_t ctx, const so_5::mbox_t & mbox )
		:	so_5::agent_t( std::move(ctx) )
	{
		so_subscribe(mbox).event( [](mhood_t<get_value>) -> int { return 42; } );
	}
};

class a_test_case_t final : public so_5::agent_t
{
public :
	a_test_case_t( context_t ctx, so_5::mbox_t mbox )
		:	so_5::agent_t( std::move(ctx) )
		,	m_mbox( std::move(mbox) )
	{}

	virtual void
	so_evt_start() override
	{
		const auto r = so_5::request_value<int, a_service_provider_t::get_value>(
				m_mbox, so_5::infinite_wait);

		ensure_or_die(
			r == 42,
			"42 is expected as result, got: " + std::to_string(r) );

		so_deregister_agent_coop_normally();
	}

private :
	const so_5::mbox_t m_mbox;
};

void
make_test_coop(so_5::coop_t & coop)
{
	auto mbox = so_5::extra::mboxes::retained_msg::make_mbox<
				so_5::extra::mboxes::retained_msg::with_service_request_traits_t>(
			coop.environment() );

	coop.make_agent<a_service_provider_t>(mbox);
	coop.make_agent<a_test_case_t>(mbox);
}

TEST_CASE( "enabled service request" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					env.introduce_coop(
							so_5::disp::active_obj::create_private_disp(env)->binder(),
							[](so_5::coop_t & coop) {
								make_test_coop( coop );
							} );
				},
				[](so_5::environment_params_t & params) {
					params.message_delivery_tracer(
							so_5::msg_tracing::std_cout_tracer() );
				} );
		},
		5 );
}

