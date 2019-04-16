#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/proxy.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace proxy_mbox_ns = so_5::extra::mboxes::proxy;

struct ping final : public so_5::message_t
	{
		const so_5::mbox_t m_reply_to;

		ping( so_5::mbox_t reply_to ) : m_reply_to{ std::move(reply_to) } {}
	};

struct pong final : public so_5::signal_t {};

struct shutdown final : public so_5::signal_t {};

class master_t final : public so_5::agent_t
	{
		const so_5::mbox_t m_mbox;

	public :
		master_t( context_t ctx, so_5::mbox_t mbox )
			:	so_5::agent_t{ std::move(ctx) }
			,	m_mbox{ std::move(mbox) }
			{
				so_subscribe_self().event( &master_t::on_pong );
			}

		void
		so_evt_start() override
			{
				so_5::send<ping>( m_mbox, so_direct_mbox() );
			}

	private :
		void
		on_pong( mhood_t<pong> )
			{
				so_5::send_delayed< shutdown >(
						m_mbox,
						std::chrono::milliseconds(50) );
			}
	};

class slave_t final : public so_5::agent_t
	{
	public :
		slave_t( context_t ctx )
			: so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self()
					.event( []( mhood_t<ping> cmd ) {
							so_5::send< pong >( cmd->m_reply_to );
						} )
					.event( [this]( mhood_t<shutdown> ) {
							so_deregister_agent_coop_normally();
						} );
			}
	};

TEST_CASE( "simple proxy" )
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop(
							so_5::disp::active_obj::make_dispatcher( env ).binder(),
							[]( so_5::coop_t & coop ) {
								auto slave = coop.make_agent< slave_t >();
								coop.make_agent< master_t >(
										so_5::mbox_t{
											std::make_unique< proxy_mbox_ns::simple_t >(
												slave->so_direct_mbox() )
										} );
							} );
					},
					[](so_5::environment_params_t & params) {
						params.message_delivery_tracer(
								so_5::msg_tracing::std_cout_tracer() );
					} );
		},
		5 );
}

