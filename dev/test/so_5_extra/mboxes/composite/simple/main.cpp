#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/composite.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace composite_ns = so_5::extra::mboxes::composite;

struct msg_first final : public so_5::message_t
{};

TEST_CASE( "builder" )
{
	run_with_time_limit( [] {
			so_5::launch( [](so_5::environment_t & env) {
						auto first_mbox = env.create_mbox();

						auto mb = composite_ns::builder(
								so_5::mbox_type_t::multi_producer_multi_consumer,
								composite_ns::drop_if_not_found() )
							.add< msg_first >( first_mbox )
							.make( env );
					} );
		},
		5 );
}

