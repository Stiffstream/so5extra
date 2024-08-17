#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <so_5_extra/msg_hierarchy/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

namespace hierarchy_ns = so_5::extra::msg_hierarchy;

using namespace std::chrono_literals;

namespace test
{

struct base_message : public hierarchy_ns::root_t<base_message>
	{
		base_message() = default;
	};

struct data_message_one
	: public base_message
	, public hierarchy_ns::node_t< data_message_one, base_message >
	{
		data_message_one()
			: hierarchy_ns::node_t< data_message_one, base_message >( *this )
			{}
	};

class a_receiver_t final : public so_5::agent_t
	{
		hierarchy_ns::consumer_t< base_message > m_consumer;

		std::string & m_trace;

		const so_5::mbox_t m_sending_mbox;
	public:
		a_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			std::string & trace )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_trace{ trace }
			, m_sending_mbox{ demuxer.sending_mbox() }
			{}

		void
		so_evt_start() override
			{
				try
					{
						so_5::send< so_5::mutable_msg< data_message_one > >( m_sending_mbox );

						m_trace = "NO EXCEPTION";
					}
				catch( const so_5::exception_t & x )
					{
						std::cerr << "Exception: " << x.what() << std::endl;
						if( so_5::rc_mutable_msg_cannot_be_delivered_via_mpmc_mbox
								== x.error_code() )
							m_trace = "OK";
						else
							m_trace = "Caught:" + std::to_string( x.error_code() );
					}

				so_deregister_agent_coop_normally();
			}
	};

} /* namespace test */

using namespace test;

TEST_CASE( "mpmc_mutable_msg" )
{
	bool completed{ false };
	std::string trace;

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop( [&trace](so_5::coop_t & coop) {
								hierarchy_ns::demuxer_t< base_message > demuxer{
										coop.environment(),
										hierarchy_ns::multi_consumer
									};
								coop.make_agent<a_receiver_t>( demuxer, trace );
							} );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
	REQUIRE( "OK" == trace );
}

