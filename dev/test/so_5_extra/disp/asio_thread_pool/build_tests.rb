#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/disp/asio_thread_pool'

	required_prj( "#{path}/simplest_agent/prj.ut.rb" )
	required_prj( "#{path}/simplest_agent/prj_s.ut.rb" )

	required_prj( "#{path}/simplest_agent_2/prj.ut.rb" )
	required_prj( "#{path}/simplest_agent_2/prj_s.ut.rb" )

	required_prj( "#{path}/agent_ring/prj.ut.rb" )
	required_prj( "#{path}/agent_ring/prj_s.ut.rb" )

	required_prj( "#{path}/multifile_hello/prj.ut.rb" )
	required_prj( "#{path}/multifile_hello/prj_s.ut.rb" )

	required_prj( "#{path}/stats/prj.ut.rb" )
	required_prj( "#{path}/stats/prj_s.ut.rb" )

	required_prj( "#{path}/stats_wt_activity/prj.ut.rb" )
	required_prj( "#{path}/stats_wt_activity/prj_s.ut.rb" )

	required_prj( "#{path}/custom_thread/prj.ut.rb" )
	required_prj( "#{path}/custom_thread/prj_s.ut.rb" )
}

