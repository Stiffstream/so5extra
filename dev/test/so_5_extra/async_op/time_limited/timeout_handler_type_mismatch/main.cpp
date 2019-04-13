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

	public :
		a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

		virtual void
		so_evt_start() override
			{
				auto op = asyncop::make<timeout>( *this );

				try
					{
						op.timeout_handler(
							so_default_state(),
							[]( mhood_t<unused>) {} );

						ensure_or_die( false,
								"An exception expected in call to timeout_handler" );
					}
				catch( const so_5::exception_t & ex )
					{
						ensure_or_die(
							so_5::extra::async_op::errors::rc_msg_type_mismatch
									== ex.error_code(),
							"rc_msg_type_mismatch is expected" );
					}

				so_deregister_agent_coop_normally();
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

