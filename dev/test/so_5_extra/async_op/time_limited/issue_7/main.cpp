#include <so_5/all.hpp>
#include <so_5/send_functions.hpp>
#include <so_5_extra/async_op/time_limited.hpp>

#include "../ensure_destroyed_stuff.hpp"

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

#include <chrono>

namespace asyncop = so_5::extra::async_op::time_limited;

using namespace std::chrono_literals;

class demo final : public so_5::agent_t
{
	struct timeout_signal final : so_5::message_t
	{
		bool * m_timeout_signal_destroyed;

		timeout_signal( bool * timeout_signal_destroyed )
			: m_timeout_signal_destroyed{ timeout_signal_destroyed }
		{}
		~timeout_signal() override
		{
			*m_timeout_signal_destroyed = true;
		}
	};

	struct reply_msg final : so_5::message_t
	{ };

	struct shutdown_signal final : so_5::signal_t
	{ };

	struct additional_data
	{
		bool * m_additional_data_destroyed;

		additional_data( bool * additional_data_destroyed )
			: m_additional_data_destroyed{ additional_data_destroyed }
		{}

		~additional_data()
		{
			*m_additional_data_destroyed = true;
		}
	};

	bool * m_timeout_signal_destroyed;
	bool * m_additional_data_destroyed;

public:
	demo( context_t ctx,
		bool * timeout_signal_destroyed,
		bool * additional_data_destroyed )
		: so_5::agent_t{ std::move(ctx) }
		, m_timeout_signal_destroyed{ timeout_signal_destroyed }
		, m_additional_data_destroyed{ additional_data_destroyed }
	{}

	void so_evt_start() override
	{
		std::cout << "[demo] so_evt_start" << std::endl;

		asyncop::definition_point_t< timeout_signal, test_op_data_t >(
				::so_5::outliving_mutable(*this) )
			.completed_on(
				so_direct_mbox(),
				so_default_state(),
				[this,
				ad = std::make_shared<additional_data>(m_additional_data_destroyed)]
				(so_5::mhood_t<reply_msg>)
				{
					std::cout << "[demo] reply_msg received" << std::endl;
				})
			.timeout_handler(
				so_default_state(),
				[this](so_5::mhood_t<timeout_signal>)
				{
					std::cout << "[demo] timeout_signal received" << std::endl;
				})
			.activate( 150ms, m_timeout_signal_destroyed );

		so_5::send_delayed<reply_msg>( so_direct_mbox(), 100ms );

		so_5::send_delayed<shutdown_signal>( so_direct_mbox(), 50ms );
	}

	void so_define_agent() override
	{
		std::cout << "[demo] so_define_agent" << std::endl;
		so_subscribe_self().event(
			[this](so_5::mhood_t<shutdown_signal>)
			{
				std::cout << "[demo] shutdown_signal received" << std::endl;
				so_deregister_agent_coop_normally();
			});
	}

	void so_evt_finish() override
	{
		std::cout << "[demo] so_evt_finish" << std::endl;
	}
};

int main()
{
	run_with_time_limit( [&] {
			bool timeout_signal_destroyed = false;
			bool additional_data_destroyed = false;

			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< demo >(
										&timeout_signal_destroyed,
										&additional_data_destroyed ) );
					});

			ensure_or_die( 0 == test_op_data_t::live_items(),
					"There should not be any live op_data instances" );
			ensure_or_die( timeout_signal_destroyed,
					"the timeout_signal_destroyed has to be set" );
			ensure_or_die( additional_data_destroyed,
					"the additional_data_destroyed has to be set" );
		},
		5 );

	return 0;
}

