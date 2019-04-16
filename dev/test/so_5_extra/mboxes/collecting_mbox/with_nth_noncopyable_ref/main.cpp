#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/collecting_mbox.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

namespace collecting_mbox_ns = so_5::extra::mboxes::collecting_mbox;

class builder_t final
{
	const std::string m_arg;

public :
	builder_t( std::string arg ) : m_arg(std::move(arg)) {}
	builder_t( const builder_t & ) = delete;
	builder_t( builder_t && ) = delete;

	std::string
	transform( const std::string & what ) const
	{
		return m_arg + what + ";";
	}
};

class hello final : public so_5::message_t
{
	const builder_t m_builder;

public :
	hello( std::string data ) : m_builder( std::move(data) ) {}

	const builder_t &
	builder() const { return m_builder; }
};

struct constexpr_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			hello,
			collecting_mbox_ns::constexpr_size_traits_t<3> >;

	static auto
	make( so_5::environment_t & env, const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( env, target );
	}
};

struct runtime_case
{
	using collecting_mbox_t = collecting_mbox_ns::mbox_template_t<
			hello,
			collecting_mbox_ns::runtime_size_traits_t >;

	static auto
	make( so_5::environment_t & env, const so_5::mbox_t & target )
	{
		return collecting_mbox_t::make( env, target, 3u );
	}
};

template< typename CASE >
class a_test_case_t final : public so_5::agent_t
{
	using collecting_mbox_t = typename CASE::collecting_mbox_t;

public :
	a_test_case_t(
		context_t ctx,
		std::string & dest )
		:	so_5::agent_t( std::move(ctx) )
		,	m_dest( dest )
		,	m_mbox( CASE::make( so_environment(), so_direct_mbox() ) )
	{}

	virtual void
	so_define_agent() override
	{
		so_subscribe_self().event( &a_test_case_t::on_messages_collected );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send< hello >( m_mbox, "one=" );
		so_5::send< hello >( m_mbox, "two=" );
		so_5::send< hello >( m_mbox, "three=" );
	}

private :
	std::string & m_dest;

	const so_5::mbox_t m_mbox;

	void
	on_messages_collected( mhood_t<typename collecting_mbox_t::messages_collected_t> cmd)
	{
		for( std::size_t i = 0; i != cmd->size(); ++i )
		{
			m_dest += cmd->with_nth( i,
					[](auto m) -> const builder_t & { return m->builder(); }
				).transform( std::to_string(i) );
		}

		so_deregister_agent_coop_normally();
	}
};

TEST_CASE( "constexpr case" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t< constexpr_case > >(
										std::ref(scenario) ) );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );

			REQUIRE( scenario == "one=0;two=1;three=2;" );
		},
		5 );
}

TEST_CASE( "runtime case" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_case_t< constexpr_case > >(
										std::ref(scenario) ) );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );

			REQUIRE( scenario == "one=0;two=1;three=2;" );
		},
		5 );
}

