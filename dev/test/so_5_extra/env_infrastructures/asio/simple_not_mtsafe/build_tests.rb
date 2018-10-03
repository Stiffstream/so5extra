#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/env_infrastructures/asio/simple_not_mtsafe'

	required_prj( "#{path}/empty_init_fn/prj.ut.rb" )
	required_prj( "#{path}/empty_init_fn/prj_s.ut.rb" )

	required_prj( "#{path}/simplest_agent/prj.ut.rb" )
	required_prj( "#{path}/simplest_agent/prj_s.ut.rb" )
	required_prj( "#{path}/simple_exec_order/prj.ut.rb" )
	required_prj( "#{path}/simple_exec_order/prj_s.ut.rb" )

	required_prj( "#{path}/agent_without_activity/prj.ut.rb" )
	required_prj( "#{path}/agent_without_activity/prj_s.ut.rb" )

	required_prj( "#{path}/simple_delayed_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_delayed_msg/prj_s.ut.rb" )
	required_prj( "#{path}/simple_periodic_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_periodic_msg/prj_s.ut.rb" )
	required_prj( "#{path}/cancel_delayed_msg/prj.ut.rb" )
	required_prj( "#{path}/cancel_delayed_msg/prj_s.ut.rb" )
	required_prj( "#{path}/cancel_periodic_msg/prj.ut.rb" )
	required_prj( "#{path}/cancel_periodic_msg/prj_s.ut.rb" )

	required_prj( "#{path}/stats_on/prj.ut.rb" )
	required_prj( "#{path}/stats_on/prj_s.ut.rb" )
	required_prj( "#{path}/stats_coop_count/prj.ut.rb" )
	required_prj( "#{path}/stats_coop_count/prj_s.ut.rb" )
	required_prj( "#{path}/stats_wt_activity/prj.ut.rb" )
	required_prj( "#{path}/stats_wt_activity/prj_s.ut.rb" )

	required_prj( "#{path}/stress_timers/prj.rb" )
}
