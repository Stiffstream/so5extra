#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

class service_t final : public so_5::agent_t
	{
		struct delayed_request final : public so_5::message_t
			{
				so_5::message_holder_t< so_5::mutable_msg<
						sync_ns::request_reply_t<int, int> > > m_request;

				delayed_request(
					so_5::message_holder_t< so_5::mutable_msg<
							sync_ns::request_reply_t<int, int> > > request )
					:	m_request{ std::move(request) }
				{}
			};

	public :
		service_t( context_t ctx ) : so_5::agent_t{ std::move(ctx) }
			{
				so_subscribe_self()
					.event( &service_t::on_request )
					.event( &service_t::on_delayed_request );
			}

	private :
		void
		on_request( mutable_mhood_t< sync_ns::request_reply_t<int, int> > cmd )
			{
				so_5::send_delayed< so_5::mutable_msg< delayed_request > >(
						*this,
						10s,
						cmd.make_holder() );
			}

		void
		on_delayed_request( mutable_mhood_t< delayed_request > cmd )
			{
				cmd->m_request->make_reply( cmd->m_request->request() * 2 );
			}
	};

TEST_CASE( "simple shutdown on empty environment" )
{
	enum result_t { undefined, no_reply, reply_received };

	result_t result = undefined;

	std::chrono::steady_clock::time_point sent_at;
	std::chrono::steady_clock::time_point received_at;

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						auto svc = env.introduce_coop( [](so_5::coop_t & coop) {
								return coop.make_agent<service_t>()->so_direct_mbox();
							} );

						try
						{
							sent_at = std::chrono::steady_clock::now();
							auto r = sync_ns::request_value<int, int>( svc, 250ms, 2 );
							result = reply_received;
						}
						catch( const so_5::exception_t & x )
						{
							ensure( sync_ns::errors::rc_no_reply == x.error_code(),
									"sync_ns::errors::no_reply expected, got: " +
									std::to_string( x.error_code() ) );
							result = no_reply;
						}

						received_at = std::chrono::steady_clock::now();

						env.stop();
					} );
		},
		5 );

	REQUIRE( result == no_reply );
	REQUIRE( sent_at + 125ms < received_at );
}

