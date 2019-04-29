#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/retained_msg.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

class a_test_case_t final : public so_5::agent_t
{
	state_t st_wait_first{ this, "wait_first" };
	state_t st_wait_second{ this, "wait_second" };
	state_t st_wait_third{ this, "wait_third" };

	struct data final : public so_5::message_t
	{
		int m_value;

		data(int value) : m_value(value) {}
	};

public :
	a_test_case_t(
		context_t ctx,
		so_5::outliving_reference_t<std::string> scenario )
		:	so_5::agent_t( std::move(ctx) )
		,	m_scenario( scenario )
		,	m_mbox( so_5::extra::mboxes::retained_msg::make_mbox<>(
					so_environment() ) )
	{}

	virtual void
	so_evt_start() override
	{
		so_5::send<data>(m_mbox, 42);

		this >>= st_wait_first;
		st_wait_first.event(m_mbox, [this](mhood_t<data> cmd) {
			update_scenario(*cmd);

			this >>= st_wait_second;
			so_5::send<data>(m_mbox, 43);
		});

		st_wait_second.event(m_mbox, [this](mhood_t<data> cmd) {
			update_scenario(*cmd);

			this >>= st_wait_third;
			so_5::send<data>(m_mbox, 44);
		});

		st_wait_third.event(m_mbox, [this](mhood_t<data> cmd) {
			update_scenario(*cmd);

			so_deregister_agent_coop_normally();
		});
	}

private :
	so_5::outliving_reference_t<std::string> m_scenario;
	const so_5::mbox_t m_mbox;

	void
	update_scenario(const data & cmd)
	{
		m_scenario.get() += std::to_string(cmd.m_value);
		m_scenario.get() += ";";
	}
};

TEST_CASE( "simplest agent with single retained message" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t >(
										so_5::outliving_mutable(scenario) ) );
					});

			REQUIRE( "42;43;44;" == scenario );
		},
		5 );
}

