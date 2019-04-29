#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace asyncop = ::so_5::extra::async_op::time_limited;

class a_test_t final : public so_5::agent_t
	{
		struct timeout final : public so_5::message_t
			{
				const std::string m_msg;

				timeout( std::string msg ) : m_msg( std::move(msg) ) {}
			};

		struct unused final : public so_5::message_t {};

		state_t st_unreacheable{ this, "unreacheable" };

	public :
		a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

		virtual void
		so_evt_start() override
			{
				asyncop::make<timeout>( *this )
					.completed_on(
							*this,
							so_default_state(),
							[]( mhood_t<unused> ) {
								throw std::runtime_error( "This should never happen!" );
							} )
					.timeout_handler(
							st_unreacheable,
							[]( mhood_t<timeout> ) {
								throw std::runtime_error( "timeout handler for "
										"unreacheable state should not be called!" );
							} )
					.default_timeout_handler(
							so_default_state(),
							[this]( mhood_t<timeout> cmd ) {
								ensure_or_die( "timedout" == cmd->m_msg,
										"unexpected value in timeout message: " +
										cmd->m_msg );
								so_deregister_agent_coop_normally();
							} )
					.activate( std::chrono::milliseconds(50), "timedout" );
			}
	};

int main()
{
	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_t >() );
					});
		},
		5 );

	return 0;
}


