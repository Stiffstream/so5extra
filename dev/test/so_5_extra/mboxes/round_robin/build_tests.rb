#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/round_robin'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_svc_request/prj.ut.rb" )
	required_prj( "#{path}/simple_svc_request/prj_s.ut.rb" )

	required_prj( "#{path}/rr_msg_delivery/prj.ut.rb" )
	required_prj( "#{path}/rr_msg_delivery/prj_s.ut.rb" )

	required_prj( "#{path}/rr_svc_delivery/prj.ut.rb" )
	required_prj( "#{path}/rr_svc_delivery/prj_s.ut.rb" )

	required_prj( "#{path}/msg_limits/prj.ut.rb" )
	required_prj( "#{path}/msg_limits/prj_s.ut.rb" )
}
