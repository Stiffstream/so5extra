#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

namespace asyncop = ::so_5::extra::async_op::time_limited;

class a_test_t final : public so_5::agent_t
	{
		struct timeout final : public so_5::message_t
			{
				const std::string m_msg;

				timeout( std::string msg ) : m_msg( std::move(msg) ) {}
			};

		struct unused final : public so_5::message_t {};

		struct expected final : public so_5::message_t
			{
				const std::string m_msg;

				expected( std::string msg ) : m_msg( std::move(msg) ) {}
			};

		struct complete_test final : public so_5::signal_t {};

	public :
		a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

		virtual void
		so_evt_start() override
			{
				so_subscribe_self()
					.event( [this]( mhood_t<complete_test> ) {
								so_5::send< unused >( *this );
								so_5::send< expected >( *this, "bye!" );
							} )
					.event( [this]( mhood_t< expected > cmd ) {
								ensure_or_die( "bye!" == cmd->m_msg,
										"'bye!' is expected" );
								so_deregister_agent_coop_normally();
							} );

				asyncop::make<timeout>( *this )
					.completed_on(
							*this,
							so_default_state(),
							[]( mhood_t<unused> ) {
								throw std::runtime_error( "This should never happen!" );
							} )
					.activate( std::chrono::milliseconds(50), "timedout" );

				so_5::send_delayed< complete_test >(
						*this,
						std::chrono::milliseconds(100) );
			}
	};

int main()
{
	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop( "test",
								env.make_agent< a_test_t >() );
					});
		},
		5 );

	return 0;
}


