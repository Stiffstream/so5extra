#!/usr/bin/ruby
require 'mxx_ru/cpp'

MxxRu::Cpp::composite_target {
	path = 'sample/so_5_extra/mboxes/round_robin'

	required_prj( "#{path}/simple/prj.rb" )
	required_prj( "#{path}/simple/prj_s.rb" )

	required_prj( "#{path}/advanced/prj.rb" )
	required_prj( "#{path}/advanced/prj_s.rb" )
}
