#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/enveloped_msg'

	required_prj( "#{path}/delivery_receipt/prj.rb" )
	required_prj( "#{path}/delivery_receipt/prj_s.rb" )

	required_prj( "#{path}/time_limited_delivery/prj.rb" )
	required_prj( "#{path}/time_limited_delivery/prj_s.rb" )
}
