#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/retained_msg'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_single_threaded/prj.ut.rb" )
	required_prj( "#{path}/simple_single_threaded/prj_s.ut.rb" )

	required_prj( "#{path}/simple_several_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_several_msg/prj_s.ut.rb" )

	required_prj( "#{path}/delivery_filter/prj.ut.rb" )
	required_prj( "#{path}/delivery_filter/prj_s.ut.rb" )

	required_prj( "#{path}/service_request/prj.ut.rb" )
	required_prj( "#{path}/service_request/prj_s.ut.rb" )

	required_prj( "#{path}/service_request_enabled/prj.ut.rb" )
	required_prj( "#{path}/service_request_enabled/prj_s.ut.rb" )

	required_prj( "#{path}/service_request_enabled_2/prj.ut.rb" )
	required_prj( "#{path}/service_request_enabled_2/prj_s.ut.rb" )

	required_prj( "#{path}/service_request_enabled_3/prj.ut.rb" )
	required_prj( "#{path}/service_request_enabled_3/prj_s.ut.rb" )

	required_prj( "#{path}/mutable_msg/prj.ut.rb" )
	required_prj( "#{path}/mutable_msg/prj_s.ut.rb" )
}
