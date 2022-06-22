/*
 * Simple example for inflight_limit_mbox.
 */

#include <so_5_extra/mboxes/inflight_limit.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

// Message to be processed.
struct msg_do_something final : public so_5::message_t
{
	std::string m_id;

	msg_do_something( std::string id ) : m_id{ std::move(id) } {}
};

// Agent that perform message processing.
// This agent will be bound to adv_thread_pool dispatcher.
class processor final : public so_5::agent_t
{
	// This is inflight_limit_mbox connected with the direct mbox.
	const so_5::mbox_t m_dest_mbox;

public:
	processor( context_t ctx, unsigned int messages_limit )
		:	so_5::agent_t{ std::move(ctx) }
		// inflight_limit_mbox has to be created.
		,	m_dest_mbox{ so_5::extra::mboxes::inflight_limit::make_mbox< msg_do_something >(
				// Bound it to the direct mbox.
				so_direct_mbox(),
				// Set the limit.
				messages_limit )
			}
	{}

	// Access to inflight_limit_mbox.
	[[nodiscard]]
	const so_5::mbox_t & dest_mbox() const noexcept { return m_dest_mbox; }

	void so_define_agent() override
	{
		// We use direct_mbox for subscription.
		// But m_dest_mbox can also be used.
		so_subscribe_self()
			.event( &processor::evt_do_something, so_5::thread_safe );
	}

private:
	void evt_do_something( mhood_t<msg_do_something> cmd )
	{
		std::cout << "*** [" << cmd->m_id << "] processing started" << std::endl;

		std::this_thread::sleep_for( 25ms );

		std::cout << "*** [" << cmd->m_id << "] processing finished" << std::endl;
	}
};

// Agent that generates messages.
class generator final : public so_5::agent_t
{
	struct msg_generate_next final : public so_5::signal_t {};

	// Name of that agent. Will be a part of message ID.
	const std::string m_name;

	// The destination for messages.
	const so_5::mbox_t m_dest_mbox;

	// Initial delay for messages from that agent.
	const std::chrono::milliseconds m_initial_delay;

	// Counter to be used for message IDs.
	unsigned int m_ordinal{};

public:
	generator(
		context_t ctx,
		std::string name,
		so_5::mbox_t dest_mbox,
		std::chrono::milliseconds initial_delay )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_name{ std::move(name) }
		,	m_dest_mbox{ std::move(dest_mbox) }
		,	m_initial_delay{ initial_delay }
	{}

	void so_define_agent() override
	{
		so_subscribe_self()
			.event( &generator::evt_generate_next );
	}

	void so_evt_start() override
	{
		so_5::send_delayed< msg_generate_next >( *this, m_initial_delay );
	}

private:
	void evt_generate_next( mhood_t<msg_generate_next> )
	{
		++m_ordinal;
		so_5::send< msg_do_something >( m_dest_mbox,
				m_name + "-" + std::to_string( m_ordinal ) );

		so_5::send_delayed< msg_generate_next >( *this, 15ms );
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
				env.introduce_coop( []( so_5::coop_t & coop ) {
						// Create process first because we need to get mbox from it.
						auto mbox = coop.make_agent_with_binder< processor >(
								so_5::disp::adv_thread_pool::make_dispatcher(
										coop.environment(),
										4u /* worker threads */ ).binder(),
								4u /* message limit */ )->dest_mbox();

						// Create generators.
						constexpr std::size_t generators_count = 4u;
						std::string names[ generators_count ] =
								{ "alice", "bob", "eve", "kate" };
						std::chrono::milliseconds initial_delays[ generators_count ] =
								{ 7ms, 0ms, 17ms, 23ms };

						for( std::size_t i{}; i != generators_count; ++i )
							coop.make_agent< generator >(
									names[ i ],
									mbox,
									initial_delays[ i ] );
					} );

				// Limit execution time.
				std::this_thread::sleep_for( 250ms );
				env.stop();
			} );
}

