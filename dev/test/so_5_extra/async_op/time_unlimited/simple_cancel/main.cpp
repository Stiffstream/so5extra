#include <so_5_extra/async_op/time_unlimited.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

using namespace ::so_5::extra::async_op::time_unlimited;

class a_test_t final : public so_5::agent_t
	{
	private :
		cancellation_point_t<> m_cp;

		struct cancel_signal final : public so_5::signal_t {};

		struct demo_signal final : public so_5::signal_t {};

		struct finish_signal final : public so_5::signal_t {};

	public :
		a_test_t( context_t ctx )
			:	so_5::agent_t( std::move(ctx) )
			{}

		virtual void
		so_define_agent() override
			{
				so_subscribe_self()
					.event( &a_test_t::on_cancel )
					.event( &a_test_t::on_finish );
			}

		virtual void
		so_evt_start() override
			{
				m_cp = make(*this)
					.completed_on(
						*this,
						so_default_state(),
						&a_test_t::on_demo_signal )
					.activate( [&] {
						so_5::send<cancel_signal>( *this );
						so_5::send<demo_signal>( *this );
						so_5::send<finish_signal>( *this );
					});
			}

	private :
		void
		on_cancel(mhood_t<cancel_signal>)
			{
				m_cp.cancel();
			}

		void
		on_demo_signal(mhood_t<demo_signal>)
			{
				throw std::runtime_error( "on_demo_signal shouldn't be called!" );
			}

		void
		on_finish(mhood_t<finish_signal>)
			{
				ensure_or_die(
						status_t::cancelled == m_cp.status(),
						"status_t::cancelled is expected" );

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
		},
		5 );

	return 0;
}

