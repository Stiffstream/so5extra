#include <so_5_extra/disp/asio_one_thread/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

#include "a.hpp"
#include "b.hpp"

int main()
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					namespace asio_ot = so_5::extra::disp::asio_one_thread;

					asio_ot::disp_params_t params;
					params.use_own_io_context();

					auto disp = asio_ot::make_dispatcher(
							env,
							"asio_ot",
							std::move(params) );

					make_coop_a( env, disp );
					make_coop_b( env, disp );
				} );
		},
		5 );
}

