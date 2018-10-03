#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/retained_msg.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

struct data final : public so_5::message_t
{
	int m_value;

	data(int value) : m_value(value) {}
};

struct make_subscription final : public so_5::signal_t {};
struct subscription_made final : public so_5::signal_t {};

void
update_scenario(
	so_5::outliving_reference_t<std::string> & scenario,
	const data & data)
{
	auto & s = scenario.get();
	s += std::to_string(data.m_value);
	s += ";";
}

class with_delivery_filter_case_t final : public so_5::agent_t
{
public :
	with_delivery_filter_case_t(
		context_t ctx,
		so_5::outliving_reference_t<std::string> scenario,
		so_5::mbox_t control_mbox,
		so_5::mbox_t retained_mbox)
		:	so_5::agent_t( std::move(ctx) )
		,	m_scenario( scenario )
		,	m_control_mbox( std::move(control_mbox) )
		,	m_retained_mbox( std::move(retained_mbox) )
	{}

	virtual void
	so_define_agent() override
	{
		so_set_delivery_filter(m_retained_mbox,
			[](const data & cmd) { return cmd.m_value > 42; });

		so_subscribe(m_control_mbox).event(
			[this](mhood_t<make_subscription>) {
				so_subscribe(m_retained_mbox).event(
					[this](mhood_t<data> cmd) {
						update_scenario(m_scenario, *cmd);
					});
				so_5::send<subscription_made>(m_control_mbox);
			});
	}

private :
	so_5::outliving_reference_t<std::string> m_scenario;
	const so_5::mbox_t m_control_mbox;
	const so_5::mbox_t m_retained_mbox;
};

class without_delivery_filter_case_t final : public so_5::agent_t
{
public :
	without_delivery_filter_case_t(
		context_t ctx,
		so_5::outliving_reference_t<std::string> scenario,
		so_5::mbox_t control_mbox,
		so_5::mbox_t retained_mbox)
		:	so_5::agent_t( std::move(ctx) )
		,	m_scenario( scenario )
		,	m_control_mbox( std::move(control_mbox) )
		,	m_retained_mbox( std::move(retained_mbox) )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe(m_control_mbox).event(
			[this](mhood_t<make_subscription>) {
				so_subscribe(m_retained_mbox).event(
					[this](mhood_t<data> cmd) {
						update_scenario(m_scenario, *cmd);
					});
				so_5::send<subscription_made>(m_control_mbox);
			});
	}

private :
	so_5::outliving_reference_t<std::string> m_scenario;
	const so_5::mbox_t m_control_mbox;
	const so_5::mbox_t m_retained_mbox;
};

template<typename Test_Case>
class supervisor_t final : public so_5::agent_t
{
public:
	supervisor_t(
		context_t ctx,
		so_5::outliving_reference_t<std::string> scenario)
		:	so_5::agent_t{std::move(ctx)}
		,	m_scenario{scenario}
		,	m_control_mbox{so_environment().create_mbox()}
		,	m_retained_mbox{so_5::extra::mboxes::retained_msg::make_mbox<>(
				so_environment())}
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe(m_control_mbox).event(
			&supervisor_t::on_subscription_made);
	}

	virtual void
	so_evt_start() override
	{
		so_5::send<data>(m_retained_mbox, 42);

		so_5::introduce_child_coop(*this,
			[this](so_5::coop_t & coop) {
				coop.make_agent<Test_Case>(
						m_scenario,
						m_control_mbox,
						m_retained_mbox);
			});

		so_5::send<make_subscription>(m_control_mbox);
	}

private:
	so_5::outliving_reference_t<std::string> m_scenario;
	const so_5::mbox_t m_control_mbox;
	const so_5::mbox_t m_retained_mbox;

	void
	on_subscription_made(mhood_t<subscription_made>)
	{
		so_5::send<data>(m_retained_mbox, 43);
		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "test case with delivery filter" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent<
											supervisor_t<with_delivery_filter_case_t>
										>(
										so_5::outliving_mutable(scenario) ) );
					});

			REQUIRE( "43;" == scenario );
		},
		5 );
}

TEST_CASE( "test case without delivery filter" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent<
											supervisor_t<without_delivery_filter_case_t>
										>(
										so_5::outliving_mutable(scenario) ) );
					});

			REQUIRE( "42;43;" == scenario );
		},
		5 );
}

