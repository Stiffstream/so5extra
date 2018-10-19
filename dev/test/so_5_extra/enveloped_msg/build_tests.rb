#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'test/so_5_extra/enveloped_msg'

	required_prj( "#{path}/simple_send/prj.ut.rb" )
	required_prj( "#{path}/simple_send/prj_s.ut.rb" )

	required_prj( "#{path}/send_mutable/prj.ut.rb" )
	required_prj( "#{path}/send_mutable/prj_s.ut.rb" )

	required_prj( "#{path}/time_limited_delivery/prj.ut.rb" )
	required_prj( "#{path}/time_limited_delivery/prj_s.ut.rb" )
}
