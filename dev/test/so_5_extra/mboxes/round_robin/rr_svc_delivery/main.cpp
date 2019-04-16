#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>

struct request final : public so_5::signal_t {};

class a_handler_t final : public so_5::agent_t
{
public :
	a_handler_t(
		context_t ctx,
		unsigned index,
		const so_5::mbox_t & rrmbox )
		:	so_5::agent_t( std::move(ctx) )
	{
		so_subscribe( rrmbox ).event( [index](mhood_t<request>) -> unsigned int {
			return index;
		} );
	}
};

class a_test_case_t final : public so_5::agent_t
{
public :
	a_test_case_t(
		context_t ctx,
		std::string & dest )
		:	so_5::agent_t( std::move(ctx) )
		,	m_rrmbox( so_5::extra::mboxes::round_robin::make_mbox<>(
					so_environment() ) )
		,	m_dest( dest )
		,	m_replies{ {0, 0, 0} }
	{}

	virtual void
	so_evt_start() override
	{
		so_5::introduce_child_coop( *this,
				so_5::disp::one_thread::create_private_disp(
						so_environment() )->binder(),
				[this]( so_5::coop_t & coop ) {
					for( std::size_t index = 0; index != m_replies.size(); ++index )
					{
						coop.make_agent< a_handler_t >(
								static_cast<unsigned>(index), m_rrmbox );
					}
				} );

		for( int i = 0; i < 3; ++i )
			for( std::size_t index = 0; index != m_replies.size(); ++index )
			{
				const auto reply = so_5::request_value< unsigned, request >(
						m_rrmbox,
						so_5::infinite_wait );

				m_replies[ reply ] += 1;
			}

		so_deregister_agent_coop_normally();
	}

	virtual void
	so_evt_finish() override
	{
		std::ostringstream ss;
		for( std::size_t index = 0; index != m_replies.size(); ++index )
			ss << index << "=" << m_replies[ index ] << ";";

		m_dest = ss.str();
	}

private :
	const so_5::mbox_t m_rrmbox;
	std::string & m_dest;

	std::array<int, 3> m_replies;
};

TEST_CASE( "simple message delivery on rrmbox" )
{
	run_with_time_limit( [] {
			std::string scenario;

			so_5::launch( [&](so_5::environment_t & env) {
						env.introduce_coop(
								[&]( so_5::coop_t & coop ) {
									coop.make_agent< a_test_case_t >(
											std::ref(scenario) );
								} );
					} );

			REQUIRE( scenario == "0=3;1=3;2=3;" );
		},
		5 );
}

