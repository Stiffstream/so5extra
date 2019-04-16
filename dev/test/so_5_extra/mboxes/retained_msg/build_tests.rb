#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/mboxes/retained_msg'

	required_prj( "#{path}/simple/prj.ut.rb" )
	required_prj( "#{path}/simple/prj_s.ut.rb" )

	required_prj( "#{path}/simple_enveloped_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_enveloped_msg/prj_s.ut.rb" )

	required_prj( "#{path}/simple_single_threaded/prj.ut.rb" )
	required_prj( "#{path}/simple_single_threaded/prj_s.ut.rb" )

	required_prj( "#{path}/simple_several_msg/prj.ut.rb" )
	required_prj( "#{path}/simple_several_msg/prj_s.ut.rb" )

	required_prj( "#{path}/delivery_filter/prj.ut.rb" )
	required_prj( "#{path}/delivery_filter/prj_s.ut.rb" )

	required_prj( "#{path}/mutable_msg/prj.ut.rb" )
	required_prj( "#{path}/mutable_msg/prj_s.ut.rb" )

	required_prj( "#{path}/mutable_enveloped_msg/prj.ut.rb" )
	required_prj( "#{path}/mutable_enveloped_msg/prj_s.ut.rb" )
}
