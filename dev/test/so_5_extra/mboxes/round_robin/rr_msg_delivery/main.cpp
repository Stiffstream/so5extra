#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/mboxes/round_robin.hpp>

#include <so_5/all.hpp>

#include <various_helpers_1/time_limited_execution.hpp>

struct request final : public so_5::message_t
{
	const so_5::mbox_t m_reply_to;

	request( so_5::mbox_t reply_to) : m_reply_to(std::move(reply_to)) {}
};

struct reply final : public so_5::message_t
{
	unsigned int m_index;

	reply( unsigned int index ) : m_index(index) {}
};

class a_handler_t final : public so_5::agent_t
{
public :
	a_handler_t(
		context_t ctx,
		unsigned index,
		const so_5::mbox_t & rrmbox )
		:	so_5::agent_t( std::move(ctx) )
	{
		so_subscribe( rrmbox ).event( [index](mhood_t<request> cmd) {
			so_5::send< reply >( cmd->m_reply_to, index );
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
		,	m_replies{ { 0, 0, 0 } }
	{
		so_subscribe_self().event( &a_test_case_t::on_reply );
	}

	virtual void
	so_evt_start() override
	{
		so_5::introduce_child_coop( *this,
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
				so_5::send< request >( m_rrmbox, so_direct_mbox() );
				++m_messages_sent;
			}
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

	unsigned int m_messages_sent = 0;
	unsigned int m_messages_received = 0;

	void
	on_reply( mhood_t<reply> cmd )
	{
		m_replies[ cmd->m_index ] += 1;
		++m_messages_received;

		if( m_messages_sent == m_messages_received )
			so_deregister_agent_coop_normally();
	}
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

