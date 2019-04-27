/*
 * A very simple usage of so_5::extra::sync::request_reply_t.
 */

#include <so_5_extra/sync/pub.hpp>

#include <so_5/all.hpp>

// Short alias for convenience.
namespace sync_ns = so_5::extra::sync;

using namespace std::chrono_literals;

// The type of service provider.
class service_provider_t final : public so_5::agent_t
{
public :
	using so_5::agent_t::agent_t;

	void so_define_agent() override
	{
		so_subscribe_self().event(
				[]( sync_ns::request_mhood_t<int, std::string> cmd ) {
					// Transform the incoming value, convert the result
					// to string and send the resulting string back.
					cmd->make_reply( std::to_string(cmd->request() * 2) );
				} );
	}
};

// The type of service consumer.
class consumer_t final : public so_5::agent_t
{
	// Message box of the service provider.
	const so_5::mbox_t m_service;

public :
	consumer_t( context_t ctx, so_5::mbox_t service )
		:	so_5::agent_t{ std::move(ctx) }
		,	m_service{ std::move(service) }
	{}

	void so_evt_start() override
	{
		// Issue a request and wait for the result no more than 500ms.
		auto result = sync_ns::request_reply<int, std::string>(
				// The destination for the request.
				m_service,
				// Max waiting time.
				500ms,
				// Request's value.
				4 );

		std::cout << "The result: " << result << std::endl;

		so_deregister_agent_coop_normally();
	}
};

int main()
{
	so_5::launch( [](so_5::environment_t & env) {
		env.introduce_coop(
			// Every agent should work on its own thread.
			so_5::disp::active_obj::make_dispatcher( env ).binder(),
			[](so_5::coop_t & coop) {
				auto service_mbox = coop.make_agent< service_provider_t >()
						->so_direct_mbox();
				coop.make_agent< consumer_t >( service_mbox );
			} );
	} );
}

