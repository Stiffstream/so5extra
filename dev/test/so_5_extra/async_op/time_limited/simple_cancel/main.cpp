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

		struct cancel_op final : public so_5::signal_t {};

		struct finish final : public so_5::signal_t {};

		asyncop::cancellation_point_t<> m_cp;

	public :
		a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

		virtual void
		so_evt_start() override
			{
				so_subscribe_self()
					.event( [this]( mhood_t<cancel_op> ) {
							ensure_or_die( m_cp.is_cancellable(),
									"async_op should be cancellable" );

							m_cp.cancel();

							ensure_or_die( !m_cp.is_cancellable(),
									"async_op should not be cancellable after cancel()" );

							so_5::send<unused>( *this );
							so_5::send_delayed< finish >( *this,
									std::chrono::milliseconds(100) );
						} )
					.event( [this]( mhood_t<finish> ) {
							so_deregister_agent_coop_normally();
						} );

				m_cp = asyncop::make<timeout>( *this )
					.completed_on(
							*this,
							so_default_state(),
							[]( mhood_t<unused> ) {
								ensure_or_die( false,
										"completion handler for cancelled async_op!" );
							} )
					.timeout_handler(
							so_default_state(),
							[]( mhood_t<timeout> ) {
								ensure_or_die( false,
										"timeout handler for cancelled async_op!" );
							} )
					.activate( std::chrono::milliseconds(100), "timedout" );

				so_5::send_delayed< cancel_op >(
						*this,
						std::chrono::milliseconds(50) );
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


