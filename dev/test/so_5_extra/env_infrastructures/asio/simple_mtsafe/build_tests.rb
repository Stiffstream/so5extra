#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/env_infrastructures/asio/simple_mtsafe'

	required_prj( "#{path}/empty_init_fn/prj.ut.rb" )
	required_prj( "#{path}/empty_init_fn/prj_s.ut.rb" )

	required_prj( "#{path}/simplest_agent/prj.ut.rb" )
	required_prj( "#{path}/simplest_agent/prj_s.ut.rb" )
	required_prj( "#{path}/simple_exec_order/prj.ut.rb" )
	required_prj( "#{path}/simple_exec_order/prj_s.ut.rb" )

	required_prj( "#{path}/unknown_exception_init_fn/prj.ut.rb" )
	required_prj( "#{path}/unknown_exception_init_fn/prj_s.ut.rb" )

	required_prj( "#{path}/unknown_exception_init_fn_2/prj.ut.rb" )
	required_prj( "#{path}/unknown_exception_init_fn_2/prj_s.ut.rb" )

	required_prj( "#{path}/unknown_exception_init_fn_3/prj.ut.rb" )
	required_prj( "#{path}/unknown_exception_init_fn_3/prj_s.ut.rb" )

	required_prj( "#{path}/agent_without_activity/prj.ut.rb" )
	required_prj( "#{path}/agent_without_activity/prj_s.ut.rb" )

	required_prj( "#{path}/autoshutdown_disabled/prj.ut.rb" )
	required_prj( "#{path}/autoshutdown_disabled/prj_s.ut.rb" )

	required_prj( "#{path}/simple_delayed_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_delayed_msg/prj_s.ut.rb" )
	required_prj( "#{path}/simple_periodic_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_periodic_msg/prj_s.ut.rb" )
	required_prj( "#{path}/cancel_delayed_msg/prj.ut.rb" )
	required_prj( "#{path}/cancel_delayed_msg/prj_s.ut.rb" )
	required_prj( "#{path}/cancel_periodic_msg/prj.ut.rb" )
	required_prj( "#{path}/cancel_periodic_msg/prj_s.ut.rb" )

	required_prj( "#{path}/delayed_msg_from_outside/prj.ut.rb" )
	required_prj( "#{path}/delayed_msg_from_outside/prj_s.ut.rb" )
	required_prj( "#{path}/delayed_msg_after_stop/prj.ut.rb" )
	required_prj( "#{path}/delayed_msg_after_stop/prj_s.ut.rb" )

	required_prj( "#{path}/periodic_msg_from_outside/prj.ut.rb" )
	required_prj( "#{path}/periodic_msg_from_outside/prj_s.ut.rb" )

	required_prj( "#{path}/stop_from_outside/prj.ut.rb" )
	required_prj( "#{path}/stop_from_outside/prj_s.ut.rb" )

	required_prj( "#{path}/ping_from_outside/prj.ut.rb" )
	required_prj( "#{path}/ping_from_outside/prj_s.ut.rb" )

	required_prj( "#{path}/coop_from_outside/prj.ut.rb" )
	required_prj( "#{path}/coop_from_outside/prj_s.ut.rb" )

	required_prj( "#{path}/stats_on/prj.ut.rb" )
	required_prj( "#{path}/stats_on/prj_s.ut.rb" )
	required_prj( "#{path}/stats_coop_count/prj.ut.rb" )
	required_prj( "#{path}/stats_coop_count/prj_s.ut.rb" )
	required_prj( "#{path}/stats_wt_activity/prj.ut.rb" )
	required_prj( "#{path}/stats_wt_activity/prj_s.ut.rb" )
}
