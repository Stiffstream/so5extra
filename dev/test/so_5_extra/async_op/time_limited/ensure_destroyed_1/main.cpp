#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

#include "../ensure_destroyed_stuff.hpp"

namespace asyncop = ::so_5::extra::async_op::time_limited;

class a_test_t final : public so_5::agent_t
	{
		struct timeout final : public so_5::message_t
			{
				const std::string m_msg;

				timeout( std::string msg ) : m_msg( std::move(msg) ) {}
			};

		struct unused final : public so_5::signal_t {};

	public :
		a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

		virtual void
		so_evt_start() override
			{
				// No actual activation.
				// op_data must be destroyed automatically.
				asyncop::definition_point_t<timeout, test_op_data_t>(
						::so_5::outliving_mutable(*this) )
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
							} );

				so_deregister_agent_coop_normally();
			}
	};

int main()
{
	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_t >() );
					});

			ensure_or_die( 0 == test_op_data_t::live_items(),
					"There should not be any live op_data instances" );
		},
		5 );

	return 0;
}

