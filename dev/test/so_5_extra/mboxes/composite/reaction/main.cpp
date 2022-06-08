#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/composite.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace composite_ns = so_5::extra::mboxes::composite;

struct msg_first final : public so_5::message_t
{};

struct msg_second final : public so_5::message_t
{};

class redirect_to_reaction final : public so_5::agent_t
{
	const so_5::mbox_t m_second_mbox;
	const so_5::mbox_t m_composite_mbox;

	[[nodiscard]]
	static so_5::mbox_t
	make_composite_mbox(
		const so_5::mbox_t & first_mbox,
		const so_5::mbox_t & second_mbox )
	{
		return composite_ns::builder(
				so_5::mbox_type_t::multi_producer_multi_consumer,
				composite_ns::redirect_to_if_not_found( second_mbox ) )
			.add< msg_first >( first_mbox )
			.make( first_mbox->environment() );
	}

public:
	redirect_to_reaction( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_second_mbox{ so_make_new_direct_mbox() }
		,	m_composite_mbox{
				make_composite_mbox( so_direct_mbox(), m_second_mbox )
			}
	{}

	void
	so_define_agent() override
	{
		so_set_delivery_filter( m_composite_mbox,
				[]( const msg_first & ) { return true; } );

		// Shouldn't throw.
		so_set_delivery_filter( m_composite_mbox,
				[]( const msg_second & ) { return true; } );

		so_subscribe_self()
			.event( [this]( mhood_t<msg_first> ) {
					so_5::send< msg_second >( m_composite_mbox );
				} )
			;

		so_subscribe( m_second_mbox )
			.event( [this]( mhood_t<msg_second> ) {
					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< msg_first >( m_composite_mbox );
	}
};

class throw_exception_reaction final : public so_5::agent_t
{
	const so_5::mbox_t m_composite_mbox;

	[[nodiscard]]
	static so_5::mbox_t
	make_composite_mbox(
		const so_5::mbox_t & first_mbox )
	{
		return composite_ns::builder(
				so_5::mbox_type_t::multi_producer_multi_consumer,
				composite_ns::throw_if_not_found() )
			.add< msg_first >( first_mbox )
			.make( first_mbox->environment() );
	}

	template< typename Lambda >
	static void
	expect_exception(
		const std::string & where,
		Lambda && lambda )
	{
		bool thrown = false;

		try
		{
			lambda();
		}
		catch( const so_5::exception_t & ex )
		{
			std::cout << "*** exception caught: " << ex.what() << std::endl;
			if( composite_ns::errors::rc_no_sink_for_message_type == ex.error_code() )
				thrown = true;
		}

		if( !thrown )
			throw std::runtime_error{ where + ": expected exception isn't thrown" };
	}

public:
	throw_exception_reaction( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_composite_mbox{
				make_composite_mbox( so_direct_mbox() )
			}
	{}

	void
	so_define_agent() override
	{
		so_set_delivery_filter( m_composite_mbox,
				[]( const msg_first & ) { return true; } );

		// Should throw.
		expect_exception(
				"set_delivery_filter",
				[this]() {
					so_set_delivery_filter( m_composite_mbox,
							[]( const msg_second & ) { return true; } );
				} );

		so_subscribe_self()
			.event( [this]( mhood_t<msg_first> ) {
					// Should throw.
					expect_exception(
							"send_msg",
							[this]() {
								so_5::send< msg_second >( m_composite_mbox );
							} );

					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< msg_first >( m_composite_mbox );
	}
};

class drop_reaction final : public so_5::agent_t
{
	const so_5::mbox_t m_composite_mbox;

	[[nodiscard]]
	static so_5::mbox_t
	make_composite_mbox(
		const so_5::mbox_t & first_mbox )
	{
		return composite_ns::builder(
				so_5::mbox_type_t::multi_producer_multi_consumer,
				composite_ns::drop_if_not_found() )
			.add< msg_first >( first_mbox )
			.make( first_mbox->environment() );
	}

public:
	drop_reaction( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_composite_mbox{
				make_composite_mbox( so_direct_mbox() )
			}
	{}

	void
	so_define_agent() override
	{
		so_set_delivery_filter( m_composite_mbox,
				[]( const msg_first & ) { return true; } );

		// Shouldn't throw.
		so_set_delivery_filter( m_composite_mbox,
				[]( const msg_second & ) { return true; } );

		so_subscribe_self()
			.event( [this]( mhood_t<msg_first> ) {
					// Shouldn't throw.
					so_5::send< msg_second >( m_composite_mbox );

					so_deregister_agent_coop_normally();
				} )
			;
	}

	void
	so_evt_start() override
	{
		so_5::send< msg_first >( m_composite_mbox );
	}
};

TEST_CASE( "redirect_to_if_not_found" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< redirect_to_reaction >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

TEST_CASE( "throw_if_not_found" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< throw_exception_reaction >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

TEST_CASE( "drop_if_not_found" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< drop_reaction >() );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

