#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/sync'

	required_prj( "#{path}/simple/prj.rb" )
	required_prj( "#{path}/simple/prj_s.rb" )

	required_prj( "#{path}/mchains/prj.rb" )
	required_prj( "#{path}/mchains/prj_s.rb" )
}
