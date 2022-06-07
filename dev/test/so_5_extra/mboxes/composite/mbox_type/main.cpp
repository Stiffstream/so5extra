#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/composite.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

#include <tuple>

namespace composite_ns = so_5::extra::mboxes::composite;

struct msg_first final : public so_5::message_t
{};

struct msg_second final : public so_5::message_t
{};

class mpmc_mbox_case final : public so_5::agent_t
{
	const so_5::mbox_t m_composite_mbox;

	[[nodiscard]]
	static so_5::mbox_t
	make_composite_mbox(
		const so_5::mbox_t & mpsc_mbox,
		const so_5::mbox_t & mpmc_mbox )
	{
		auto builder = composite_ns::multi_consumer_builder(
				composite_ns::throw_if_not_found() )
			.add< msg_first >( mpsc_mbox )
			.add< msg_second >( mpmc_mbox );

		try
		{
			builder.add< so_5::mutable_msg< msg_first > >( mpsc_mbox );
			throw std::runtime_error{ "an exception has to be throw by "
					"builder.add" };
		}
		catch( const so_5::exception_t & ex )
		{
			std::cout << "*** exception caught: " << ex.what() << std::endl;
			if( so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox != ex.error_code() )
				throw std::runtime_error{ "unexpected exception caught" };
		}

		return builder.make( mpsc_mbox->environment() );
	}

public:
	mpmc_mbox_case( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_composite_mbox{
				make_composite_mbox(
						so_direct_mbox(),
						so_environment().create_mbox() )
			}
	{}

	void
	so_evt_start() override
	{
		so_5::send< msg_first >( m_composite_mbox );
		so_5::send< msg_second >( m_composite_mbox );

		try
		{
			so_5::send< so_5::mutable_msg< msg_first > >( m_composite_mbox );
			throw std::runtime_error{ "an exception has to be throw by send()" };
		}
		catch( const so_5::exception_t & ex )
		{
			std::cout << "*** exception caught: " << ex.what() << std::endl;
			if( so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox != ex.error_code() )
				throw std::runtime_error{ "unexpected exception caught" };
		}

		so_deregister_agent_coop_normally();
	}
};

class mpsc_mbox_case final : public so_5::agent_t
{
	const so_5::mbox_t m_composite_mbox;

	[[nodiscard]]
	static so_5::mbox_t
	make_composite_mbox(
		const so_5::mbox_t & mpsc_mbox,
		const so_5::mbox_t & mpmc_mbox )
	{
		auto builder = composite_ns::single_consumer_builder(
				composite_ns::throw_if_not_found() )
			.add< msg_first >( mpsc_mbox );

		// MPMC mbox can be added as a sink for immutable message.
		builder.add< msg_second >( mpmc_mbox );

		builder.add< so_5::mutable_msg< msg_first > >( mpsc_mbox );

		try
		{
			builder.add< so_5::mutable_msg< msg_second > >( mpmc_mbox );
			throw std::runtime_error{ "an exception has to be throw by "
					"builder.add<so_5::mutable_msg<msg_second>>(mpmc_mbox)" };
		}
		catch( const so_5::exception_t & ex )
		{
			std::cout << "*** exception caught: " << ex.what() << std::endl;
			if( so_5::extra::mboxes::composite::errors::
					rc_mpmc_sink_can_be_used_with_mpsc_composite != ex.error_code() )
				throw std::runtime_error{
						"unexpected exception caught, error_code=" +
								std::to_string( ex.error_code() )
					};
		}

		return builder.make( mpsc_mbox->environment() );
	}

public:
	mpsc_mbox_case( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_composite_mbox{
				make_composite_mbox(
						so_direct_mbox(),
						so_environment().create_mbox() )
			}
	{}

	void
	so_evt_start() override
	{
		so_5::send< msg_first >( m_composite_mbox );
		so_5::send< so_5::mutable_msg< msg_first > >( m_composite_mbox );

		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "mpmc_mbox" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< mpmc_mbox_case >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

TEST_CASE( "mpsc_mbox" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< mpsc_mbox_case >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

