/*
 * Simple example of using first_last_subscriber_notification mbox.
 */

#include <so_5_extra/mboxes/first_last_subscriber_notification.hpp>

#include <so_5_extra/revocable_timer/pub.hpp>

#include <so_5/all.hpp>

#include <sstream>

namespace notifications_ns = so_5::extra::mboxes::first_last_subscriber_notification;
namespace timer_ns = so_5::extra::revocable_timer;

using namespace std::chrono_literals;

struct msg_acquired_data final : public so_5::message_t
{
	const std::string m_data;

	explicit msg_acquired_data( std::string data )
		:	m_data{ std::move(data) }
	{}
};

class data_consumer final : public so_5::agent_t
{
	struct msg_start final : public so_5::signal_t {};
	struct msg_finish final : public so_5::signal_t {};

	const std::string m_name;

	const so_5::mbox_t m_data_mbox;

	const std::chrono::milliseconds m_start_delay;
	const std::chrono::milliseconds m_work_duration;

public:
	data_consumer(
		context_t ctx,
		std::string name,
		so_5::mbox_t data_mbox,
		std::chrono::milliseconds start_delay,
		std::chrono::milliseconds work_duration )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_name{ std::move(name) }
		,	m_data_mbox{ std::move(data_mbox) }
		,	m_start_delay{ start_delay }
		,	m_work_duration{ work_duration }
	{}

	void so_define_agent() override
	{
		so_subscribe_self()
			.event( &data_consumer::evt_start )
			.event( &data_consumer::evt_finish )
			;
	}

	void so_evt_start() override
	{
		so_5::send_delayed< msg_start >( *this, m_start_delay );
	}

	void so_evt_finish() override
	{
		std::cout << "[" << m_name << "] work finished" << std::endl;
	}

private:
	void evt_start( mhood_t<msg_start> )
	{
		std::cout << "[" << m_name << "] work started" << std::endl;

		so_subscribe( m_data_mbox ).event( &data_consumer::evt_data );

		so_5::send_delayed< msg_finish >( *this, m_work_duration );
	}

	void evt_finish( mhood_t<msg_finish> )
	{
		so_deregister_agent_coop_normally();
	}

	void evt_data( mhood_t<msg_acquired_data> cmd )
	{
		std::cout << "[" << m_name << "] data received: '"
				<< cmd->m_data << "'" << std::endl;
	}
};

class data_producer final : public so_5::agent_t
{
	struct msg_acquire final : public so_5::signal_t {};

	state_t st_wait_consumers{ this, "wait_consumers" };
	state_t st_consumers_connected{ this, "consumers_connected" };

	const so_5::mbox_t m_data_mbox;

	timer_ns::timer_id_t m_acquisition_timer;

	int m_session{};
	int m_data_index{};

public:
	data_producer(
		context_t ctx )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_data_mbox{
				notifications_ns::make_mbox< msg_acquired_data >(
						so_environment(),
						so_direct_mbox(),
						so_5::mbox_type_t::multi_producer_multi_consumer )
			}
	{}

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
		m_acquisition_timer.revoke();

		std::cout << "*** data acquisition stopped ***" << std::endl;
	}

	void evt_first_consumer( mhood_t<notifications_ns::msg_first_subscriber> )
	{
		st_consumers_connected.activate();
	}

	void evt_last_consumer( mhood_t<notifications_ns::msg_last_subscriber> )
	{
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
			const so_5::mbox_t data_mbox = [&]() {
					auto agent = env.make_agent< data_producer >();
					so_5::mbox_t mbox = agent->data_mbox();
					env.register_agent_as_coop( std::move(agent) );
					return mbox;
				}();

			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"first",
							data_mbox,
							50ms,
							250ms ) );

			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"second",
							data_mbox,
							100ms,
							200ms ) );

			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"third",
							data_mbox,
							500ms,
							150ms ) );

			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"fourth",
							data_mbox,
							700ms,
							300ms ) );

			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"fifth",
							data_mbox,
							1200ms,
							300ms ) );

			env.register_agent_as_coop(
					env.make_agent< data_consumer >(
							"sixth",
							data_mbox,
							1300ms,
							300ms ) );

			std::this_thread::sleep_for( 1800ms );
			env.stop();
		} );
}

