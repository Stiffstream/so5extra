#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

#include "../ensure_destroyed_stuff.hpp"

namespace asyncop = ::so_5::extra::async_op::time_limited;

class a_test_t final : public so_5::agent_t
	{
		struct completed final : public so_5::signal_t {};
		struct timeout final : public so_5::signal_t {};
		struct cancel final : public so_5::signal_t {};
		struct finish final : public so_5::signal_t {};

		asyncop::cancellation_point_t<test_op_data_t> m_cp;

	public :
		a_test_t( context_t ctx ) : so_5::agent_t( std::move(ctx) ) {}

		virtual void
		so_evt_start() override
			{
				so_default_state()
					.event( [this]( mhood_t<cancel> ) {
							m_cp.cancel();
							so_5::send_delayed< finish >(
									*this,
									std::chrono::milliseconds(100) );
						} )
					.event( [this]( mhood_t<finish> ) {
							so_deregister_agent_coop_normally();
						} );

				m_cp = asyncop::definition_point_t<timeout, test_op_data_t>(
						::so_5::outliving_mutable(*this) )
					.completed_on(
							*this,
							so_default_state(),
							[]( mhood_t<completed> ) {
								ensure_or_die( false,
										"This should never happen!" );
							} )
					.timeout_handler(
							so_default_state(),
							[]( mhood_t<timeout> ) {
								ensure_or_die( false,
										"This should never happen 2!" );
							} )
					.activate( std::chrono::milliseconds(100) );

				so_5::send_delayed< cancel >(
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

			ensure_or_die( 0 == test_op_data_t::live_items(),
					"There should not be any live op_data instances" );
		},
		5 );

	return 0;
}

