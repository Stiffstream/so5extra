#include <so_5_extra/env_infrastructures/asio/simple_not_mtsafe.hpp>

#include <so_5/all.hpp>

using clock_type = std::chrono::steady_clock;
using milliseconds = std::chrono::milliseconds;

class sender_A final : public so_5::agent_t
{
	milliseconds m_pause{ 20 };
	static const milliseconds delta;

	unsigned long m_received{ 0 };

	struct hello final : public so_5::signal_t {};

public :
	sender_A( context_t ctx ) : so_5::agent_t(std::move(ctx))
	{
		so_subscribe_self().event( &sender_A::on_hello );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send_delayed< hello >( *this, m_pause );
	}

private :
	void
	on_hello( mhood_t<hello> )
	{
		++m_received;
		if( 0ul == m_received % 1000ul )
			std::cout << "sender_A: " << m_received << std::endl;

		m_pause = (m_pause + delta) % 100;
		so_5::send_delayed< hello >( *this, m_pause );
	}
};
const milliseconds sender_A::delta{ 15 };

class sender_B final : public so_5::agent_t
{
	milliseconds m_pause{ 25 };
	static const milliseconds delta;
	static const milliseconds zero;

	so_5::timer_id_t m_timer;
	unsigned long m_received{ 0 };

	struct hello final : public so_5::signal_t {};

public :
	sender_B( context_t ctx ) : so_5::agent_t(std::move(ctx))
	{
		so_subscribe_self().event( &sender_B::on_hello );
	}

	virtual void
	so_evt_start() override
	{
		send_next();
	}

private :
	void
	on_hello( mhood_t<hello> )
	{
		++m_received;
		if( 0ul == m_received % 1000ul )
			std::cout << "sender_B: " << m_received << std::endl;

		m_pause = (m_pause + delta) % 100;
		send_next();
	}

	void
	send_next()
	{
		m_timer = so_5::send_periodic< hello >( *this, m_pause, zero );
	}
};

const milliseconds sender_B::delta{ 18 };
const milliseconds sender_B::zero{ milliseconds::zero() };

class sender_C final : public so_5::agent_t
{
	milliseconds m_pause{ 30 };
	static const milliseconds delta;

	so_5::timer_id_t m_timer;
	unsigned long m_received{ 0 };

	struct hello final : public so_5::signal_t {};

public :
	sender_C( context_t ctx ) : so_5::agent_t(std::move(ctx))
	{
		so_subscribe_self().event( &sender_C::on_hello );
	}

	virtual void
	so_evt_start() override
	{
		send_next();
	}

private :
	void
	on_hello( mhood_t<hello> )
	{
		++m_received;
		if( 0ul == m_received % 50ul )
		{
			if( 0ul == m_received % 1000ul )
				std::cout << "sender_C: " << m_received << std::endl;

			m_pause = (m_pause + delta) % 100;
			send_next();
		}
	}

	void
	send_next()
	{
		m_timer = so_5::send_periodic< hello >( *this, m_pause, delta );
	}
};
const milliseconds sender_C::delta{ 25 };

int main()
{
	asio::io_context io_svc;

	so_5::launch( [](so_5::environment_t & env) {
				env.register_agent_as_coop(
						env.make_agent< sender_A >() );

				env.register_agent_as_coop(
						env.make_agent< sender_B >() );

				env.register_agent_as_coop(
						env.make_agent< sender_C >() );
			},
			[&io_svc](so_5::environment_params_t & params) {
				using namespace so_5::extra::env_infrastructures::asio::simple_not_mtsafe;

				params.infrastructure_factory( factory(io_svc) );
			} );

	return 0;
}

