#include <so_5_extra/async_op/time_limited.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>
#include <various_helpers_1/ensure.hpp>

using namespace ::so_5::extra::async_op::time_limited;

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

		struct timeout final : public so_5::signal_t {};

	public :
		a_test_t( context_t ctx )
			:	so_5::agent_t( std::move(ctx) )
			{}

		virtual void
		so_evt_start() override
			{
				auto one_shot = make< timeout >( *this );

				one_shot.completed_on(
						*this,
						st_A,
						[](mhood_t<A>) {} )
					.completed_on(
						*this,
						st_B,
						[](mhood_t<B>) {} )
					.completed_on(
						*this,
						st_C,
						[](mhood_t<C>) {} )
					.completed_on(
						*this,
						st_A,
						[](mhood_t<A>) {} );

				ensure_no_subscriptions();

				try
					{
						one_shot.activate( std::chrono::milliseconds(200) );

						throw std::runtime_error( "An exception must be "
								"thrown in one_shot.activate()!" );
					}
				catch( const ::so_5::exception_t & x )
					{
						ensure_or_die(
								::so_5::rc_evt_handler_already_provided ==
										x.error_code(),
								"rc_evt_handler_already_provided expected" );
					}

				ensure_or_die(
						!one_shot.is_activable(),
						"not_activated status is expected after exception in "
						"activate()" );

				ensure_no_subscriptions();

				so_deregister_agent_coop_normally();
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

