#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/mchains/fixed_size'

	required_prj( "#{path}/fibonacci/prj.rb" )
	required_prj( "#{path}/fibonacci/prj_s.rb" )
}
