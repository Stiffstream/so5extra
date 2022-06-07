/*
 * Simple example of using composite mbox.
 */

#include <so_5_extra/mboxes/composite.hpp>

#include <so_5/all.hpp>

#include <sstream>

// Short aliases for necessary namespaces from so5extra.
namespace composite_ns = so_5::extra::mboxes::composite;

using namespace std::chrono_literals;

// Signals to be used for status distribution.
struct msg_acquisition_started final : public so_5::signal_t {};
struct msg_acquisition_finished final : public so_5::signal_t {};

// Message to be used for data distribution.
struct msg_acquired_data final : public so_5::message_t
{
	const std::string m_data;

	explicit msg_acquired_data( std::string data )
		:	m_data{ std::move(data) }
	{}
};

// Agent that listens for statuses.
class status_listener final : public so_5::agent_t
{
public:
	// There is no need to have own constructor.
	using so_5::agent_t::agent_t;

	void so_define_agent() override
	{
		// Note: agent receives signals from own direct mbox.
		so_subscribe_self()
			.event( [](mhood_t<msg_acquisition_started>) {
					std::cout << "acquisition: started" << std::endl;
				} )
			.event( [](mhood_t<msg_acquisition_finished>) {
					std::cout << "acquisition: finished" << std::endl;
				} )
			;
	}
};

// Agent that consumes data.
class data_consumer final : public so_5::agent_t
{
public:
	// There is no need to have own constructor.
	using so_5::agent_t::agent_t;

	void so_define_agent() override
	{
		// Note: agent receives message from own direct mbox.
		so_subscribe_self().event( &data_consumer::evt_data );
	}

private:
	void evt_data( mhood_t<msg_acquired_data> cmd )
	{
		std::cout << "data received: '" << cmd->m_data << "'" << std::endl;
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

	// Mbox for outgoing messages.
	const so_5::mbox_t m_data_mbox;

	// Timer to be used for periodic msg_acquire signals.
	so_5::timer_id_t m_acquisition_timer;

	// Counters to identify every portion of data.
	int m_data_index{};

public:
	data_producer( context_t ctx, so_5::mbox_t data_mbox )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_data_mbox{ std::move(data_mbox) }
	{}

	void so_define_agent() override
	{
		so_subscribe_self()
			.event( &data_producer::evt_acquire )
			;
	}

	void so_evt_start() override
	{
		// Initiate periodic signal for data producing.
		m_acquisition_timer = so_5::send_periodic< msg_acquire >(
				*this,
				// No initial delay.
				0ms,
				// Acquisition period.
				100ms );
	}

private:
	void evt_acquire( mhood_t<msg_acquire> )
	{
		so_5::send< msg_acquisition_started >( m_data_mbox );

		std::ostringstream data_ss;
		data_ss << "index:" << m_data_index;
		++m_data_index;
		std::this_thread::sleep_for( 10ms );

		so_5::send< msg_acquired_data >( m_data_mbox, data_ss.str() );

		so_5::send< msg_acquisition_finished >( m_data_mbox );
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
			env.introduce_coop( []( so_5::coop_t & coop ) {
					// Create consumers of acquisition-related information.
					auto * listener = coop.make_agent< status_listener >();
					auto * consumer = coop.make_agent< data_consumer >();

					// Make a composite mbox for data distribution.
					auto data_mbox = composite_ns::single_consumer_builder(
								composite_ns::throw_if_not_found() )
							.add< msg_acquisition_started >( listener->so_direct_mbox() )
							.add< msg_acquisition_finished >( listener->so_direct_mbox() )
							.add< msg_acquired_data >( consumer->so_direct_mbox() )
							.make( coop.environment() );

					// Now we can create data_producer.
					// Let it be an active object.
					coop.make_agent_with_binder< data_producer >(
							so_5::disp::active_obj::make_dispatcher(
									coop.environment() ).binder(),
							data_mbox );
				} );

			std::this_thread::sleep_for( 500ms );
			env.stop();
		} );
}

