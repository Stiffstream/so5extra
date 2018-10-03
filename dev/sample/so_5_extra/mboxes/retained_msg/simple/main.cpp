/*
 * Simple usage of retained mbox.
 */

#include <so_5_extra/mboxes/retained_msg.hpp>

#include <so_5/all.hpp>

#include <vector>

using namespace std::chrono;

// Type of container with mboxes.
using data_sources_container_t = std::vector< so_5::mbox_t >;

// A data producer. Produces next value every N ms.
class data_producer_t final : public so_5::agent_t
{
	// A periodic signal to distribute next value.
	struct send_next_t final : public so_5::signal_t {};

public :
	// Message with data inside.
	struct data_t final : public so_5::message_t
	{
		const std::string m_name;
		int m_value;

		data_t( std::string name, int value )
			:	m_name( std::move(name) )
			,	m_value( value )
		{}

		friend std::ostream & operator<<(std::ostream & to, const data_t & d)
		{
			return (to << "[" << d.m_name << "]=" << d.m_value);
		}
	};

	data_producer_t(
		// Environment to work in.
		context_t ctx,
		// New values generation pause.
		milliseconds pause,
		// Data distribution mbox.
		so_5::mbox_t distribution_mbox )
		:	so_5::agent_t( std::move(ctx) )
		,	m_name( "data_" + std::to_string(pause.count()) + "ms" )
		,	m_pause( pause )
		,	m_distribution_mbox( std::move(distribution_mbox) )
	{
		so_subscribe_self().event( &data_producer_t::on_send_next );
	}

	virtual void so_evt_start() override
	{
		m_timer = so_5::send_periodic<send_next_t>( *this, m_pause, m_pause );
	}

private :
	const std::string m_name;
	const milliseconds m_pause;
	const so_5::mbox_t m_distribution_mbox;

	so_5::timer_id_t m_timer;

	int m_value{ 0 };

	void on_send_next( mhood_t<send_next_t> )
	{
		std::cout << m_name << ": produce next value: " << m_value << std::endl;

		so_5::send< data_t >( m_distribution_mbox, m_name, m_value );
		++m_value;
	}
};

// A listener which will listen for data for some time.
class data_listener_t final : public so_5::agent_t
{
	// A signal to finis work of this agent.
	struct finish_t final : public so_5::signal_t {};

public :
	data_listener_t(
		// Environment to work in.
		context_t ctx,
		// Name of listener.
		std::string name,
		// Mboxes to listen.
		data_sources_container_t data_mboxes )
		:	so_5::agent_t( std::move(ctx) )
		,	m_name( std::move(name) )
		,	m_data_mboxes( std::move(data_mboxes) )
	{
		so_subscribe_self().event( &data_listener_t::on_finish );
	}

	virtual void so_evt_start() override
	{
		// A subscription to data distribution mbox must be done here.
		for( const auto & mb : m_data_mboxes )
			so_subscribe( mb ).event( &data_listener_t::on_data );

		// Limit lifetime of itself.
		so_5::send_delayed< finish_t >( *this, 2s );

		std::cout << "listener(" << m_name << ") started" << std::endl;
	}

	virtual void so_evt_finish() override
	{
		std::cout << "listener(" << m_name << ") finished" << std::endl;
	}

private :
	const std::string m_name;
	const data_sources_container_t m_data_mboxes;

	void on_finish( mhood_t<finish_t> )
	{
		so_deregister_agent_coop_normally();
	}

	void on_data( mhood_t<data_producer_t::data_t> cmd )
	{
		std::cout << "listener(" << m_name << ") data received: "
				<< *cmd << std::endl;
	}
};

// Agent for managing this example.
class example_manager_t final : public so_5::agent_t
{
	// A delayed message for creation of another listener.
	struct make_listener_t final : public so_5::message_t
	{
		const std::string m_name;

		make_listener_t( std::string name ) : m_name( std::move(name) ) {}
	};

	// A delayed message to finish example.
	struct finish_t final : public so_5::signal_t {};

public :
	example_manager_t(
		// Environment to work in.
		context_t ctx,
		// Data distribution mboxes.
		data_sources_container_t data_mboxes )
		:	so_5::agent_t( std::move(ctx) )
		,	m_data_mboxes( std::move(data_mboxes) )
	{
		so_subscribe_self()
			.event( &example_manager_t::on_make_listener )
			.event( &example_manager_t::on_finish );
	}

	virtual void so_evt_start() override
	{
		// Data producers must be created.
		so_5::introduce_child_coop( *this,
			[&]( so_5::coop_t & coop ) {
				auto current_pause = 215ms;
				for( const auto & mb : m_data_mboxes )
				{
					coop.make_agent< data_producer_t >( current_pause, mb );
					current_pause += 175ms;
				}
			} );

		// Some data_listeners must be created later.
		so_5::send_delayed< make_listener_t >( *this,
				500ms,
				"first" );
		so_5::send_delayed< make_listener_t >( *this,
				1000ms,
				"second" );
		so_5::send_delayed< make_listener_t >( *this,
				1500ms,
				"third" );

		// Limit lifetime of itself.
		so_5::send_delayed< finish_t >( *this, 4s );
	}

private :
	const data_sources_container_t m_data_mboxes;

	void on_make_listener( mhood_t<make_listener_t> cmd )
	{
		so_5::introduce_child_coop( *this,
			[&]( so_5::coop_t & coop ) {
				coop.make_agent< data_listener_t >( cmd->m_name, m_data_mboxes );
			} );
	}

	void on_finish( mhood_t<finish_t> )
	{
		so_deregister_agent_coop_normally();
	}
};

int main()
{
	so_5::launch( []( so_5::environment_t & env ) {
		// Create main example coop.
		env.introduce_coop( [&]( so_5::coop_t & coop ) {
			// Create retained_msg mboxes.
			data_sources_container_t mboxes{
				so_5::extra::mboxes::retained_msg::make_mbox<>( env ),
				so_5::extra::mboxes::retained_msg::make_mbox<>( env ),
				so_5::extra::mboxes::retained_msg::make_mbox<>( env ) };
					
			// Move retained_msg mboxes to the example manager.
			coop.make_agent< example_manager_t >( std::move(mboxes) );
		} );
	} );

	return 0;
}

