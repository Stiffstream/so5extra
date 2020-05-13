#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/disp/asio_one_thread/pub.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

using tid_set_t = std::set< so_5::current_thread_id_t >;

class arbiter_t final : public so_5::agent_t
{
public :
	struct finished final : public so_5::message_t
	{
		tid_set_t m_tids;

		finished( tid_set_t tids ) : m_tids( std::move(tids) ) {}
	};

	arbiter_t(
		context_t ctx,
		so_5::outliving_reference_t<tid_set_t> result_set,
		std::size_t ring_size )
		:	so_5::agent_t( std::move(ctx) )
		,	m_result_set( result_set )
		,	m_ring_size( ring_size )
	{
		so_subscribe( so_environment().create_mbox( "arbiter" ) )
			.event( &arbiter_t::on_finished );
	}

private :
	so_5::outliving_reference_t< tid_set_t > m_result_set;
	const std::size_t m_ring_size;
	std::size_t m_finished_count{ 0u };

	void
	on_finished( mhood_t<finished> cmd )
	{
		m_result_set.get().insert( cmd->m_tids.begin(), cmd->m_tids.end() );
		++m_finished_count;

		if( m_finished_count == m_ring_size )
			so_deregister_agent_coop_normally();
	}
};

class ring_member_t final : public so_5::agent_t
{
	state_t st_finished{ this };

public :
	struct your_turn final : public so_5::signal_t {};

	ring_member_t( context_t ctx ) : so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self().event( &ring_member_t::on_your_turn );
	}

	void
	set_next( so_5::mbox_t next )
	{
		m_next = std::move(next);
	}

private :
	std::size_t m_turns_passed{ 0u };
	tid_set_t m_tids;

	so_5::mbox_t m_next;

	void
	on_your_turn( mhood_t<your_turn> )
	{
		m_tids.insert( so_5::query_current_thread_id() );

		++m_turns_passed;
		so_5::send< your_turn >( m_next );

		if( m_turns_passed == 50 )
		{
			so_5::send< arbiter_t::finished >(
					so_environment().create_mbox( "arbiter" ),
					std::move(m_tids) );

			this >>= st_finished;
		}
	}
};

so_5::mbox_t
make_ring_coop(
	tid_set_t & result_set,
	so_5::coop_t & coop,
	so_5::extra::disp::asio_one_thread::dispatcher_handle_t disp )
{
	constexpr std::size_t ring_size = 25;

	coop.make_agent_with_binder< arbiter_t >(
			disp.binder(),
			so_5::outliving_mutable(result_set),
			ring_size );

	std::array< ring_member_t *, ring_size > ring_agents;
	for( std::size_t i = 0u; i != ring_size; ++i )
	{
		ring_agents[ i ] = coop.make_agent_with_binder< ring_member_t >(
				disp.binder() );
	}

	for( std::size_t i = 0u; i != ring_size; ++i )
	{
		ring_agents[ i ]->set_next(
				ring_agents[ (i+1) % ring_size ]->so_direct_mbox() );
	}

	return ring_agents.front()->so_direct_mbox();
}

TEST_CASE( "agent ring on asio_one_thread disp (external io_context)" )
{
	run_with_time_limit( [] {
			tid_set_t result_set;

			asio::io_context io_svc;
			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_ot = so_5::extra::disp::asio_one_thread;

						asio_ot::disp_params_t params;
						params.use_external_io_context( io_svc );

						auto disp = asio_ot::make_dispatcher(
								env,
								"asio_ot",
								std::move(params) );

						so_5::mbox_t first_mbox;
						env.introduce_coop(
								[&]( so_5::coop_t & coop )
								{
									first_mbox = make_ring_coop(
											result_set,
											coop,
											disp );
								} );

						so_5::send< ring_member_t::your_turn >( first_mbox );
					} );

			REQUIRE( 1u == result_set.size() );

			std::cout << "TIDs: " << std::flush;
			for( const auto & t : result_set )
				std::cout << t << " ";
			std::cout << std::endl;
		},
		5 );
}

TEST_CASE( "agent ring on asio_one_thread disp (own io_context)" )
{
	run_with_time_limit( [] {
			tid_set_t result_set;

			so_5::launch( [&](so_5::environment_t & env) {
						namespace asio_ot = so_5::extra::disp::asio_one_thread;

						asio_ot::disp_params_t params;
						params.use_own_io_context();

						auto disp = asio_ot::make_dispatcher(
								env,
								"asio_ot",
								std::move(params) );

						so_5::mbox_t first_mbox;
						env.introduce_coop(
								[&]( so_5::coop_t & coop )
								{
									first_mbox = make_ring_coop(
											result_set,
											coop,
											disp );
								} );

						so_5::send< ring_member_t::your_turn >( first_mbox );
					} );

			REQUIRE( 1u == result_set.size() );

			std::cout << "TIDs: " << std::flush;
			for( const auto & t : result_set )
				std::cout << t << " ";
			std::cout << std::endl;
		},
		5 );
}

