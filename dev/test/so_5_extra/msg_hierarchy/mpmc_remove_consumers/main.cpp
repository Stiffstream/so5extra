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

struct receiver_completed final : public so_5::signal_t {};

class a_sender_t final : public so_5::agent_t
	{
		const so_5::mbox_t m_sending_mbox;
		const unsigned m_portion_size;
		unsigned m_receivers;

	public:
		struct start_sending final : public so_5::signal_t {};

		a_sender_t(
			context_t ctx,
			so_5::mbox_t sending_mbox,
			unsigned portion_size,
			unsigned receivers_count )
			: so_5::agent_t{ std::move(ctx) }
			, m_sending_mbox{ std::move(sending_mbox) }
			, m_portion_size{ portion_size }
			, m_receivers{ receivers_count }
			{}

		void
		so_define_agent() override
			{
				so_subscribe_self()
					.event( &a_sender_t::evt_start_sending )
					.event( &a_sender_t::evt_receiver_completed )
					;
			}

	private:
		void
		evt_start_sending( mhood_t<start_sending> )
			{
				send_another_portion();
			}

		void
		evt_receiver_completed( mhood_t<receiver_completed> )
			{
				if( --m_receivers > 0 )
					send_another_portion();
				else
					so_deregister_agent_coop_normally();
			}

		void
		send_another_portion()
			{
				for( unsigned i = 0u; i != m_portion_size; ++i )
					so_5::send< base_message >( m_sending_mbox );
			}
	};

class a_receiver_t final : public so_5::agent_t
	{
		const state_t st_normal{ this, "normal" };
		const state_t st_deactivated{ this, "deactivated" };

		hierarchy_ns::consumer_t< base_message > m_consumer;

		const so_5::mbox_t m_sender_mbox;
		unsigned int m_messages_to_receive;

	public:
		a_receiver_t(
			context_t ctx,
			hierarchy_ns::demuxer_t< base_message > & demuxer,
			so_5::mbox_t sender_mbox,
			unsigned int messages_to_receive )
			: so_5::agent_t{ std::move(ctx) }
			, m_consumer{ demuxer.allocate_consumer() }
			, m_sender_mbox{ std::move(sender_mbox) }
			, m_messages_to_receive{ messages_to_receive }
			{}

		void
		so_define_agent() override
			{
				const auto receiving_mbox = m_consumer.receiving_mbox< base_message >();

				this >>= st_normal;

				st_normal.event( receiving_mbox, &a_receiver_t::on_message );
			}

	private:
		void
		on_message( mhood_t<base_message> )
			{
				if( --m_messages_to_receive == 0u )
					{
						so_5::send< receiver_completed >( m_sender_mbox );

						this >>= st_deactivated;
						so_deregister_agent_coop_normally();
					}
			}
	};

} /* namespace test */

using namespace test;

constexpr unsigned portion_size = 1000u;

TEST_CASE( "mpmc_remove_consumers" )
{
	bool completed{ false };

	run_with_time_limit( [&] {
			so_5::launch( [&](so_5::environment_t & env) {
						const auto sender_mbox = [&env]() -> so_5::mbox_t
							{
								hierarchy_ns::demuxer_t< base_message > demuxer{
										env,
										so_5::mbox_type_t::multi_producer_multi_consumer
									};

								// Every agent will work on a separate thread.
								auto result_sender_mbox = env.introduce_coop(
										so_5::disp::one_thread::make_dispatcher( env ).binder(),
										[&demuxer](so_5::coop_t & coop) {
											return coop.make_agent<a_sender_t>(
													demuxer.sending_mbox(),
													portion_size,
													4u )->so_direct_mbox();
										} );

								// receiver N1
								env.introduce_coop(
										so_5::disp::one_thread::make_dispatcher( env ).binder(),
										[&demuxer, &result_sender_mbox](so_5::coop_t & coop) {
											coop.make_agent< a_receiver_t >(
													demuxer,
													result_sender_mbox,
													portion_size );
										} );
								// receiver N2
								env.introduce_coop(
										so_5::disp::one_thread::make_dispatcher( env ).binder(),
										[&demuxer, &result_sender_mbox](so_5::coop_t & coop) {
											coop.make_agent< a_receiver_t >(
													demuxer,
													result_sender_mbox,
													portion_size * 2u );
										} );
								// receiver N3
								env.introduce_coop(
										so_5::disp::one_thread::make_dispatcher( env ).binder(),
										[&demuxer, &result_sender_mbox](so_5::coop_t & coop) {
											coop.make_agent< a_receiver_t >(
													demuxer,
													result_sender_mbox,
													portion_size * 3u );
										} );
								// receiver N4
								env.introduce_coop(
										so_5::disp::one_thread::make_dispatcher( env ).binder(),
										[&demuxer, &result_sender_mbox](so_5::coop_t & coop) {
											coop.make_agent< a_receiver_t >(
													demuxer,
													result_sender_mbox,
													portion_size * 4u );
										} );

								return result_sender_mbox;
							}();

						so_5::send< a_sender_t::start_sending >( sender_mbox );
					} );

			completed = true;
		},
		5 );

	REQUIRE( completed == true );
}

