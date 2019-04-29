#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN 
#include <doctest/doctest.h>

#include <so_5_extra/enveloped_msg/send_functions.hpp>
#include <so_5_extra/enveloped_msg/just_envelope.hpp>

#include <so_5/all.hpp>

#include <test/3rd_party/various_helpers/time_limited_execution.hpp>
#include <test/3rd_party/various_helpers/ensure.hpp>

#include <sstream>

namespace msg_ns = so_5::extra::enveloped_msg;

struct classical_message_t final : public so_5::message_t
	{
		int m_a;
		const char * m_b;

		classical_message_t( int a, const char * b )
			:	m_a{ a }, m_b{ b }
		{}
	};

struct user_message_t final
	{
		int m_a;
		const char * m_b;
	};

TEST_CASE( "send_to(mpmc_mbox)" )
{
	std::string trace;
	run_with_time_limit( [&] {
			so_5::wrapped_env_t sobj;
			auto mbox = sobj.environment().create_mbox();

			try
				{
					msg_ns::make< so_5::mutable_msg<classical_message_t> >( 1, "Hello!" )
							.envelope< msg_ns::just_envelope_t >()
							.send_to( mbox );
				}
			catch( const so_5::exception_t & x )
				{
					trace += "classical_message=" +
							std::to_string( x.error_code() ) + ";";
				}

			try
				{
					msg_ns::make< so_5::mutable_msg<user_message_t> >( 2, "Bye!" )
							.envelope< msg_ns::just_envelope_t >()
							.send_to( mbox );
				}
			catch( const so_5::exception_t & x )
				{
					trace += "user_message=" +
							std::to_string( x.error_code() ) + ";";
				}
		},
		5 );

	const std::string expected{
			"classical_message=172;"
			"user_message=172;"
	};

	REQUIRE( trace == expected );
}

