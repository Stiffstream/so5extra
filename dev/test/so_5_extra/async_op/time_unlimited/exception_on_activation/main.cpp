#include <so_5_extra/async_op/time_unlimited.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

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

		struct additional_data
			{
				int * m_destructions_counter;

				additional_data( int * destructions_counter )
					: m_destructions_counter{ destructions_counter }
					{}

				~additional_data()
					{
						(*m_destructions_counter) += 1;
					}
			};

		int * m_destructions_counter;

		std::shared_ptr< additional_data >
		make_additional_data()
			{
				return std::make_shared< additional_data >( m_destructions_counter );
			}

	public :
		a_test_t( context_t ctx, int * destructions_counter )
			:	so_5::agent_t( std::move(ctx) )
			,	m_destructions_counter{ destructions_counter }
			{}

		virtual void
		so_evt_start() override
			{
				auto dp = make(*this);
				dp.completed_on(
						*this,
						st_A,
						[ad = make_additional_data()](mhood_t<A>) {} )
					.completed_on(
						*this,
						st_B,
						[ad = make_additional_data()](mhood_t<B>) {} )
					.completed_on(
						*this,
						st_C,
						[ad = make_additional_data()](mhood_t<C>) {} )
					.completed_on(
						*this,
						st_A,
						[ad = make_additional_data()](mhood_t<A>) {} );

				ensure_no_subscriptions();

				try
					{
						dp.activate();

						throw std::runtime_error( "An exception must be "
								"thrown in dp.activate()!" );
					}
				catch( const ::so_5::exception_t & x )
					{
						ensure_or_die(
								::so_5::rc_evt_handler_already_provided ==
										x.error_code(),
								"rc_evt_handler_already_provided expected" );
					}

				ensure_or_die(
						!dp.is_activable(),
						"definition_point should be not activable" );

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
			int destructions = 0;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< a_test_t >( &destructions ) );
					});

			ensure_or_die( 4 == destructions,
					"unexpected number of additional data destructions: "
					+ std::to_string( destructions ) );
		},
		5 );

	return 0;
}

