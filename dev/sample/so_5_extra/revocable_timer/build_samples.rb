#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/revocable_timer'

	required_prj( "#{path}/simple/prj.rb" )
	required_prj( "#{path}/simple/prj_s.rb" )
}
