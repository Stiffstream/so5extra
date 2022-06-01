/*
 * Simple example of using first_last_subscriber_notification mbox.
 */

#include <so_5_extra/mboxes/first_last_subscriber_notification.hpp>

#include <so_5_extra/revocable_timer/pub.hpp>

#include <so_5/all.hpp>

#include <sstream>

// Short aliases for necessary namespaces from so5extra.
namespace notifications_ns = so_5::extra::mboxes::first_last_subscriber_notification;
namespace timer_ns = so_5::extra::revocable_timer;

using namespace std::chrono_literals;

// Message to be used for data distribution.
struct msg_acquired_data final : public so_5::message_t
{
	const std::string m_data;

	explicit msg_acquired_data( std::string data )
		:	m_data{ std::move(data) }
	{}
};

// Agent that consumes data.
class data_consumer final : public so_5::agent_t
{
	// Signal to be used to deregister the consumer.
	struct msg_done final : public so_5::signal_t {};

	// Name to be use by the consumer.
	const std::string m_name;

	// Mbox for msg_acquired_data messages.
	const so_5::mbox_t m_data_mbox;

	// How long the consumer should work.
	const std::chrono::milliseconds m_work_duration;

public:
	data_consumer(
		context_t ctx,
		std::string name,
		so_5::mbox_t data_mbox,
		std::chrono::milliseconds work_duration )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_name{ std::move(name) }
		,	m_data_mbox{ std::move(data_mbox) }
		,	m_work_duration{ work_duration }
	{}

	void so_define_agent() override
	{
		so_subscribe_self().event( &data_consumer::evt_done );

		so_subscribe( m_data_mbox ).event( &data_consumer::evt_data );
	}

	void so_evt_start() override
	{
		std::cout << "[" << m_name << "] work started" << std::endl;

		// Limit the lifetime of the agent.
		so_5::send_delayed< msg_done >( *this, m_work_duration );
	}

	void so_evt_finish() override
	{
		std::cout << "[" << m_name << "] work finished" << std::endl;
	}

private:
	void evt_done( mhood_t<msg_done> )
	{
		// The agent has to be deregistered.
		// Subscription to msg_acquired_data from m_data_mbox will be
		// removed automatically.
		so_deregister_agent_coop_normally();
	}

	void evt_data( mhood_t<msg_acquired_data> cmd )
	{
		std::cout << "[" << m_name << "] data received: '"
				<< cmd->m_data << "'" << std::endl;
	}
};

// Producer that produces data, but only when there is a subscriber.
//
// Also creates and owns the mbox for msg_acquired_data messages.
class data_producer final : public so_5::agent_t
{
	// Periodic signal to be used for data producing.
	// This signal will be generated only when
	// the agent is in st_consumers_connected state.
	struct msg_acquire final : public so_5::signal_t {};

	// State in that the producer does nothing.
	state_t st_wait_consumers{ this, "wait_consumers" };
	// State in that the producer periodically generates data.
	state_t st_consumers_connected{ this, "consumers_connected" };

	// Mbox for msg_acquired_data messages.
	const so_5::mbox_t m_data_mbox;

	// Timer to be used for periodic msg_acquire signals.
	timer_ns::timer_id_t m_acquisition_timer;

	// Counters to identify every portion of data.
	int m_session{};
	int m_data_index{};

public:
	data_producer( context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_data_mbox{
				// Make mbox for msg_acquired_data.
				notifications_ns::make_mbox< msg_acquired_data >(
						so_environment(),
						// Use the direct mbox for first/last subscriber notifications.
						so_direct_mbox(),
						// It should be a MPMC mbox.
						so_5::mbox_type_t::multi_producer_multi_consumer )
			}
	{}

	// Getter for msg_acquired_data's mbox.
	[[nodiscard]]
	const so_5::mbox_t &
	data_mbox() const noexcept
	{
		return m_data_mbox;
	}

	void so_define_agent() override
	{
		st_consumers_connected
			.on_enter( &data_producer::on_enter_st_consumers_connected )
			.on_exit( &data_producer::on_exit_st_consumers_connected )
			.event( &data_producer::evt_last_consumer )
			.event( &data_producer::evt_acquire )
			;

		st_wait_consumers
			.event( &data_producer::evt_first_consumer )
			;

		st_wait_consumers.activate();
	}

private:
	void on_enter_st_consumers_connected()
	{
		// Initiate periodic signal for data producing.
		m_acquisition_timer = timer_ns::send_periodic< msg_acquire >(
				*this,
				// No initial delay.
				0ms,
				// Acquisition period.
				100ms );

		++m_session;
		m_data_index = 0;

		std::cout << "*** data acquisition started ***" << std::endl;
	}

	void on_exit_st_consumers_connected()
	{
		// Timer for periodic signals has to be stopped.
		m_acquisition_timer.revoke();

		std::cout << "*** data acquisition stopped ***" << std::endl;
	}

	void evt_first_consumer( mhood_t<notifications_ns::msg_first_subscriber> )
	{
		// Because we have at least one consumer we should switch
		// to the appropriate state and start producing data.
		st_consumers_connected.activate();
	}

	void evt_last_consumer( mhood_t<notifications_ns::msg_last_subscriber> )
	{
		// There are no more consumers. We have to wait new consumers
		// in a different state.
		st_wait_consumers.activate();
	}

	void evt_acquire( mhood_t<msg_acquire> )
	{
		std::ostringstream data_ss;
		data_ss << "session:" << m_session << ";index:" << m_data_index;
		++m_data_index;

		so_5::send< msg_acquired_data >( m_data_mbox, data_ss.str() );
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
			// Create data_producer instance and get data-mbox from it.
			const so_5::mbox_t data_mbox = [&]() {
					auto agent = env.make_agent< data_producer >();
					so_5::mbox_t mbox = agent->data_mbox();
					env.register_agent_as_coop( std::move(agent) );
					return mbox;
				}();

			// Now consumers have to be introduced.
			std::this_thread::sleep_for( 50ms );
			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"first",
							data_mbox,
							250ms ) );

			std::this_thread::sleep_for( 50ms );
			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"second",
							data_mbox,
							200ms ) );

			std::this_thread::sleep_for( 400ms );
			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"third",
							data_mbox,
							150ms ) );

			std::this_thread::sleep_for( 300ms );
			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"fourth",
							data_mbox,
							300ms ) );

			std::this_thread::sleep_for( 400ms );
			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"fifth",
							data_mbox,
							300ms ) );

			std::this_thread::sleep_for( 100ms );
			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"sixth",
							data_mbox,
							300ms ) );

			std::this_thread::sleep_for( 200ms );
			env.stop();
		} );
}

