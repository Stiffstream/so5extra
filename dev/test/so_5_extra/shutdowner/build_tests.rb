#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/shutdowner'

	required_prj( "#{path}/empty_env/prj.ut.rb" )
	required_prj( "#{path}/empty_env/prj_s.ut.rb" )

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/one_subscriber/prj.ut.rb" )
	required_prj( "#{path}/one_subscriber/prj_s.ut.rb" )

	required_prj( "#{path}/one_subscriber_stenv/prj.ut.rb" )
	required_prj( "#{path}/one_subscriber_stenv/prj_s.ut.rb" )

	required_prj( "#{path}/many_subscribers/prj.ut.rb" )
	required_prj( "#{path}/many_subscribers/prj_s.ut.rb" )

	required_prj( "#{path}/many_subscribers_stenv/prj.ut.rb" )
	required_prj( "#{path}/many_subscribers_stenv/prj_s.ut.rb" )

	required_prj( "#{path}/subscribe_when_shutdown/prj.ut.rb" )
	required_prj( "#{path}/subscribe_when_shutdown/prj_s.ut.rb" )

	required_prj( "#{path}/time_elapsed/prj.ut.rb" )
	required_prj( "#{path}/time_elapsed/prj_s.ut.rb" )
}
