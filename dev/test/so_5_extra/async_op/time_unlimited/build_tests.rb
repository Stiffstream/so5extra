#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/async_op/time_unlimited'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_2/prj.ut.rb" )
	required_prj( "#{path}/simple_2/prj_s.ut.rb" )

	required_prj( "#{path}/simple_cancel/prj.ut.rb" )
	required_prj( "#{path}/simple_cancel/prj_s.ut.rb" )

	required_prj( "#{path}/several_completion/prj.ut.rb" )
	required_prj( "#{path}/several_completion/prj_s.ut.rb" )

	required_prj( "#{path}/exception_on_activation/prj.ut.rb" )
	required_prj( "#{path}/exception_on_activation/prj_s.ut.rb" )

	required_prj( "#{path}/exception_on_activation_2/prj.ut.rb" )
	required_prj( "#{path}/exception_on_activation_2/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_1/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_1/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_2/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_2/prj_s.ut.rb" )

	required_prj( "#{path}/ensure_destroyed_3/prj.ut.rb" )
	required_prj( "#{path}/ensure_destroyed_3/prj_s.ut.rb" )

}

