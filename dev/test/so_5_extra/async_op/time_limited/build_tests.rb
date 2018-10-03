#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/async_op/time_limited'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_timeout/prj.ut.rb" )
	required_prj( "#{path}/simple_timeout/prj_s.ut.rb" )

	required_prj( "#{path}/simple_cancel/prj.ut.rb" )
	required_prj( "#{path}/simple_cancel/prj_s.ut.rb" )

	required_prj( "#{path}/simple_no_user_default_timeout/prj.ut.rb" )
	required_prj( "#{path}/simple_no_user_default_timeout/prj_s.ut.rb" )

	required_prj( "#{path}/simple_default_timeout_handler/prj.ut.rb" )
	required_prj( "#{path}/simple_default_timeout_handler/prj_s.ut.rb" )

	required_prj( "#{path}/timeout_handler_type_mismatch/prj.ut.rb" )
	required_prj( "#{path}/timeout_handler_type_mismatch/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_1/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_1/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_2/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_2/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_3/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_3/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_4/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_4/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_5/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_5/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_6/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_6/prj_s.ut.rb" )

	required_prj( "#{path}/several_completion/prj.ut.rb" )
	required_prj( "#{path}/several_completion/prj_s.ut.rb" )

	required_prj( "#{path}/exception_on_activation/prj.ut.rb" )
	required_prj( "#{path}/exception_on_activation/prj_s.ut.rb" )

	required_prj( "#{path}/msg_limits/prj.ut.rb" )
	required_prj( "#{path}/msg_limits/prj_s.ut.rb" )
}
