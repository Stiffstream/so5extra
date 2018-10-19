#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/revocable_timer'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_deliver/prj.ut.rb" )
	required_prj( "#{path}/simple_deliver/prj_s.ut.rb" )

	required_prj( "#{path}/resend_existing/prj.ut.rb" )
	required_prj( "#{path}/resend_existing/prj_s.ut.rb" )

	required_prj( "#{path}/mutable_periodic/prj.ut.rb" )
	required_prj( "#{path}/mutable_periodic/prj_s.ut.rb" )

	required_prj( "#{path}/resend_mutable_periodic/prj.ut.rb" )
	required_prj( "#{path}/resend_mutable_periodic/prj_s.ut.rb" )
}
