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
	struct timeout_signal final : so_5::signal_t
	{ };

	struct reply_msg final : so_5::message_t
	{ };

	struct shutdown_signal final : so_5::signal_t
	{ };

public:
	using so_5::agent_t::agent_t;

	void so_evt_start() override
	{
		std::cout << "[demo] so_evt_start" << std::endl;
		asyncop::definition_point_t< timeout_signal, test_op_data_t >(
				::so_5::outliving_mutable(*this) )
#if 0
		asyncop::make< timeout_signal, test_op_data_t >(*this)
#endif
			.completed_on(so_direct_mbox(), so_default_state(),
				[this](so_5::mhood_t<reply_msg>)
				{ std::cout << "[demo] reply_msg received" << std::endl; })
			.timeout_handler(so_default_state(), [this](so_5::mhood_t<timeout_signal>)
				{ std::cout << "[demo] timeout_signal received" << std::endl; })
			.activate( 150ms );

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
			so_5::launch( [&](so_5::environment_t & env) {
						env.register_agent_as_coop(
								env.make_agent< demo >() );
					});

			ensure_or_die( 0 == test_op_data_t::live_items(),
					"There should not be any live op_data instances" );
		},
		5 );

	return 0;
}

