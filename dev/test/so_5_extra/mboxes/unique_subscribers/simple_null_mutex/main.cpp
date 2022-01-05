#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/unique_subscribers.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace mbox_ns = so_5::extra::mboxes::unique_subscribers;

class first final : public so_5::agent_t
{
	struct ready final : public so_5::signal_t {};

	const so_5::mbox_t m_test_mbox;

public:
	first( context_t ctx, so_5::mbox_t test_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_test_mbox{ std::move(test_mbox) }
	{}

	void
	so_define_agent() override
	{
		so_subscribe( m_test_mbox )
			.event( [this]( mhood_t<ready> ) {
					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< ready >( m_test_mbox );
	}
};

class second final : public so_5::agent_t
{
	struct ready final : public so_5::message_t
	{
		std::string m_data;

		explicit ready( std::string data ) : m_data{ std::move(data) } {}
	};

	const so_5::mbox_t m_test_mbox;

public:
	second( context_t ctx, so_5::mbox_t test_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_test_mbox{ std::move(test_mbox) }
	{}

	void
	so_define_agent() override
	{
		so_subscribe( m_test_mbox )
			.event( [this]( mhood_t<ready> cmd ) {
					std::cout << "second.ready: " << cmd->m_data << std::endl;
					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< ready >( m_test_mbox, "Hello, Immutable World!" );
	}
};

class third final : public so_5::agent_t
{
	struct ready final : public so_5::message_t
	{
		std::string m_data;

		explicit ready( std::string data ) : m_data{ std::move(data) } {}
	};

	const so_5::mbox_t m_test_mbox;

public:
	third( context_t ctx, so_5::mbox_t test_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_test_mbox{ std::move(test_mbox) }
	{}

	void
	so_define_agent() override
	{
		so_subscribe( m_test_mbox )
			.event( [this]( mutable_mhood_t<ready> cmd ) {
					std::cout << "third.ready: " << cmd->m_data << std::endl;
					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< so_5::mutable_msg<ready> >(
				m_test_mbox, "Hello, Mutable World!" );
	}
};

TEST_CASE( "simple case" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						auto test_mbox = mbox_ns::mbox_template_t< so_5::null_mutex_t >::make( env );

						env.register_agent_as_coop(
								env.make_agent< first >( test_mbox ) );

						env.register_agent_as_coop(
								env.make_agent< second >( test_mbox ) );

						env.register_agent_as_coop(
								env.make_agent< third >( test_mbox ) );
					},
					[](so_5::environment_params_t & params) {
						params.infrastructure_factory(
								so_5::env_infrastructures::simple_not_mtsafe::factory() );

						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

