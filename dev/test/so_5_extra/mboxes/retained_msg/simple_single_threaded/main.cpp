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
		,	m_mbox(
					so_5::extra::mboxes::retained_msg::make_mbox<
								so_5::extra::mboxes::retained_msg::default_traits_t,
								so_5::null_mutex_t
							>( so_environment() ) )
	{}

	virtual void
	so_evt_start() override
	{
		so_5::send<retained_data>(m_mbox, 42);

		so_subscribe(m_mbox).event(&a_test_case_t::on_retained_data);
	}

private :
	const so_5::mbox_t m_mbox;

	void
	on_retained_data( mhood_t<retained_data> cmd )
	{
		ensure_or_die( 42 == cmd->m_value, "42 expected in retained_data" );

		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "simplest agent with single retained message" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t >() );
					},
					[](so_5::environment_params_t & params) {
						params.infrastructure_factory(
								so_5::env_infrastructures::simple_not_mtsafe::factory());
					} );
		},
		5 );
}

