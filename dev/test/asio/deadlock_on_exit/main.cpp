#include <thread>
#include <future>

#include <asio.hpp>

int main()
{
	std::promise< asio::io_context * > promise;

	std::thread child_thread{
		[&promise]() {
			asio::io_context ctx;

			asio::post( ctx, [&promise, &ctx]() {
					promise.set_value( &ctx );
				} );

			auto work = asio::make_work_guard( ctx );
			ctx.run();
		}
	};

	auto * ctx_ptr = promise.get_future().get();
	asio::post( *ctx_ptr, [ctx_ptr]() { ctx_ptr->stop(); } );

	child_thread.join();
}

