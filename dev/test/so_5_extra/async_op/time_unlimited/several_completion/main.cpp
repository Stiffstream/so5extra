#include <so_5_extra/async_op/time_unlimited.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

using namespace ::so_5::extra::async_op::time_unlimited;

class a_test_t final : public so_5::agent_t
	{
	private :
		state_t st_A{ this };
		state_t st_B{ this };
		state_t st_C{ this };

		struct A final : public so_5::message_t
			{
				int m_value;
				A( int value ) : m_value(value) {}
			};

		struct B final : public so_5::message_t
			{
				int m_value;
				B( int value ) : m_value(value) {}
			};

		struct C final : public so_5::message_t
			{
				int m_value;
				C( int value ) : m_value(value) {}
			};

	public :
		a_test_t( context_t ctx )
			:	so_5::agent_t( std::move(ctx) )
			{}

		virtual void
		so_evt_start() override
			{
				auto dp = make(*this)
					.completed_on(
						*this,
						st_A,
						[](mhood_t<A>) {
							throw std::runtime_error( "A received!" );
						} )
					.completed_on(
						*this,
						st_B,
						[this](mhood_t<B> cmd) {
							ensure_no_subscriptions();
							ensure_or_die( 2 == cmd->m_value,
									"2 is expected as message payload" );
							so_deregister_agent_coop_normally();
						} )
					.completed_on(
						*this,
						st_C,
						[](mhood_t<C>) {
							throw std::runtime_error( "C received!" );
						} );

				ensure_no_subscriptions();

				dp.activate();

				ensure_or_die(
						so_has_subscription<A>(so_direct_mbox(), st_A),
						"There should be a subscription to A in st_A" );
				ensure_or_die(
						so_has_subscription<B>(so_direct_mbox(), st_B),
						"There should be a subscription to B in st_B" );
				ensure_or_die(
						so_has_subscription<C>(so_direct_mbox(), st_C),
						"There should be a subscription to C in st_C" );

				this >>= st_B;

				so_5::send<B>( *this, 2 );
			}

	private :
		void
		ensure_no_subscriptions()
			{
				ensure_or_die(
						!so_has_subscription<A>(so_direct_mbox(), st_A),
						"There shouldn't be a subscription to A in st_A" );
				ensure_or_die(
						!so_has_subscription<B>(so_direct_mbox(), st_B),
						"There shouldn't be a subscription to B in st_B" );
				ensure_or_die(
						!so_has_subscription<C>(so_direct_mbox(), st_C),
						"There shouldn't be a subscription to C in st_C" );
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

