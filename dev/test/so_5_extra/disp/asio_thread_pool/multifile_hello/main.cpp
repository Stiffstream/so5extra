#include <so_5_extra/disp/asio_thread_pool/pub.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

#include "a.hpp"
#include "b.hpp"

int main()
{
	run_with_time_limit( [] {
			so_5::launch( [&](so_5::environment_t & env) {
					namespace asio_tp = so_5::extra::disp::asio_thread_pool;

					asio_tp::disp_params_t params;
					params.use_own_io_context();

					auto disp = asio_tp::create_private_disp(
							env,
							"asio_tp",
							std::move(params) );

					make_coop_a( env, disp );
					make_coop_b( env, disp );
				} );
		},
		5 );
}

